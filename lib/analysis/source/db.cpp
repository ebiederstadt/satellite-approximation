#include "analysis/db.h"
#include "analysis/sis.h"
#include "analysis/utils.h"

#include <fmt/format.h>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

namespace analysis {
DataBase::DataBase(fs::path const& base_path)
    : logger(spdlog::get("analysis::DB"))
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
SELECT band_name FROM approximated_data WHERE date_id=? AND spatial=1
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

void DataBase::store_index_info(std::string const& date_string, analysis::Indices index, givde::f64 min, givde::f64 max, givde::f64 mean, analysis::DataChoices choice)
{
    std::string sql_string = R"sql(
CREATE TABLE IF NOT EXISTS index_data(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    index_name STRING,
    using_approximated_data INTEGER,
    min REAL,
    max REAL,
    mean REAL,
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
INSERT INTO index_data (index_name, using_approximated_data, min, max, mean, year, month, day)
VALUES(?, ?, ?, ?, ?, ?, ?, ?)
)sql";
    sqlite3_stmt* stmt_insert;
    rc = sqlite3_prepare_v2(db, sql_string.c_str(), (int)sql_string.length(), &stmt_insert, nullptr);
    if (rc == SQLITE_ERROR) {
        throw std::runtime_error("Failed to compile statement. Error: " + std::string(sqlite3_errmsg(db)));
    }
    auto index_name = magic_enum::enum_name(index);
    utils::Date date(date_string);
    sqlite3_bind_text(stmt_insert, 1, index_name.data(), (int)index_name.length(), SQLITE_STATIC);
    std::visit(Visitor {
                   [&](UseApproximatedData) {
                       sqlite3_bind_int(stmt_insert, 2, (int)true);
                   },
                   [&](UseRealData) {
                       sqlite3_bind_int(stmt_insert, 2, (int)false);
                   } },
        choice);
    sqlite3_bind_double(stmt_insert, 3, min);
    sqlite3_bind_double(stmt_insert, 4, max);
    sqlite3_bind_double(stmt_insert, 5, mean);
    date.bind_sql(stmt_insert, 6);

    rc = sqlite3_step(stmt_insert);

    sqlite3_finalize(stmt_insert);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert data into index_data table. Error: " + std::string(sqlite3_errmsg(db)));
    }
}
}
