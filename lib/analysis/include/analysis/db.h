#pragma once

#include <filesystem>
#include <givde/types.hpp>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <variant>

#include "forward.h"
#include "utils/date.h"
#include "utils/db.h"
#include "utils/noCopying.h"

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
    MAKE_NONMOVABLE(DataBase);

public:
    explicit DataBase(fs::path const& base_path);
    ~DataBase();

    CloudShadowStatus get_status(std::string date);

    std::vector<std::string> get_approximated_data(std::string const& date_string);

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

    void save_noise_removal(std::string const& date_string, f64 percent_invalid, int threshold);
    bool noise_exists(std::string const& date_string, int threshold);

    void store_index_info(std::string const& date_string, Indices index, MatX<f64> const& index_matrix, DataChoices choice);
    void store_index_info(std::string const& date_string, Indices index, MatX<f64> const& index_matrix, MatX<bool> const& invalid_matrix, UseRealData choice);

private:
    sqlite3* db;
    std::string sql;
    std::unique_ptr<utils::StmtWrapper> stmt;
    std::shared_ptr<spdlog::logger> logger;

    void create_sis_table();
    void prepare_stmt(
        std::string sql_string,
        Indices index,
        f64 threshold,
        int start_year,
        int end_year,
        DataChoices choice,
        utils::StmtWrapper &stmt_to_prepare);

    int index_table_helper(std::string const& date_string, Indices index, f64 min, f64 max, f64 mean, int num_elements, bool use_approx_data, utils::StmtWrapper &stmt);
};
}
