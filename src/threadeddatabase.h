// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only

#pragma once

class QUrl;
class QSqlDatabase;

#include <QString>
#include <QObject>
#include <QFuture>
#include <QSharedDataPointer>

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "threadeddatabase_p.h"

#include <futuresql_export.h>

struct DatabaseConfigurationPrivate;

///
/// Options for connecting to a database
///
class FUTURESQL_EXPORT DatabaseConfiguration {
public:
    DatabaseConfiguration();
    DatabaseConfiguration(const DatabaseConfiguration &);
    ~DatabaseConfiguration();

    /// Set the name of the database driver, for example DATABASE_TYPE_SQLITE or a custom string.
    void setType(const QString &type);
    const QString &type() const;

    /// Set the hostname
    void setHostName(const QString &hostName);
    const std::optional<QString> &hostName() const;

    /// Set the name of the database (path of the file for SQLite)
    void setDatabaseName(const QString &databaseName);
    const std::optional<QString> &databaseName() const;

    /// Set user name
    void setUserName(const QString &userName);
    const std::optional<QString> &userName() const;

    /// Set password
    void setPassword(const QString &password);
    const std::optional<QString> &password() const;

private:
    QSharedDataPointer<DatabaseConfigurationPrivate> d;
};

///
/// The SQLite database driver
///
const QString DATABASE_TYPE_SQLITE = QStringLiteral("QSQLITE");

template <typename T>
concept FromSql = requires(T v, typename T::ColumnTypes row)
{
    typename T::ColumnTypes;
    { std::tuple(row) } -> std::same_as<typename T::ColumnTypes>;
};

template <typename ...Args>
constexpr bool isQVariantConvertible = std::conjunction_v<std::is_convertible<Args, QVariant>...>;

struct ThreadedDatabasePrivate;

///
/// A database connection that lives on a new thread
///
class FUTURESQL_EXPORT ThreadedDatabase : public QThread {
public:
    ///
    /// \brief Connect to a database
    /// \param configuration of the database connection
    /// \return
    ///
    static std::unique_ptr<ThreadedDatabase> establishConnection(const DatabaseConfiguration &config);

    ///
    /// \brief Execute an SQL query on the database, ignoring the result.
    /// \param SQL query string to execute
    /// \param Parameters to bind to the placeholders in the SQL Query
    /// \return
    ///
    template <typename ...Args>
    requires isQVariantConvertible<Args...>
    auto execute(const QString &sqlQuery, Args... args) -> QFuture<void> {
        return db().execute(sqlQuery, args...);
    }

    ///
    /// Run database migrations in the given directory.
    /// The directory needs to contain a subdirectory for each migration.
    /// The subdirectories need to be named so that when sorted alphabetically the migrations will be run in the correct order.
    /// Each subdirectory needs to contain a file named up.sql.
    ///
    /// \param Directory which contains the migrations.
    /// \return a future that finishes when the database changes are finished
    ///
    auto runMigrations(const QString &migrationDirectory) -> QFuture<void>;

    ///
    /// Declare that the database is currently at the state of the migration in the migration subdirectory
    /// migrationName.
    ///
    /// The automatic migrations will then start with all migrations that are newer than migrationName.
    ///
    /// @warning This function should only be used for the initial switch from a different migration system, for example a custom made one.
    /// \param migrationName
    /// \return a future that finishes when the database changes are finished
    ///
    auto setCurrentMigrationLevel(const QString &migrationName) -> QFuture<void>;

    ///
    /// \brief Execute an SQL query on the database, retrieving the result.
    /// \param SQL Query to execute
    /// \param parameters to bind to the placeholders in the SQL query.
    /// \return Future of a list of lists of variants.
    ///
    /// T must provide a tuple of the column types as `using ColumnTypes = std::tuple<...>`
    /// and a, if the column types are not the same types in the same order as the attributes of the struct,
    /// a `static T fromSql(ColumnTypes tuple)` deserialization method.
    ///
    template <typename T, typename ...Args>
    requires FromSql<T> && isQVariantConvertible<Args...>
    auto getResults(const QString &sqlQuery, Args... args) -> QFuture<std::vector<T>> {
        return db().getResults<T, Args...>(sqlQuery, args...);
    }

    ///
    /// \brief Like getResults, but for retrieving just one row.
    ///
    template <typename T, typename ...Args>
    requires FromSql<T> && isQVariantConvertible<Args...>
    auto getResult(const QString &sqlQuery, Args... args) -> QFuture<std::optional<T>> {
        return db().getResult<T, Args...>(sqlQuery, args...);
    }

    template <typename Func>
    requires std::is_invocable_v<Func, const QSqlDatabase &>
    auto runOnThread(Func &&func) -> QFuture<std::invoke_result_t<Func, const QSqlDatabase &>> {
        return db().runOnThread(std::move(func));
    }

    ThreadedDatabase();
    ~ThreadedDatabase();

private:
    asyncdatabase_private::AsyncSqlDatabase &db();

    std::unique_ptr<ThreadedDatabasePrivate> d;
};

///
/// \brief Deserialize just a single value from a query result.
///
template <typename T>
struct SingleValue {
    using ColumnTypes = std::tuple<T>;

    static SingleValue fromSql(ColumnTypes tuple) {
        auto [value] = tuple;
        return SingleValue { value };
    }

    operator const T &() const {
        return value;
    }

    T value;
};
