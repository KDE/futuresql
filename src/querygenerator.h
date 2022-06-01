#pragma once

#include <QString>
#include <QTextStream>
#include <QFuture>

#include <memory>
#include <optional>
#include <vector>

#include "threadeddatabase_p.h"
#include "querygenerator_p.h"

struct Condition {
    enum Operator {
        Equal,
        LessThan,
        GreaterThan,
        LessOrEqual,
        GreaterOrEqual
    };

    enum Chain {
        And,
        Or
    };

    Condition &attr(const QString &attribute) {
        m_attribute = attribute;
        return *this;
    }

    Condition &equals() {
        m_op = Equal;
        return *this;
    }

    Condition &leq() {
        m_op = LessOrEqual;
        return *this;
    }

    Condition &geq() {
        m_op = GreaterOrEqual;
        return *this;
    }

    Condition &lt() {
        m_op = LessThan;
        return *this;
    }

    Condition &gt() {
        m_op = GreaterThan;
        return *this;
    }

    Condition &value(const QVariant &value) {
        m_cmpValue = value;
        return *this;
    }

    Condition andWhere() {
        m_chain = Chain::And;

        Condition condition;
        condition.conditions = conditions;
        condition.conditions.push_back(*this);

        return condition;
    }

    Condition orWhere() {
        m_chain = Chain::Or;

        Condition condition;
        condition.conditions = conditions;
        condition.conditions.push_back(*this);

        return condition;
    }

    std::vector<Condition> collect() {
        auto c = conditions;
        c.push_back(*this);
        return c;
    }

    QString string() const {
        QString str;
        QTextStream out(&str);

        auto generateClause = [&](Condition condition) {
            out << condition.m_attribute;
            switch (condition.m_op) {
            case Condition::Equal:
                out << " == ";
                break;
            case Condition::LessThan:
                out << " < ";
                break;
            case Condition::GreaterThan:
                out << " > ";
                break;
            case Condition::LessOrEqual:
                out << " <= ";
                break;
            case Condition::GreaterOrEqual:
                out << " >= ";
                break;
            }
            out  << "? ";

            if (condition.m_chain) {
            switch(*condition.m_chain) {
            case And:
                out << "AND ";
                break;
            case Or:
                out << "OR ";
            }
            }
        };

        std::ranges::for_each(conditions, generateClause);
        generateClause(*this);

        return str;
    }

    QString m_attribute;
    Operator m_op;
    QVariant m_cmpValue;
    std::optional<Chain> m_chain;
    std::vector<Condition> conditions;
};

class ThreadedDatabase;

struct InsertStatement {
    static InsertStatement build() {
        return InsertStatement {};
    }

    InsertStatement &db(ThreadedDatabase *db) {
        m_db = db;
        return *this;
    }

    InsertStatement &into(const QString &table) {
        m_into = table;
        return *this;
    }

    InsertStatement &ignoreExisting() {
        m_ignore = true;
        return *this;
    }

    template <typename ...Args>
    InsertStatement &values(Args ...values) {
        m_bindValues = {values...};
        return *this;
    }

    template <typename ...Args>
    InsertStatement &columns(Args &...columns) {
        m_columns = {columns...};
        return *this;
    }

    QString string() const {
        QString str;
        QTextStream out(&str);

        Q_ASSERT(m_columns.size() == m_bindValues.size());

        out << "INSERT INTO "
            << m_into << " (";

        out << m_columns.front();
        std::for_each(m_columns.begin() + 1, m_columns.end(), [&](const auto column) {
            out << ", " << column;
        });

        out << ") VALUES (";
        for (size_t i = 0; i < m_columns.size() - 1; i++) {
            out << "?, ";
        }
        out << "?" << ")";

        return str;
    }

    QFuture<void> execute();

    bool m_ignore;
    std::vector<QString> m_columns;
    QString m_into;
    std::vector<QVariant> m_bindValues;
    ThreadedDatabase *m_db;
};

struct SelectStatement {
    enum Constraint {
        All,
        Distinct
    };

    enum Order {
        Ascending,
        Descending
    };

    enum Join {
        Natural
    };

    static SelectStatement build() {
        return SelectStatement {};
    }

    SelectStatement &db(ThreadedDatabase *db) {
        m_db = db;
        return *this;
    }

    SelectStatement &constraint(Constraint c) {
        m_constraint = c;
        return *this;
    }

    SelectStatement &into(const QString &tableName) {
        m_into = tableName;
        return *this;
    }

    template <typename ...Args>
    SelectStatement &columns(Args &...columns) {
        m_columns = {columns...};
        return *this;
    }

    SelectStatement &from(const QString &from) {
        m_from.push_back(from);
        return *this;
    }

    SelectStatement &where(Condition &condition) {
        m_where = std::move(condition);
        return *this;
    }

    SelectStatement &groupBy(const QString &attribute) {
        m_groupBy = attribute;
        return *this;
    }

    SelectStatement &orderBy(const QString &attribute, Order order) {
        m_orderBy = attribute;
        m_order = order;
        return *this;
    }

    SelectStatement &naturalJoin(const QString &table) {
        m_joinType = Natural;
        m_joinTable = table;
        return *this;
    }

    QString string() const {
        QString str;
        QTextStream out(&str);

        out << "SELECT ";
        if (m_constraint) {
            switch (*m_constraint) {
            case All:
                out << "ALL ";
            case Distinct:
                out << "DISTINCT ";
            }
        }

        if (!m_columns.empty()) {
            out << m_columns.front();
            std::for_each(m_from.begin() + 1, m_from.end(), [&](const auto &from) {
                out << "," << from;
            });
            out << " ";
        }

        if (m_into) {
            out << "INTO " << *m_into;
        }

        if (!m_from.empty()) {
            out << "FROM ";
            out << m_from.front();
            std::for_each(m_from.begin() + 1, m_from.end(), [&](const auto &from) {
                out << "," << from;
            });
            out << " ";
        }

        if (m_joinTable) {
            out << " NATURAL JOIN ";
            switch (m_joinType) {
            case Natural:
                out << *m_joinTable << " ";
                break;
            }
        }

        if (m_where) {
            out << "WHERE ";
            out << m_where->string() << " ";
        }

        if (m_groupBy) {
            out << "GROUP BY " << *m_groupBy << " ";
        }

        if (m_order && m_orderBy) {
            out << "ORDER BY ";
            out << *m_orderBy << " ";
            switch (*m_order) {
            case Order::Ascending:
                out << "ASC ";
                break;
            case Order::Descending:
                out << "DESC ";
                break;
            }
        }

        return str;
    }

    template <typename T>
    QFuture<std::vector<T>> getResults()
    {
        auto future = genericGetResults();
        return mapFuture(future, [](auto rows) {
            auto parsedRows = asyncdatabase_private::parseRows<typename T::ColumnTypes>(std::move(rows));
            std::vector<T> out;
            std::ranges::transform(parsedRows, std::back_inserter(out), T::fromSql);
            return out;
        });
    }

private:
    QFuture<asyncdatabase_private::Rows> genericGetResults();

    std::optional<Constraint> m_constraint;
    std::vector<QString> m_columns;
    std::optional<QString> m_into;
    std::vector<QString> m_from;
    Join m_joinType;
    std::optional<QString> m_joinTable;
    std::optional<Condition> m_where;
    std::optional<QString> m_groupBy;
    std::optional<Order> m_order;
    std::optional<QString> m_orderBy;

    ThreadedDatabase *m_db = nullptr;
};

struct UpdateStatement {
    static UpdateStatement build() {
        return UpdateStatement {};
    }

    UpdateStatement &db(ThreadedDatabase *db) {
        m_db = db;
        return *this;
    }

    UpdateStatement &table(const QString &tableName) {
        m_table = tableName;
        return *this;
    }

    UpdateStatement &set(const QString &attribute, const QVariant &value) {
        m_sets.push_back({attribute, value});
        return *this;
    }

    UpdateStatement &where(Condition condition) {
        m_condition = condition;
        return *this;
    }

    QString string() {
        QString str;
        QTextStream out(&str);

        out << "UPDATE " << m_table << " SET ";
        out << m_sets.front().first << " = " << " ? ";
        std::for_each(m_sets.begin() + 1, m_sets.end(), [&](auto pair) {
            auto [key, value] = pair;
            out << ", " << key << " = " << " ? ";
        });

        if (m_condition) {
            out << "WHERE " << m_condition->string() << " ";
        }

        return str;
    }

    std::vector<QVariant> bindValues() {
        std::vector<QVariant> values;
        std::ranges::transform(m_sets, std::back_inserter(values), [](const auto pair) {
            return pair.second;
        });
        std::ranges::transform(m_condition->collect(), std::back_inserter(values), [](const auto condition) {
            return condition.m_cmpValue;
        });
        return values;
    }

    QFuture<void> execute();

    QString m_table;
    std::vector<std::pair<QString, QVariant>> m_sets;
    std::optional<Condition> m_condition;

    ThreadedDatabase *m_db;
};
