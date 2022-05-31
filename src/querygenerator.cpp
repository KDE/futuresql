#include "querygenerator.h"
#include "threadeddatabase.h"

namespace ranges = std::ranges;

QFuture<void> InsertStatement::execute() {
    return m_db->db().executeGeneric(string(), m_bindValues);
}

QFuture<asyncdatabase_private::Rows> SelectStatement::genericGetResults()
{
    std::vector<QVariant> bindValues;
    if (m_where) {
        ranges::transform(m_where->conditions, std::back_inserter(bindValues), [](Condition condition) {
            return condition.m_cmpValue;
        });
        bindValues.push_back(m_where->m_cmpValue);
    }
    return m_db->db().fetchGeneric(string(), bindValues);
}
