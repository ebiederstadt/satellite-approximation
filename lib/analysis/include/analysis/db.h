#pragma once

#include <filesystem>
#include <givde/types.hpp>
#include <sqlite3.h>
#include <variant>

#include "noCopying.h"

namespace fs = std::filesystem;
using namespace givde;

namespace analysis {
struct CloudShadowStatus {
    bool clouds_exist = false;
    bool shadows_exist = false;
};

enum class Indices;
struct UseApproximatedData;
struct UseRealData;
using DataChoices = std::variant<UseApproximatedData, UseRealData>;

class DataBase {
    MAKE_NONCOPYABLE(DataBase);
public:
    DataBase(fs::path const& base_path);
    ~DataBase();

    CloudShadowStatus get_status(std::string date);

    std::optional<int> result_exists(
        Indices index,
        f64 threshold,
        int start_year,
        int end_year,
        DataChoices choice);
    int save_result_in_table(
        Indices index,
        f64 threshold,
        int start_year,
        int end_year,
        DataChoices choice);

private:
    sqlite3* db;
    std::string sql;
    sqlite3_stmt* stmt;

    void create_sis_table();
    sqlite3_stmt* prepare_stmt(
        std::string sql_string,
        Indices index,
        f64 threshold,
        int start_year,
        int end_year,
        DataChoices choice);
};
}
