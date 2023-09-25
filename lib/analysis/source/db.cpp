#include "analysis/db.h"
#include "analysis/sis.h"
#include "analysis/utils.h"

#include <fmt/format.h>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

namespace analysis {
DataBase::DataBase(fs::path const& base_path)
{
    fs::path db_path = base_path / fs::path("approximation.db");
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(fmt::format("Failed to open database. Looking for file: {}", db_path.string()));
    }
    sql = "SELECT clouds_computed, shadows_computed FROM dates WHERE date='?';";
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

CloudShadowStatus DataBase::get_status(std::string_view date)
{
    sqlite3_bind_text(stmt, 1, date.data(), (int)date.length(), SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        spdlog::error("Failed to find date of interest: {}", date);
        return {};
    }
    auto cloud_status = (bool)sqlite3_column_int(stmt, 0);
    auto shadow_status = (bool)sqlite3_column_int(stmt, 1);

    return { cloud_status, shadow_status };
}

void DataBase::prepare_stmt(
    sqlite3_stmt* stmt_to_prepare,
    std::string_view sql_string,
    Indices index,
    f64 threshold,
    int start_year,
    int end_year,
    DataChoices choice)
{
    auto index_name = magic_enum::enum_name(index);
    threshold = std::round(threshold * 100.0) / 100.0;

    sqlite3_prepare_v2(db, sql_string.data(), (int)sql_string.length(), &stmt_to_prepare, nullptr);
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
    exclude_shadow_pixels INTEGER);
)sql";
    int rc = sqlite3_exec(db, sql_create.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to create single_image_summary table. Error: " + std::string(sqlite3_errmsg(db)));
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

    sqlite3_stmt* stmt_select;
    std::string sql_select = R"sql(
SELECT id FROM single_image_summary
WHERE index_name='?' AND threshold=? AND start_year=? AND end_year=? AND use_approximated_data=? AND exclude_cloudy_pixels=? AND exclude_shadow_pixels=?;
)sql";
    prepare_stmt(stmt_select, sql_select, index, threshold, start_year, end_year, choice);
    int rc = sqlite3_step(stmt_select);
    std::optional<int> return_value = {};
    if (rc == SQLITE_ROW) {
        return_value = sqlite3_column_int(stmt_select, 0);
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
    DataChoices choice)
{
    create_sis_table();

    std::string sql_insert = R"sql(
INSERT INFO single_image_summary(index_name, threshold, start_year, end_year, use_approximated_dta, exclude_cloudy_pixels, exclude_shadow_pixels)
VALUES(?, ?, ?, ?, ?, ?, ?)
RETURNING id;
)sql";
    sqlite3_stmt* stmt_insert;
    prepare_stmt(stmt_insert, sql_insert, index, threshold, start_year, end_year, choice);

    int rc = sqlite3_step(stmt_insert);
    int result = -1;
    if (rc == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt_insert);

    if (rc == SQLITE_ERROR) {
        throw std::runtime_error("Failed to insert data into sis db. Error " + std::string(sqlite3_errmsg(db)));
    }
    return result;
}

}
