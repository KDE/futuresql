<!--
SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im

SPDX-License-Identifier: BSD-2-Clause
-->

# FutureSQL

A non-blocking database framework for Qt.

FutureSQL was in part inspired by Diesel, and provides a higher level of abstraction than QtSql.
Its features include non-blocking database access by default, relatively boilderplate-free queries,
automatic database migrations and simple mapping to objects.

In order to make FutureSQL's use of templates less confusing, FutureSQL uses C++20 concepts,
and requires a C++20 compiler.

Warning: The API is not finalized yet.

## Usage

The following example demonstrates the usage of FutureSQL in conjunction with QCoro:
```cpp
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


// A data structure that represents data from the "test" table
struct HelloWorld {
    // Types that the database columns can be converted to. The types must be convertible from QVariant.
    using ColumnTypes = std::tuple<int, QString>;

    // This function gets a row from the database as a tuple, and puts it into the HelloWorld structs.
    // If the ColumnTypes already match the types and order of the attributes in the struct, you don't need to implement it.
    //
    // Try to comment it out, the example should still compile and work.
    static HelloWorld fromSql(ColumnTypes &&tuple) {
        auto [id, data] = tuple;
        return HelloWorld { id, data };
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
    config.setType(DatabaseType::SQLite);

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
```

In code were coroutines don't make sense, you can use `QCoro::connect` of QCoro > v0.8.0,
or `QFuture<T>::then()` on Qt6, to run a callback once a database query finished.

## Migrations

FutureSQL can manage database migrations in a way that is mostly compatible with diesel.
You just need to pass it a directory (preferably in QRC) that contains migrations in the following format:

```
├── 2022-05-20-194850_init
│   └── up.sql
└── 2022-05-25-212054_playlists
    └── up.sql
```

Naming the migration directories after dates is a good practice, as the migrations are run in sorted order.
Naming them for example by a counting number would break once the numbers get to large.
For example, if you don't use leading zeros and only one digit, you'd only have up to 10 migrations.

The migration directory structure can be generated by the `diesel` command line tool. You can install it using cargo as follows:
```bash
cargo install diesel_cli --features sqlite --no-default-features
```

Finally, you can run the migrations from your C++ code:
```cpp
database->runMigrations(":/migrations/");
```
