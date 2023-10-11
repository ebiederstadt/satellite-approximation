#include "analysis/db.h"
#include "analysis/sis.h"
#include "analysis/utils.h"

#include <fmt/format.h>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

namespace analysis {
DataBase::DataBase(fs::path const& base_path)
    : logger(utils::create_logger("analysis::DB"))
{
    fs::path db_path = base_path / fs::path("approximation.db");
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(fmt::format("Failed to open database. Looking for file: {}", db_path.string()));
    }
    sql = "SELECT clouds_computed, shadows_computed, percent_invalid FROM dates WHERE year=? AND month=? AND day=?;";
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)sql.length(), &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to compile SQL. Error: " + std::string(sqlite3_errmsg(db)));
    }
}

DataBase::~DataBase()
{
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

std::vector<std::string> DataBase::get_approximated_data(std::string const& date_string)
{
    utils::Date date(date_string);

    std::string sql_select = R"sql(
SELECT band_name FROM approximated_data WHERE year=? AND month=? AND day=? AND spatial=1
)sql";
    sqlite3_stmt* stmt_select;
    int rc = sqlite3_prepare_v2(db, sql_select.c_str(), (int)sql_select.length(), &stmt_select, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to compile SQL. Error: " + std::string(sqlite3_errmsg(db)));
    }
    date.bind_sql(stmt_select, 1);

    std::vector<std::string> bands;
    while (sqlite3_step(stmt_select) == SQLITE_ROW) {
        bands.push_back(reinterpret_cast<char const*>(sqlite3_column_text(stmt_select, 0)));
    }

    return bands;
}

CloudShadowStatus DataBase::get_status(std::string date_string)
{
    utils::Date date(date_string);
    date.bind_sql(stmt, 1);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        logger->error(
            "Failed to find date of interest: {}. Ran query: {}, rc={}", date, sqlite3_expanded_sql(stmt), rc);
        return {};
    }
    auto cloud_status = (bool)sqlite3_column_int(stmt, 0);
    auto shadow_status = (bool)sqlite3_column_int(stmt, 1);
    auto percent_invalid = sqlite3_column_double(stmt, 2);

    sqlite3_reset(stmt);
    return { cloud_status, shadow_status, percent_invalid };
}

sqlite3_stmt* DataBase::prepare_stmt(
    std::string sql_string,
    Indices index,
    f64 threshold,
    int start_year,
    int end_year,
    DataChoices choice)
{
    sqlite3_stmt* stmt_to_prepare = nullptr;

    auto index_name = magic_enum::enum_name(index);
    threshold = std::round(threshold * 100.0) / 100.0;

    int rc = sqlite3_prepare_v2(db, sql_string.c_str(), (int)sql_string.length(), &stmt_to_prepare, nullptr);
    if (rc == SQLITE_ERROR) {
        throw std::runtime_error("Failed to compile statement. Error: " + std::string(sqlite3_errmsg(db)));
    }
    sqlite3_bind_text(stmt_to_prepare, 1, index_name.data(), (int)index_name.length(), SQLITE_STATIC);
    sqlite3_bind_double(stmt_to_prepare, 2, threshold);
    sqlite3_bind_int(stmt_to_prepare, 3, start_year);
    sqlite3_bind_int(stmt_to_prepare, 4, end_year);
    std::visit(Visitor {
                   [&](UseApproximatedData) {
                       sqlite3_bind_int(stmt_to_prepare, 5, (int)true);
                       sqlite3_bind_int(stmt_to_prepare, 6, (int)false);
                       sqlite3_bind_int(stmt_to_prepare, 7, (int)false);
                   },
                   [&](UseRealData choice) {
                       sqlite3_bind_int(stmt_to_prepare, 5, (int)false);
                       sqlite3_bind_int(stmt_to_prepare, 6, (int)choice.exclude_cloudy_pixels);
                       sqlite3_bind_int(stmt_to_prepare, 7, (int)choice.exclude_shadow_pixels);
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
    int rc = sqlite3_exec(db, sql_create.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(
            "Failed to create single_image_summary table. Error: " + std::string(sqlite3_errmsg(db)));
    }
}

std::optional<int> DataBase::result_exists(
    Indices index,
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
    sqlite3_stmt* stmt_select = prepare_stmt(sql_select, index, threshold, start_year, end_year, choice);
    int rc = sqlite3_step(stmt_select);
    std::optional<int> return_value = {};
    if (rc == SQLITE_ROW) {
        return_value = sqlite3_column_int(stmt_select, 0);
    } else if (rc != SQLITE_ERROR) {
        return {};
    }

    sqlite3_finalize(stmt_select);
    if (rc == SQLITE_ERROR) {
        throw std::runtime_error("Failed to select rows from single image summary table. Error: "
            + std::string(sqlite3_errmsg(db)));
    }
    return return_value;
}

int DataBase::save_result_in_table(
    Indices index,
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
    sqlite3_stmt* stmt_insert = prepare_stmt(sql_insert, index, threshold, start_year, end_year, choice);
    sqlite3_bind_double(stmt_insert, 8, min);
    sqlite3_bind_double(stmt_insert, 9, max);
    sqlite3_bind_double(stmt_insert, 10, mean);
    sqlite3_bind_int(stmt_insert, 11, num_days_used);

    int rc = sqlite3_step(stmt_insert);
    int result = -1;
    if (rc == SQLITE_ROW) {
        result = sqlite3_column_int(stmt_insert, 0);
    } else if (rc != SQLITE_ERROR) {
        logger->error("Failed to insert into db. rc={}", rc);
    }
    sqlite3_finalize(stmt_insert);

    if (rc == SQLITE_ERROR) {
        throw std::runtime_error("Failed to insert data into sis db. Error " + std::string(sqlite3_errmsg(db)));
    }
    return result;
}

int DataBase::index_table_helper(std::string const& date_string, Indices index, f64 min, f64 max, f64 mean, int num_elements, bool use_approx_data, sqlite3_stmt** stmt_insert)
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
    int rc = sqlite3_exec(db, sql_string.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(
            "Failed to create index_data table. Error: " + std::string(sqlite3_errmsg(db)));
    }

    sql_string = R"sql(
INSERT INTO index_data (index_name, using_approximated_data, min, max, mean, year, month, day, num_elements_used, exclude_cloudy_pixels, exclude_shadow_pixels)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)sql";
    rc = sqlite3_prepare_v2(db, sql_string.c_str(), (int)sql_string.length(), &(*stmt_insert), nullptr);
    if (rc == SQLITE_ERROR) {
        throw std::runtime_error("Failed to compile statement. Error: " + std::string(sqlite3_errmsg(db)));
    }
    auto index_name = magic_enum::enum_name(index);
    utils::Date date(date_string);
    sqlite3_bind_text(*stmt_insert, 1, index_name.data(), (int)index_name.length(), SQLITE_STATIC);
    sqlite3_bind_int(*stmt_insert, 2, (int)use_approx_data);
    sqlite3_bind_double(*stmt_insert, 3, min);
    sqlite3_bind_double(*stmt_insert, 4, max);
    sqlite3_bind_double(*stmt_insert, 5, mean);
    int idx = date.bind_sql(*stmt_insert, 6);
    sqlite3_bind_int(*stmt_insert, idx, num_elements);

    return idx + 1;
}

void DataBase::store_index_info(std::string const& date_string, Indices index, MatX<f64> const& index_matrix, MatX<bool> const& valid_pixels, UseRealData choice)
{
    MatX<f64> masked_index = valid_pixels.select(index_matrix, MatX<f64>::Constant(index_matrix.rows(), index_matrix.cols(), NO_DATA_INDICATOR));
    VecX<f64> selected_elements = selectMatrixElements(masked_index, NO_DATA_INDICATOR);
    if (selected_elements.size() == 0) {
        return;
    }

    sqlite3_stmt* stmt_insert = nullptr;
    int idx = index_table_helper(date_string, index, selected_elements.minCoeff(), selected_elements.maxCoeff(), selected_elements.mean(), selected_elements.size(), false, &stmt_insert);
    sqlite3_bind_int(stmt_insert, idx, choice.exclude_cloudy_pixels);
    sqlite3_bind_int(stmt_insert, idx + 1, choice.exclude_shadow_pixels);

    int rc = sqlite3_step(stmt_insert);

    sqlite3_finalize(stmt_insert);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error(fmt::format("Failed to insert data into index_data table. Error: {}, Error code: {}", sqlite3_errmsg(db), rc));
    }
}

void DataBase::store_index_info(std::string const& date_string, analysis::Indices index, MatX<f64> const& index_matrix, DataChoices choice)
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
    sqlite3_stmt* stmt_insert = nullptr;
    int idx = index_table_helper(date_string, index, index_matrix.minCoeff(), index_matrix.maxCoeff(), index_matrix.mean(), index_matrix.size(), use_approx_data, &stmt_insert);
    sqlite3_bind_int(stmt_insert, idx, (bool)false);
    sqlite3_bind_int(stmt_insert, idx + 1, (bool)false);

    int rc = sqlite3_step(stmt_insert);

    sqlite3_finalize(stmt_insert);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error(fmt::format("Failed to insert data into index_data table. Error: {}, Error code: {}", sqlite3_errmsg(db), rc));
    }
}

void DataBase::save_noise_removal(std::string const& date_string, givde::f64 percent_invalid, int threshold)
{
    utils::Date date(date_string);

    sqlite3_stmt* stmt_insert = nullptr;
    std::string sql_string = R"sql(
INSERT OR REPLACE INTO dates (year, month, day, percent_invalid_noise_removed, threshold_used_for_noise_removal)
VALUES(?, ?, ?, ?, ?);)sql";
    sqlite3_prepare_v2(db, sql_string.c_str(), (int)sql_string.length(), &stmt_insert, nullptr);
    int index = date.bind_sql(stmt_insert, 1);
    sqlite3_bind_double(stmt_insert, index, percent_invalid);
    sqlite3_bind_int(stmt_insert, index + 1, threshold);
    int rc = sqlite3_step(stmt_insert);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt_insert);
        throw std::runtime_error(fmt::format("Failed to insert data into dates table. Error: {}, Error code: {}", sqlite3_errmsg(db), rc));
    }
    sqlite3_finalize(stmt_insert);
}

bool DataBase::noise_exists(std::string const& date_string, int threshold)
{
    utils::Date date(date_string);

    sqlite3_stmt* stmt_select;
    std::string sql_string = "SELECT * FROM dates WHERE year = ? AND month = ? AND day = ? AND threshold_used_for_noise_removal = ?";
    sqlite3_prepare_v2(db, sql_string.c_str(), (int)sql_string.length(), &stmt_select, nullptr);
    int index = date.bind_sql(stmt_select, 1);
    sqlite3_bind_int(stmt_select, index, threshold);
    int rc = sqlite3_step(stmt_select);

    if (rc == SQLITE_DONE) {
        return false;
    } else if (rc == SQLITE_ROW) {
        return true;
    } else {
        sqlite3_finalize(stmt_select);
        throw std::runtime_error(fmt::format("Failed to select data from dates table. Error: {}, Error code: {}", sqlite3_errmsg(db), rc));
    }
}
}
