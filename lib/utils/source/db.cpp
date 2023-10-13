#include "utils/db.h"
#include "utils/error.h"
#include "utils/log.h"

namespace utils {
StmtWrapper::StmtWrapper(sqlite3* db, std::string const& sql)
{
    int rc = sqlite3_prepare_v2(db, sql.c_str(), (int)sql.length(), &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw utils::DBError("Failed to prepare statement", rc);
    }
}

StmtWrapper::~StmtWrapper()
{
    int rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK) {
        throw utils::DBError("Failed to finalize prepared statement", rc);
    }
}

StmtWrapper& StmtWrapper::operator=(StmtWrapper&& other) noexcept
{
    stmt = other.stmt;
}
}