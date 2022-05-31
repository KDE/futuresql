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

    QString string() const {
        QString str;
        QTextStream out(&str);

        out << m_attribute;
        switch (m_op) {
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
        out  << "?" << " ";

        if (m_chain && m_nextCondition) {
            switch (*m_chain) {
            case And:
                out << "AND ";
                break;
            case Or:
                out << "OR ";
                break;
            }

            out << m_nextCondition->string();
        }

        return str;
    }

    std::vector<QVariant> bindValues() const {
        std::vector<QVariant> values;
        values.push_back(m_cmpValue);

        if (m_nextCondition) {
            auto other = m_nextCondition->bindValues();
            values.insert(values.end(), other.begin(), other.end());
        }

        return values;
    }

    QString m_attribute;
    Operator m_op;
    QVariant m_cmpValue;
    std::optional<Chain> m_chain;
    std::shared_ptr<Condition> m_nextCondition;
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

    enum OrderBy {
        Ascending,
        Descending
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

    SelectStatement &where(const QString &attribute, Condition::Operator op, const QString &cmpValue) {
        m_where = Condition {
            attribute,
            op,
            cmpValue
        };

        m_bindValues = m_where->bindValues();

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

        if (m_where) {
            out << "WHERE ";
            out << m_where->string() << " ";
        }

        if (m_order) {
            out << "ORDER BY ";
            switch (*m_order) {
            case OrderBy::Ascending:
                out << "ASC";
            case OrderBy::Descending:
                out << "DESC";
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
    std::optional<Condition> m_where;
    std::optional<OrderBy> m_order;

    std::vector<QVariant> m_bindValues;
    ThreadedDatabase *m_db = nullptr;
};
