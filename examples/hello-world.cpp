// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im
//
// SPDX-License-Identifier: BSD-2-Clause


// Qt
#include <QCoreApplication>
#include <QTimer>

// QCoro
#include <QCoro/Task>
#include <QCoro/QCoroFuture>
#include <QCoro/QCoroSignal>

// FutureSQL
#include <ThreadedDatabase>

// STL
#include <tuple>


// A data structure that represents data from the "test" table
struct HelloWorld {
    // Types that the database columns can be converted to. The types must be convertible from QVariant.
    using ColumnTypes = std::tuple<int, QString>;

    // This function get's a row from the database as a tuple, and puts it into the HelloWorld structs.
    static HelloWorld fromSql(ColumnTypes tuple) {
        auto [id, data] = tuple;
        return HelloWorld { id, data };
    }

    ColumnTypes toSql() const {
        return {id, data};
    }

    // attributes
    int id;
    QString data;
};

QCoro::Task<> databaseExample() {
    // This object contains the database configuration,
    // in this case just the path to the SQLite file, and the database type (SQLite).
    DatabaseConfiguration config;
    config.setDatabaseName("database.sqlite");
    config.setType(DATABASE_TYPE_SQLITE);

    // Here we open the database file, and get a handle to the database.
    auto database = ThreadedDatabase::establishConnection(config);

    // Execute some queries.
    co_await database->execute("CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY AUTOINCREMENT, data TEXT)");

    // Query parameters are bound by position in the query. The execute function is variadic and you can add as many parameters as you need.
    co_await database->execute("INSERT INTO test (data) VALUES (?)", QStringLiteral("Hello World"));

    // Retrieve some data from the database.
    // The data is directly returned as our HelloWorld struct.
    auto results = co_await database->getResults<HelloWorld>("SELECT * FROM test");

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
