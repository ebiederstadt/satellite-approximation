#pragma once

#include "noCopying.h"
#include <filesystem>
#include <givde/types.hpp>
#include <sqlite3.h>
#include <string>

namespace fs = std::filesystem;
using namespace givde;

namespace utils {
class StmtWrapper {
    MAKE_NONCOPYABLE(StmtWrapper);

public:
    StmtWrapper(sqlite3* db, std::string const& sql);
    StmtWrapper() = default;
    ~StmtWrapper() noexcept(false);

    StmtWrapper& operator=(StmtWrapper&&) noexcept;

    sqlite3_stmt* stmt = nullptr;
};

struct CloudShadowStatus {
    bool clouds_exist = false;
    bool shadows_exist = false;
    f64 percent_invalid = 0.0;
    f64 percent_invalid_denoised;
};

class DataBase {
    MAKE_NONCOPYABLE(DataBase);
    MAKE_NONMOVABLE(DataBase);

public:
    explicit DataBase(fs::path base_path);
    ~DataBase() noexcept(false);

    CloudShadowStatus get_status(std::string const& date_string);

protected:
    sqlite3* db = nullptr;

private:
    fs::path db_path;
    std::unique_ptr<StmtWrapper> stmt;
};
}