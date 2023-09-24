#pragma once

#include <sqlite3.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace analysis {
    struct CloudShadowStatus {
        bool clouds_exist;
        bool shadows_exist;
    };

    class DataBase {
    public:
        DataBase(fs::path const &base_path);
        ~DataBase();

        CloudShadowStatus get_status(std::string_view date);

    private:
        sqlite3* db;
        std::string sql;
        sqlite3_stmt* stmt;
    };
}
