// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only

#include "threadeddatabase.h"

#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUrl>
#include <QStringBuilder>
#include <QVariant>
#include <QSqlResult>
#include <QSqlError>
#include <QLoggingCategory>

#define SCHAMA_MIGRATIONS_TABLE "__qt_schema_migrations"

Q_DECLARE_LOGGING_CATEGORY(asyncdatabase)
Q_LOGGING_CATEGORY(asyncdatabase, "asyncdatabase")

namespace asyncdatabase_private {

// migrations
void createInternalTable(QSqlDatabase &database) {
    QSqlQuery query(QStringLiteral("create table if not exists " SCHAMA_MIGRATIONS_TABLE " ("
                                        "version Text primary key not null, "
                                        "run_on timestamp not null default current_timestamp)"), database);
    if (!query.exec()) {
        printSqlError(query);
    }
}

void markMigrationRun(QSqlDatabase &database, const QString &name) {
    qCDebug(asyncdatabase) << "Marking migration" << name << "as done.";

    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral("insert into " SCHAMA_MIGRATIONS_TABLE " (version) values (:name)"))) {
        printSqlError(query);
    }
    query.bindValue(QStringLiteral(":name"), name);
    if (!query.exec()) {
        printSqlError(query);
    }
}

QString currentDatabaseVersion(QSqlDatabase &database) {
    QSqlQuery query(database);
    query.prepare(QStringLiteral("select version from " SCHAMA_MIGRATIONS_TABLE " order by version desc limit 1"));
    query.exec();

    query.next();
    return query.value(0).toString();
}

void runDatabaseMigrations(QSqlDatabase &database, const QString &migrationDirectory)
{
    createInternalTable(database);

    QDir dir(migrationDirectory);
    const auto entries = dir.entryList(QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot, QDir::SortFlag::Name);

    const QString currentVersion = currentDatabaseVersion(database);
    for (const auto &entry : entries) {
        QDir subdir(entry);
        if (subdir.dirName() > currentVersion) {
            QFile file(migrationDirectory % QDir::separator() % entry % QDir::separator() % "up.sql");
            if (!file.open(QFile::ReadOnly)) {
                qCDebug(asyncdatabase) << "Failed to open migration file" << file.fileName();
            }
            qCDebug(asyncdatabase) << "Running migration" << subdir.dirName();

            database.transaction();

            // Hackish
            const auto statements = file.readAll().split(';');

            bool migrationSuccessful = true;
            for (const QByteArray &statement : statements) {
                const auto trimmedStatement = statement.trimmed();
                QSqlQuery query(database);

                if (!trimmedStatement.isEmpty()) {
                    qCDebug(asyncdatabase) << "Running" << trimmedStatement;
                    if (!query.prepare(trimmedStatement)) {
                        printSqlError(query);
                    } else {
                        bool success = query.exec();
                        migrationSuccessful &= success;
                        if (!success) {
                            printSqlError(query);
                        }
                    }
                }
            }

            if (migrationSuccessful) {
                database.commit();
                markMigrationRun(database, subdir.dirName());
            } else {
                database.rollback();
            }
        }
    }
    qCDebug(asyncdatabase) << "Migrations finished";
}

struct AsyncSqlDatabasePrivate {
    QSqlDatabase database;
    std::unordered_map<QString, QSqlQuery> preparedQueryCache;
};

// Internal asynchronous database class
QFuture<void> AsyncSqlDatabase::establishConnection(const DatabaseConfiguration &configuration)
{
    return runAsync([=, this] {
        d->database = QSqlDatabase::addDatabase(configuration.type());
        if (configuration.databaseName()) {
            d->database.setDatabaseName(*configuration.databaseName());
        }
        if (configuration.hostName()) {
            d->database.setHostName(*configuration.hostName());
        }
        if (configuration.userName()) {
            d->database.setUserName(*configuration.userName());
        }
        if (configuration.password()) {
            d->database.setPassword(*configuration.password());
        }

        if (!d->database.open()) {
            qCDebug(asyncdatabase) << "Failed to open database" << d->database.lastError().text();
            if (configuration.databaseName()) {
                qCDebug(asyncdatabase) << "Tried to use database" << *configuration.databaseName();
            }
        }
    });
}

auto AsyncSqlDatabase::runMigrations(const QString &migrationDirectory) -> QFuture<void> {
    return runAsync([=, this] {
        runDatabaseMigrations(d->database, migrationDirectory);
    });
}

auto AsyncSqlDatabase::setCurrentMigrationLevel(const QString &migrationName) -> QFuture<void> {
    return runAsync([=, this] {
        createInternalTable(d->database);
        markMigrationRun(d->database, migrationName);
    });
}

AsyncSqlDatabase::AsyncSqlDatabase()
    : QObject()
    , d(std::make_unique<AsyncSqlDatabasePrivate>())
{
}

AsyncSqlDatabase::~AsyncSqlDatabase() {
    runAsync([db = d->database] {
        QSqlDatabase::removeDatabase(db.databaseName());
    });
};

Row AsyncSqlDatabase::retrieveRow(const QSqlQuery &query) {
    Row row;
    int i = 0;

    while (true) {
        if (query.isValid()) {
            QVariant value = query.value(i);
            if (value.isValid()) {
                row.push_back(std::move(value));
                i++;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    return row;
}

Rows AsyncSqlDatabase::retrieveRows(QSqlQuery &query)
{
    Rows rows;
    while (query.next()) {
        rows.push_back(retrieveRow(query));
    }

    return rows;
}

std::optional<Row> AsyncSqlDatabase::retrieveOptionalRow(QSqlQuery &query)
{
    query.next();

    if (query.isValid()) {
        return retrieveRow(query);
    } else {
        return std::nullopt;
    }
}

QSqlDatabase &AsyncSqlDatabase::db()
{
    return d->database;
}

void printSqlError(const QSqlQuery &query)
{
    qCDebug(asyncdatabase) << "SQL error:" << query.lastError().text();
}

std::optional<QSqlQuery> AsyncSqlDatabase::prepareQuery(const QSqlDatabase &database, const QString &sqlQuery)
{
    qCDebug(asyncdatabase) << "Running" << sqlQuery;

    // Check whether we already have a prepared version of this query
    if (d->preparedQueryCache.contains(sqlQuery)) {
        return d->preparedQueryCache[sqlQuery];
    }

    // If not, prepare one
    QSqlQuery query(database);

    // If this fails, return without caching the query
    if (!query.prepare(sqlQuery)) {
        printSqlError(query);
        return {};
    }

    // Else, cache the prepared query
    d->preparedQueryCache.insert({sqlQuery, query});
    return query;
}

QSqlQuery AsyncSqlDatabase::runQuery(QSqlQuery &query)
{
    if (!query.exec()) {
        printSqlError(query);
    }
    return query;
}

}

struct DatabaseConfigurationPrivate : public QSharedData {
    QString type;
    std::optional<QString> hostName;
    std::optional<QString> databaseName;
    std::optional<QString> userName;
    std::optional<QString> password;
};

DatabaseConfiguration::DatabaseConfiguration() : d(new DatabaseConfigurationPrivate)
{}

DatabaseConfiguration::~DatabaseConfiguration() = default;
DatabaseConfiguration::DatabaseConfiguration(const DatabaseConfiguration &) = default;

void DatabaseConfiguration::setType(const QString &type) {
    d->type = type;
}

const QString &DatabaseConfiguration::type() const {
    return d->type;
}

void DatabaseConfiguration::setHostName(const QString &hostName) {
    d->hostName = hostName;
}

const std::optional<QString> &DatabaseConfiguration::hostName() const {
    return d->hostName;
}

void DatabaseConfiguration::setDatabaseName(const QString &databaseName) {
    d->databaseName = databaseName;
}

const std::optional<QString> &DatabaseConfiguration::databaseName() const {
    return d->databaseName;
}

void DatabaseConfiguration::setUserName(const QString &userName) {
    d->userName = userName;
}

const std::optional<QString> &DatabaseConfiguration::userName() const {
    return d->userName;
}

void DatabaseConfiguration::setPassword(const QString &password) {
    d->password = password;
}

const std::optional<QString> &DatabaseConfiguration::password() const {
    return d->password;
}


struct ThreadedDatabasePrivate {
    asyncdatabase_private::AsyncSqlDatabase db;
};

std::unique_ptr<ThreadedDatabase> ThreadedDatabase::establishConnection(const DatabaseConfiguration &config) {
    auto threadedDb = std::make_unique<ThreadedDatabase>();
    threadedDb->setObjectName(QStringLiteral("database thread"));
    threadedDb->d->db.moveToThread(&*threadedDb);
    threadedDb->start();
    threadedDb->d->db.establishConnection(config);
    return threadedDb;
}

auto ThreadedDatabase::runMigrations(const QString &migrationDirectory) -> QFuture<void> {
    return d->db.runMigrations(migrationDirectory);
}

auto ThreadedDatabase::setCurrentMigrationLevel(const QString &migrationName) -> QFuture<void> {
    return d->db.setCurrentMigrationLevel(migrationName);
}

ThreadedDatabase::ThreadedDatabase()
    : QThread()
    , d(std::make_unique<ThreadedDatabasePrivate>())
{
}

ThreadedDatabase::~ThreadedDatabase()
{
    quit();
    wait();
}

asyncdatabase_private::AsyncSqlDatabase &ThreadedDatabase::db()
{
    return d->db;
}
