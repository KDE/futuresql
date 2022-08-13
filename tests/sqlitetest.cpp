// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only

#include <QTest>
#include <QTimer>

#include <QCoro/Task>
#include <QCoro/QCoroFuture>

#include <threadeddatabase.h>

struct Test {
    using ColumnTypes = std::tuple<int, QString>;

    static auto fromSql(ColumnTypes columns) {
        auto [id, data] = columns;
        return Test { id, data };
    }

public:
    int id;
    QString data;
};

class SqliteTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testInMemory() {
        bool finished = false;
        QMetaObject::invokeMethod(this, [&]() -> QCoro::Task<> {
            DatabaseConfiguration cfg;
            cfg.setDatabaseName(":memory:");
            cfg.setType(DATABASE_TYPE_SQLITE);
            auto db = ThreadedDatabase::establishConnection(cfg);

            co_await db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, data TEXT)");
            co_await db->execute("INSERT INTO test (data) VALUES (?)", "Hello World");
            auto opt = co_await db->getResult<Test>("SELECT * FROM test LIMIT 1");
            Q_ASSERT(opt.has_value());
            auto list = co_await db->getResults<Test>("SELECT * FROM test");
            Q_ASSERT(list.size() == 1);
            co_await db->execute("INSERT INTO test (data) VALUES (?)", "FutureSQL");
            list = co_await db->getResults<Test>("SELECT * from test ORDER BY id ASC");
            Q_ASSERT(list.size() == 2);
            Q_ASSERT(list.at(0).data == "Hello World");

            Q_UNREACHABLE();

            finished = true;
        });
        while (!finished) {
            QCoreApplication::processEvents();
        }
    }
};

QTEST_MAIN(SqliteTest)

#include "sqlitetest.moc"
