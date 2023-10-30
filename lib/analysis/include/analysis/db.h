#pragma once

#include <utils/date.h>
#include <utils/db.h>
#include <utils/indices.h>
#include <utils/noCopying.h>

#include "forward.h"

namespace analysis {
class DataBase : public utils::DataBase {
public:
    explicit DataBase(fs::path base_path);

    std::vector<std::string> get_approximated_data(std::string const& date_string);

    std::optional<int> result_exists(
        utils::Indices index,
        f64 threshold,
        int start_year,
        int end_year,
        DataChoices choice);
    int save_result_in_table(
        utils::Indices index,
        f64 threshold,
        int start_year,
        int end_year,
        DataChoices choice,
        f64 min,
        f64 max,
        f64 mean,
        int num_days_used);

    void store_index_info(std::string const& date_string, utils::Indices index, MatX<f64> const& index_matrix, DataChoices choice);
    void store_index_info(std::string const& date_string, utils::Indices index, MatX<f64> const& index_matrix, MatX<bool> const& invalid_matrix, UseRealData choice);

private:
    std::string sql;

    void create_sis_table();
    SQLite::Statement prepare_stmt(
        std::string sql_string,
        utils::Indices index,
        f64 threshold,
        int start_year,
        int end_year,
        DataChoices choice);

    SQLite::Statement index_table_helper(std::string const& date_string, utils::Indices index, f64 min, f64 max, f64 mean, int num_elements, bool use_approx_data);
};
}
