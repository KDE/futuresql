// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only

#include <QTest>
#include <QTimer>
#include <QSignalSpy>

#include <QCoro/QCoroTask>
#include <QCoro/QCoroFuture>

#include <threadeddatabase.h>

#define QCORO_VERIFY(statement) \
do {\
    if (!QTest::qVerify(static_cast<bool>(statement), #statement, "", __FILE__, __LINE__))\
        co_return;\
} while (false)

#define QCORO_COMPARE(actual, expected) \
do {\
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        co_return;\
} while (false)

struct TestCustom {
    using ColumnTypes = std::tuple<int, QString>;

    static auto fromSql(ColumnTypes &&columns) {
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
    Q_SIGNAL void finished();

    static QCoro::Task<std::unique_ptr<ThreadedDatabase>> initDatabase() {
        DatabaseConfiguration cfg;
        cfg.setDatabaseName(QStringLiteral(":memory:"));
        cfg.setType(DATABASE_TYPE_SQLITE);
        auto db = ThreadedDatabase::establishConnection(cfg);

        co_await db->execute(QStringLiteral("CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, data TEXT)"));
        co_await db->execute(QStringLiteral("INSERT INTO test (data) VALUES (?)"), QStringLiteral("Hello World"));

        co_return db;
    }

private Q_SLOTS:
    void testDeserialization() {
        QMetaObject::invokeMethod(this, [this]() -> QCoro::Task<> {
            auto db = co_await initDatabase();

            // Custom deserializer
            auto opt = co_await db->getResult<TestCustom>(QStringLiteral("SELECT * FROM test LIMIT 1"));
            QCORO_VERIFY(opt.has_value());
            auto list = co_await db->getResults<TestCustom>(QStringLiteral("SELECT * FROM test"));
            QCORO_COMPARE(list.size(), 1);
            co_await db->execute(QStringLiteral("INSERT INTO test (data) VALUES (?)"), QStringLiteral("FutureSQL"));
            list = co_await db->getResults<TestCustom>(QStringLiteral("SELECT * from test ORDER BY id ASC"));
            QCORO_COMPARE(list.size(), 2);
            QCORO_COMPARE(list.at(0).data, u"Hello World");

            // default deserializer
            auto opt2 = co_await db->getResult<TestDefault>(QStringLiteral("SELECT * FROM test LIMIT 1"));
            QCORO_VERIFY(opt2.has_value());
            auto list2 = co_await db->getResults<TestDefault>(QStringLiteral("SELECT * from test ORDER BY id ASC"));
            QCORO_COMPARE(list2.size(), 2);
            QCORO_COMPARE(list2.at(0).data, u"Hello World");

            Q_EMIT finished();
        });

        QSignalSpy spy(this, &SqliteTest::finished);
        spy.wait();
        QVERIFY(spy.count() == 1);
    }
};

QTEST_MAIN(SqliteTest)

#include "sqlitetest.moc"
