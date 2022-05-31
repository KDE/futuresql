#include "querygenerator.h"
#include "threadeddatabase.h"

namespace ranges = std::ranges;

QFuture<void> InsertStatement::execute() {
    return m_db->db().executeGeneric(string(), m_bindValues);
}

QFuture<asyncdatabase_private::Rows> SelectStatement::genericGetResults()
{
    return m_db->db().fetchGeneric(string(), m_bindValues);
}
