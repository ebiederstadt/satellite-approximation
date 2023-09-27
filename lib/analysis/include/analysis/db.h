#pragma once

#include <filesystem>
#include <givde/types.hpp>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <variant>

#include "noCopying.h"
#include "forward.h"

namespace fs = std::filesystem;
using namespace givde;

namespace analysis {
struct CloudShadowStatus {
    bool clouds_exist = false;
    bool shadows_exist = false;
    f64 percent_invalid = 0.0;
};

class DataBase {
    MAKE_NONCOPYABLE(DataBase);

public:
    DataBase(fs::path const& base_path);
    ~DataBase();

    CloudShadowStatus get_status(std::string date);

    std::vector<std::string> get_approximated_data(std::string date);

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
        DataChoices choice,
        f64 min,
        f64 max,
        f64 mean,
        int num_days_used);

private:
    sqlite3* db;
    std::string sql;
    sqlite3_stmt* stmt;
    std::shared_ptr<spdlog::logger> logger;

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
