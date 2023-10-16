#include "approx/db.h"
#include "approx/laplace.h"
#include "utils/db.h"
#include "utils/error.h"
#include "utils/fmt_filesystem.h"
#include "utils/log.h"

#include <magic_enum.hpp>
#include <sqlite3.h>

namespace approx {
static auto logger = utils::create_logger("approx");

f64 DayInfo::distance(date_time::date const& other, f64 weight, bool use_denoised_data) const
{
    auto num_days = (f64)std::abs((other - date).days());
    return weight * num_days + (1 - weight) * (use_denoised_data ? percent_invalid_noise_removed : percent_invalid);
}

DataBase::DataBase(fs::path base_path)
    : utils::DataBase(std::move(base_path))
{
}

void DataBase::create_approx_table()
{
    std::string sql = R"sql(
CREATE TABLE IF NOT EXISTS approximated_data(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    band_name TEXT,
    method TEXT,
    using_denoised INTEGER,
    year INTEGER NOT NULL,
    month INTEGER NOT NULL,
    day INTEGER NOT NULL,
    FOREIGN KEY(year, month, day) REFERENCES dates(year, month, day));
)sql";
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        throw utils::DBError("Failed to create approximated_data table", rc, *logger);
    }
}

int DataBase::write_approx_results(std::string const& date_string, std::string const& band_name, ApproxMethod method, bool using_denoised)
{
    create_approx_table();

    auto method_string = magic_enum::enum_name(method);
    utils::Date date(date_string);

    std::string sql_string = R"sql(
INSERT OR REPLACE INTO approximated_data (band_name, method, using_denoised, year, month, day)
VALUES(?, ?, ?, ?, ?, ?)
RETURNING id;
)sql";
    if (stmt_insert == nullptr) {
        stmt_insert = std::make_unique<utils::StmtWrapper>(db, sql_string);
    }
    sqlite3_bind_text(stmt_insert->stmt, 1, band_name.c_str(), (int)band_name.length(), SQLITE_STATIC);
    sqlite3_bind_text(stmt_insert->stmt, 2, method_string.data(), (int)method_string.length(), SQLITE_STATIC);
    sqlite3_bind_int(stmt_insert->stmt, 3, (int)using_denoised);
    date.bind_sql(stmt_insert->stmt, 4);

    int rc = sqlite3_step(stmt_insert->stmt);
    int id;
    if (rc == SQLITE_ROW) {
        id = sqlite3_column_int(stmt_insert->stmt, 0);
    } else {
        throw utils::DBError("Failed to insert data into approximated data table", rc, *logger);
    }

    sqlite3_reset(stmt_insert->stmt);
    return id;
}

std::unordered_map<std::string, int> DataBase::get_approx_status(std::string const& date_string, ApproxMethod method, bool using_denoised)
{
    // In case we try and run this function before creating the approximation table
    create_approx_table();

    utils::Date date(date_string);
    std::string sql_string = R"sql(
SELECT id, band_name
FROM approximated_data
WHERE method = ? AND using_denoised = ? AND year = ? AND month = ? AND day = ?;
)sql";
    if (stmt_select == nullptr) {
        stmt_select = std::make_unique<utils::StmtWrapper>(db, sql_string);
    }

    auto method_string = magic_enum::enum_name(method);
    sqlite3_bind_text(stmt_select->stmt, 1, method_string.data(), (int)method_string.length(), SQLITE_STATIC);
    sqlite3_bind_int(stmt_select->stmt, 2, (int)using_denoised);
    date.bind_sql(stmt_select->stmt, 3);

    std::unordered_map<std::string, int> output;
    while (sqlite3_step(stmt_select->stmt) == SQLITE_ROW) {

        int id = sqlite3_column_int(stmt_select->stmt, 0);
        std::string name = reinterpret_cast<char const*>(sqlite3_column_text(stmt_select->stmt, 1));
        output.emplace(name, id);
    }

    sqlite3_reset(stmt_select->stmt);

    return output;
}

std::vector<DayInfo> DataBase::select_close_images(std::string const& date_string)
{
    auto date = date_time::from_simple_string(date_string);
    auto next_month = date + date_time::months(1);
    auto previous_month = date - date_time::months(1);

    std::vector<DayInfo> return_value;

    std::string sql_string = R"sql(
SELECT year, month, day, percent_invalid, percent_invalid_noise_removed, threshold_used_for_noise_removal
FROM dates WHERE
    (year = ? OR year = ? OR year = ?) AND
    (month = ? OR month = ? OR month = ?) AND NOT
    (year = ? AND month = ? AND day = ?)
    ORDER BY year, month, day
)sql";
    {
        utils::StmtWrapper stmt(db, sql_string);
        sqlite3_bind_int(stmt.stmt, 1, date.year());
        sqlite3_bind_int(stmt.stmt, 2, next_month.year());
        sqlite3_bind_int(stmt.stmt, 3, previous_month.year());
        sqlite3_bind_int(stmt.stmt, 4, date.month());
        sqlite3_bind_int(stmt.stmt, 5, next_month.month());
        sqlite3_bind_int(stmt.stmt, 6, previous_month.month());
        sqlite3_bind_int(stmt.stmt, 7, date.year());
        sqlite3_bind_int(stmt.stmt, 8, date.month());
        sqlite3_bind_int(stmt.stmt, 9, date.day());

        while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            DayInfo info;
            int year = sqlite3_column_int(stmt.stmt, 0);
            int month = sqlite3_column_int(stmt.stmt, 1);
            int day = sqlite3_column_int(stmt.stmt, 2);
            info.date = date_time::date(year, month, day);

            info.percent_invalid = sqlite3_column_double(stmt.stmt, 3);
            info.percent_invalid_noise_removed = sqlite3_column_double(stmt.stmt, 4);

            return_value.push_back(info);
        }
    }

    return return_value;
}

DayInfo DataBase::select_info_about_date(std::string const& date_string)
{
    auto date = date_time::from_simple_string(date_string);

    DayInfo info;
    std::string sql_string = R"sql(
SELECT percent_invalid, percent_invalid_noise_removed, threshold_used_for_noise_removal
FROM dates WHERE year = ? AND month = ? AND day = ?
    ORDER BY year, month, day
)sql";
    {
        utils::StmtWrapper stmt(db, sql_string);
        sqlite3_bind_int(stmt.stmt, 1, date.year());
        sqlite3_bind_int(stmt.stmt, 2, date.month());
        sqlite3_bind_int(stmt.stmt, 3, date.day());

        while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            info.percent_invalid = sqlite3_column_double(stmt.stmt, 0);
            info.percent_invalid_noise_removed = sqlite3_column_double(stmt.stmt, 1);
        }
    }

    return info;
}
}
