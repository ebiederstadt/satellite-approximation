#pragma once

#include "noCopying.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <filesystem>
#include <givde/types.hpp>
#include <string>

namespace fs = std::filesystem;
using namespace givde;

namespace utils {
struct CloudShadowStatus {
    bool clouds_exist = false;
    bool shadows_exist = false;
    f64 percent_invalid = 0.0;
};

class DataBase {
public:
    explicit DataBase(fs::path base_path);
    CloudShadowStatus get_status(std::string const& date_string);

protected:
    SQLite::Database db;
    void create_table();

private:
    SQLite::Statement stmt;
};
}