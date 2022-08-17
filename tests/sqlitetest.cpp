// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only

#include <QTest>
#include <QTimer>

#include <QCoro/QCoroTask>
#include <QCoro/QCoroFuture>

#include <threadeddatabase.h>

struct TestCustom {
    using ColumnTypes = std::tuple<int, QString>;

    static auto fromSql(ColumnTypes columns) {
        auto [id, data] = columns;
        return TestCustom { id, data };
    }

public:
    int id;
    QString data;
};

struct TestDefault {
    using ColumnTypes = std::tuple<int, QString>;

public:
    int id;
    QString data;
};

class SqliteTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    static QCoro::Task<std::unique_ptr<ThreadedDatabase>> initDatabase() {
        DatabaseConfiguration cfg;
        cfg.setDatabaseName(":memory:");
        cfg.setType(DATABASE_TYPE_SQLITE);
        auto db = ThreadedDatabase::establishConnection(cfg);

        co_await db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, data TEXT)");
        co_await db->execute("INSERT INTO test (data) VALUES (?)", "Hello World");

        co_return db;
    }

    void testDeserialization() {
        bool finished = false;
        QMetaObject::invokeMethod(this, [&finished]() -> QCoro::Task<> {
            auto db = co_await initDatabase();

            // Custom deserializer
            auto opt = co_await db->getResult<TestCustom>("SELECT * FROM test LIMIT 1");
            Q_ASSERT(opt.has_value());
            auto list = co_await db->getResults<TestCustom>("SELECT * FROM test");
            Q_ASSERT(list.size() == 1);
            co_await db->execute("INSERT INTO test (data) VALUES (?)", "FutureSQL");
            list = co_await db->getResults<TestCustom>("SELECT * from test ORDER BY id ASC");
            Q_ASSERT(list.size() == 2);
            Q_ASSERT(list.at(0).data == "Hello World");

            // default deserializer
            auto opt2 = co_await db->getResult<TestDefault>("SELECT * FROM test LIMIT 1");
            Q_ASSERT(opt2.has_value());
            auto list2 = co_await db->getResults<TestDefault>("SELECT * from test ORDER BY id ASC");
            Q_ASSERT(list2.size() == 2);
            Q_ASSERT(list2.at(0).data == "Hello World");


            finished = true;
        });
        while (!finished) {
            QCoreApplication::processEvents();
        }
    }
};

QTEST_MAIN(SqliteTest)

#include "sqlitetest.moc"
