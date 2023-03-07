// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im
//
// SPDX-License-Identifier: BSD-2-Clause

/*
 * This example demonstrates how to use transactions with FutureSQL
 * For explanations of the general concepts, see the hello-world example,
 * which is very similar (except it doesn't use transactions, of course),
 * but it contains more comments.
 */

// Qt
#include <QCoreApplication>
#include <QTimer>

// QCoro
#include <QCoro/QCoroTask>
#include <QCoro/QCoroFuture>

// FutureSQL
#include <ThreadedDatabase>

// STL
#include <tuple>

// Why is this function not part of the library, I hear you ask.
// Currently, FutureSQL should not hard-depend on QCoro,
// even though with Qt5, QCoro is the only way to use it without
// hand-crafting helper functions to deal with QFutures.
template <typename Func>
QCoro::Task<> transaction(std::unique_ptr<ThreadedDatabase> &database, Func queryFunc) {
    co_await database->execute(QStringLiteral("BEGIN TRANSACTION"));
    co_await queryFunc();
    co_await database->execute(QStringLiteral("COMMIT"));
}

struct HelloWorld {
    using ColumnTypes = std::tuple<int, QString>;

    // attributes
    int id;
    QString data;
};

QCoro::Task<> databaseExample() {
    // This object contains the database configuration,
    // in this case just the path to the SQLite file, and the database type (SQLite).
    DatabaseConfiguration config;
    config.setDatabaseName(QStringLiteral("database.sqlite"));
    config.setType(DatabaseType::SQLite);

    // Here we open the database file, and get a handle to the database.
    auto database = ThreadedDatabase::establishConnection(config);

    // Run the following steps in a transaction
    co_await transaction(database, [&database]() -> QCoro::Task<> {
        // Create the table
        co_await database->execute(QStringLiteral("CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY AUTOINCREMENT, data TEXT)"));

        // Insert some initial data
        co_await database->execute(QStringLiteral("INSERT INTO test (data) VALUES (?)"), QStringLiteral("Hello World"));
    });

    // Retrieve some data from the database.
    // The data is directly returned as our HelloWorld struct.
    auto results = co_await database->getResults<HelloWorld>(QStringLiteral("SELECT * FROM test"));

    // Print out the data in the result list
    for (const auto &result : results) {
        qDebug() << result.id << result.data;
    }

    // Quit the event loop as we are done
    QCoreApplication::instance()->quit();
}

// Just a minimal main function for QCoro, to start the Qt event loop.
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QTimer::singleShot(0, databaseExample);
    return app.exec();
}
