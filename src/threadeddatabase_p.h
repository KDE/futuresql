// SPDX-FileCopyrightText: 2022 Jonah Brüchert <jbb@kaidan.im
//
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL

#pragma once

#include <memory>
#include <tuple>
#include <optional>
#include <concepts>

#include <QFuture>
#include <QFutureWatcher>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QThread>

#include <futuresql_export.h>

class DatabaseConfiguration;

namespace asyncdatabase_private {

// Helpers for iterating over tuples
template <typename Tuple, typename Func, std::size_t i>
inline constexpr void iterate_impl(Tuple &tup, Func fun)
{
    if constexpr(i >= std::tuple_size_v<std::decay_t<decltype(tup)>>) {
        return;
    } else {
        fun(std::get<i>(tup));
        return asyncdatabase_private::iterate_impl<Tuple, Func, i + 1>(tup, fun);
    }
}

template <typename Tuple, typename Func>
inline constexpr void iterate_tuple(Tuple &tup, Func fun)
{
    asyncdatabase_private::iterate_impl<Tuple, Func, 0>(tup, fun);
}

// Helpers to construct a struct from a tuple
template <typename T, typename ...Args>
constexpr T constructFromTuple(std::tuple<Args...> &&args) {
    return std::apply([](auto && ...args) { return T { args...}; }, std::move(args));
}

template <typename T>
concept FromSql = requires(T v, typename T::ColumnTypes row)
{
    typename T::ColumnTypes;
    { std::tuple(row) } -> std::same_as<typename T::ColumnTypes>;
};

template <typename T>
concept FromSqlCustom = requires(T v, typename T::ColumnTypes row)
{
    typename T::ColumnTypes;
    { std::tuple(row) } -> std::same_as<typename T::ColumnTypes>;
    { T::fromSql(std::move(row)) } -> std::same_as<T>;
};

template <typename T>
requires FromSql<T> && (!FromSqlCustom<T>)
auto deserialize(typename T::ColumnTypes &&row) {
    return constructFromTuple<T>(std::move(row));
}

template <typename T>
requires FromSqlCustom<T>
auto deserialize(typename T::ColumnTypes &&row) {
    return T::fromSql(std::move(row));
}

using Row  = std::vector<QVariant>;
using Rows = std::vector<Row>;

template <typename RowTypesTuple>
auto parseRow(const Row &row) -> RowTypesTuple
{
    auto tuple = RowTypesTuple();
    int i = 0;
    asyncdatabase_private::iterate_tuple(tuple, [&](auto &elem) {
        elem = row.at(i).value<std::decay_t<decltype(elem)>>();
        i++;
    });
    return tuple;
}

template <typename RowTypesTuple>
auto parseRows(const Rows &rows) -> std::vector<RowTypesTuple> {
    std::vector<RowTypesTuple> parsedRows;
    parsedRows.reserve(rows.size());
    std::transform(rows.begin(), rows.end(), std::back_inserter(parsedRows), parseRow<RowTypesTuple>);
    return parsedRows;
}

void runDatabaseMigrations(QSqlDatabase &database, const QString &migrationDirectory);

void printSqlError(const QSqlQuery &query);

struct AsyncSqlDatabasePrivate;

class FUTURESQL_EXPORT AsyncSqlDatabase : public QObject {
    Q_OBJECT

public:
    AsyncSqlDatabase();
    ~AsyncSqlDatabase() override;

    QFuture<void> establishConnection(const DatabaseConfiguration &configuration);

    template <typename T, typename ...Args>
    auto getResults(const QString &sqlQuery, Args... args) -> QFuture<std::vector<T>> {
        return runAsync([=, this] {
            auto query = executeQuery(sqlQuery, args...);

            // If the query failed to execute, don't try to deserialize it
            if (!query) {
                return std::vector<T> {};
            }

            auto rows = parseRows<typename T::ColumnTypes>(retrieveRows(*query));

            std::vector<T> deserializedRows;
            std::transform(rows.begin(), rows.end(), std::back_inserter(deserializedRows), [](auto &&row) {
                return deserialize<T>(std::move(row));
            });
            return deserializedRows;
        });
    }

    template <typename T, typename ...Args>
    auto getResult(const QString &sqlQuery, Args... args) -> QFuture<std::optional<T>> {
        return runAsync([=, this]() -> std::optional<T> {
            auto query = executeQuery(sqlQuery, args...);

            // If the query failed to execute, don't try to deserialize it
            if (!query) {
                return {};
            }

            if (const auto row = retrieveOptionalRow(*query)) {
                return deserialize<T>(parseRow<typename T::ColumnTypes>(*row));
            }

            return {};
        });
    }

    template <typename ...Args>
    auto execute(const QString &sqlQuery, Args... args) -> QFuture<void> {
        return runAsync([=, this] {
            executeQuery(sqlQuery, args...);
        });
    }

    template <typename Func>
    requires std::is_invocable_v<Func, const QSqlDatabase &>
    auto runOnThread(Func &&func) -> QFuture<std::invoke_result_t<Func, const QSqlDatabase &>> {
        return runAsync([func = std::move(func), this]() {
            return func(db());
        });
    }

    auto runMigrations(const QString &migrationDirectory) -> QFuture<void>;

    auto setCurrentMigrationLevel(const QString &migrationName) -> QFuture<void>;

private:
    template <typename ...Args>
    std::optional<QSqlQuery> executeQuery(const QString &sqlQuery, Args... args) {
        auto query = prepareQuery(db(), sqlQuery);
        if (!query) {
            return {};
        }

        auto argsTuple = std::make_tuple<Args...>(std::move(args)...);
        int i = 0;
        asyncdatabase_private::iterate_tuple(argsTuple, [&](auto &arg) {
            query->bindValue(i, arg);
            i++;
        });
        return runQuery(std::move(*query));
    }

    template <typename Functor>
    QFuture<std::invoke_result_t<Functor>> runAsync(Functor func) {
        using ReturnType = std::invoke_result_t<Functor>;
        QFutureInterface<ReturnType> interface;
        QMetaObject::invokeMethod(this, [interface, func]() mutable {
            if constexpr (!std::is_same_v<ReturnType, void>) {
                auto result = func();
                interface.reportResult(result);
            } else {
                func();
            }

            interface.reportFinished();
        });

        return interface.future();
    }

    Row retrieveRow(const QSqlQuery &query);
    Rows retrieveRows(QSqlQuery &query);
    std::optional<Row> retrieveOptionalRow(QSqlQuery &query);

    QSqlDatabase &db();

    // non-template helper functions to allow patching a much as possible in the shared library
    std::optional<QSqlQuery> prepareQuery(const QSqlDatabase &database, const QString &sqlQuery);
    QSqlQuery runQuery(QSqlQuery &&query);

    std::unique_ptr<AsyncSqlDatabasePrivate> d;
};

}
