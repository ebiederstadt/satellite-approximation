#pragma once

#include "noCopying.h"
#include <sqlite3.h>
#include <string>

namespace utils {
class StmtWrapper {
    MAKE_NONCOPYABLE(StmtWrapper);

public:
    StmtWrapper(sqlite3* db, std::string const& sql);
    StmtWrapper() = default;
    ~StmtWrapper();

    StmtWrapper& operator=(StmtWrapper&&) noexcept;

    sqlite3_stmt* stmt = nullptr;
};
}