#include "analysis/db.h"
#include "analysis/sis.h"
#include "analysis/utils.h"

#include <fmt/format.h>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

namespace analysis {
static auto logger = utils::create_logger("analysis::DB");

DataBase::DataBase(fs::path base_path)
    : utils::DataBase(std::move(base_path))
{
}

std::vector<std::string> DataBase::get_approximated_data(std::string const& date_string)
{
    utils::Date date(date_string);

    std::string sql_select = R"sql(
SELECT band_name FROM approximated_data WHERE year=? AND month=? AND day=? AND spatial=1
)sql";
    SQLite::Statement stmt_select(db, sql_select);
    date.bind_sql(stmt_select, 1);

    std::vector<std::string> bands;
    while (stmt_select.executeStep()) {
        bands.push_back(stmt_select.getColumn(0));
    }

    return bands;
}

SQLite::Statement DataBase::prepare_stmt(
    std::string sql_string,
    utils::Indices index,
    f64 threshold,
    int start_year,
    int end_year,
    DataChoices choice)
{

    auto index_name = magic_enum::enum_name(index);
    threshold = std::round(threshold * 100.0) / 100.0;

    SQLite::Statement stmt_to_prepare(db, sql_string);
    stmt_to_prepare.bind(1, index_name.data());
    stmt_to_prepare.bind(2, threshold);
    stmt_to_prepare.bind(3, start_year);
    stmt_to_prepare.bind(4, end_year);
    std::visit(Visitor {
                   [&](UseApproximatedData) {
                       stmt_to_prepare.bind(5, (int)true);
                       stmt_to_prepare.bind(6, (int)false);
                       stmt_to_prepare.bind(7, (int)false);
                   },
                   [&](UseRealData choice) {
                       stmt_to_prepare.bind(5, (int)false);
                       stmt_to_prepare.bind(6, (int)choice.exclude_cloudy_pixels);
                       stmt_to_prepare.bind(7, (int)choice.exclude_shadow_pixels);
                   } },
        choice);

    return stmt_to_prepare;
}

void DataBase::create_sis_table()
{
    std::string sql_create = R"sql(
CREATE TABLE IF NOT EXISTS single_image_summary(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    index_name TEXT,
    threshold REAL,
    start_year INTEGER,
    end_year INTEGER,
    use_approximated_data INTEGER,
    exclude_cloudy_pixels INTEGER,
    exclude_shadow_pixels INTEGER,
    min REAL,
    max REAL,
    mean REAL,
    num_days_used INTEGER);
)sql";
    SQLite::Statement statement(db, sql_create);
    statement.exec();
}

std::optional<int> DataBase::result_exists(
    utils::Indices index,
    f64 threshold,
    int start_year,
    int end_year,
    DataChoices choice)
{
    create_sis_table();

    std::string sql_select = R"sql(
SELECT id FROM single_image_summary
WHERE index_name=? AND threshold=? AND start_year=? AND end_year=? AND use_approximated_data=? AND exclude_cloudy_pixels=? AND exclude_shadow_pixels=?;
)sql";
    SQLite::Statement stmt_select = prepare_stmt(sql_select, index, threshold, start_year, end_year, choice);
    while (stmt_select.executeStep()) {
        return stmt_select.getColumn(0);
    }
    return {};
}

int DataBase::save_result_in_table(
    utils::Indices index,
    f64 threshold,
    int start_year,
    int end_year,
    DataChoices choice,
    f64 min,
    f64 max,
    f64 mean,
    int num_days_used)
{
    create_sis_table();

    std::string sql_insert = R"sql(
INSERT INTO single_image_summary (index_name, threshold, start_year, end_year, use_approximated_data, exclude_cloudy_pixels, exclude_shadow_pixels, min, max, mean, num_days_used)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
RETURNING id;
)sql";
    SQLite::Transaction transaction(db);
    SQLite::Statement stmt_insert = prepare_stmt(sql_insert, index, threshold, start_year, end_year, choice);
    stmt_insert.bind(8, min);
    stmt_insert.bind(9, max);
    stmt_insert.bind(10, mean);
    stmt_insert.bind(11, num_days_used);

    stmt_insert.executeStep();
    int result = stmt_insert.getColumn(0);
    transaction.commit();

    return result;
}

SQLite::Statement DataBase::index_table_helper(std::string const& date_string, utils::Indices index, f64 min, f64 max, f64 mean, int num_elements, bool use_approx_data)
{
    std::string sql_string = R"sql(
CREATE TABLE IF NOT EXISTS index_data(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    index_name STRING,
    using_approximated_data INTEGER,
    exclude_cloudy_pixels INTEGER,
    exclude_shadow_pixels INTEGER,
    min REAL,
    max REAL,
    mean REAL,
    num_elements_used INTEGER,
    year INTEGER NOT NULL,
    month INTEGER NOT NULL,
    day INTEGER NOT NULL,
    FOREIGN KEY(year, month, day) REFERENCES dates(year, month, day));
)sql";
    {
        SQLite::Transaction transaction(db);
        db.exec(sql_string);
        transaction.commit();
    }

    sql_string = R"sql(
INSERT INTO index_data (index_name, using_approximated_data, min, max, mean, year, month, day, num_elements_used, exclude_cloudy_pixels, exclude_shadow_pixels)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)sql";
    SQLite::Statement stmt(db, sql_string);
    auto index_name = magic_enum::enum_name(index);
    utils::Date date(date_string);
    stmt.bind(1, index_name.data());
    stmt.bind(2, (int)use_approx_data);
    stmt.bind(3, min);
    stmt.bind(4, max);
    stmt.bind(5, mean);
    int idx = date.bind_sql(stmt, 6);
    stmt.bind(idx, num_elements);

    return stmt;
}

void DataBase::store_index_info(std::string const& date_string, utils::Indices index, MatX<f64> const& index_matrix, MatX<bool> const& valid_pixels, UseRealData choice)
{
    MatX<f64> masked_index = valid_pixels.select(index_matrix, MatX<f64>::Constant(index_matrix.rows(), index_matrix.cols(), NO_DATA_INDICATOR));
    VecX<f64> selected_elements = selectMatrixElements(masked_index, NO_DATA_INDICATOR);
    if (selected_elements.size() == 0) {
        return;
    }

    int idx = 10;
    SQLite::Statement stmt_insert = index_table_helper(date_string, index, selected_elements.minCoeff(), selected_elements.maxCoeff(), selected_elements.mean(), selected_elements.size(), false);
    stmt_insert.bind(idx, choice.exclude_cloudy_pixels);
    stmt_insert.bind(idx + 1, choice.exclude_shadow_pixels);

    stmt_insert.exec();
}

void DataBase::store_index_info(std::string const& date_string, utils::Indices index, MatX<f64> const& index_matrix, DataChoices choice)
{
    bool use_approx_data;
    std::visit(Visitor {
                   [&](UseApproximatedData) {
                       use_approx_data = true;
                   },
                   [&](UseRealData) {
                       use_approx_data = false;
                   } },
        choice);
    int idx = 10;
    SQLite::Statement stmt_insert = index_table_helper(date_string, index, index_matrix.minCoeff(), index_matrix.maxCoeff(), index_matrix.mean(), index_matrix.size(), use_approx_data);
    stmt_insert.bind(idx, (bool)false);
    stmt_insert.bind(idx + 1, (bool)false);

    stmt_insert.exec();
}
}
