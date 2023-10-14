#pragma once

#include <utils/date.h>
#include <utils/db.h>
#include <utils/noCopying.h>

#include "forward.h"

namespace analysis {
class DataBase : public utils::DataBase {
public:
    explicit DataBase(fs::path base_path);

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
    std::string sql;

    void create_sis_table();
    void prepare_stmt(
        std::string sql_string,
        Indices index,
        f64 threshold,
        int start_year,
        int end_year,
        DataChoices choice,
        utils::StmtWrapper& stmt_to_prepare);

    int index_table_helper(std::string const& date_string, Indices index, f64 min, f64 max, f64 mean, int num_elements, bool use_approx_data, utils::StmtWrapper& stmt);
};
}
