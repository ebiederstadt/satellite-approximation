#include "approx/db.h"
#include "utils/db.h"
#include "utils/error.h"
#include "utils/log.h"

#include <magic_enum.hpp>
#include <sqlite3.h>

namespace approx {
static auto logger = utils::create_logger("approx");

f64 DayInfo::distance(date_time::date const& other, f64 weight) const
{
    auto num_days = (f64)std::abs((other - date).days());
    return weight * num_days + (1 - weight) * percent_invalid;
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
    year INTEGER NOT NULL,
    month INTEGER NOT NULL,
    day INTEGER NOT NULL,
    FOREIGN KEY(year, month, day) REFERENCES dates(year, month, day));
)sql";
    SQLite::Statement stmt(db, sql);
    stmt.exec();
}

int DataBase::write_approx_results(std::string const& date_string, std::string const& band_name, ApproxMethod method)
{
    create_approx_table();

    auto method_string = magic_enum::enum_name(method);
    utils::Date date(date_string);

    std::string sql_string = R"sql(
INSERT OR REPLACE INTO approximated_data (band_name, method, year, month, day)
VALUES(?, ?, ?, ?, ?)
RETURNING id;
)sql";
    if (stmt_insert == nullptr) {
        stmt_insert = std::make_unique<SQLite::Statement>(db, sql_string);
    }
    stmt_insert->bind(1, band_name);
    stmt_insert->bind(2, method_string.data());
    date.bind_sql(*stmt_insert, 4);

    while (stmt_insert->executeStep()) {
        return stmt_insert->getColumn(0);
    }
}

std::unordered_map<std::string, int> DataBase::get_approx_status(std::string const& date_string, ApproxMethod method)
{
    // In case we try and run this function before creating the approximation table
    create_approx_table();

    utils::Date date(date_string);
    std::string sql_string = R"sql(
SELECT id, band_name
FROM approximated_data
WHERE method = ? AND year = ? AND month = ? AND day = ?;
)sql";
    if (stmt_select == nullptr) {
        stmt_select = std::make_unique<SQLite::Statement>(db, sql_string);
    }

    auto method_string = magic_enum::enum_name(method);
    stmt_select->bind(1, method_string.data());
    date.bind_sql(*stmt_select, 3);

    std::unordered_map<std::string, int> output;
    while (stmt_select->executeStep()) {
        int id = stmt_select->getColumn(0);
        std::string name = stmt_select->getColumn(1);
        output.emplace(name, id);
    }

    return output;
}

std::vector<DayInfo> DataBase::select_close_images(std::string const& date_string)
{
    auto date = date_time::from_simple_string(date_string);
    auto next_month = date + date_time::months(1);
    auto previous_month = date - date_time::months(1);

    std::vector<DayInfo> return_value;

    std::string sql_string = R"sql(
SELECT year, month, day, percent_invalid
FROM dates WHERE
    (year = ? OR year = ? OR year = ?) AND
    (month = ? OR month = ? OR month = ?) AND NOT
    (year = ? AND month = ? AND day = ?)
    ORDER BY year, month, day
)sql";
    {
        SQLite::Statement stmt(db, sql_string);
        stmt.bind(1, date.year());
        stmt.bind(2, next_month.year());
        stmt.bind(3, previous_month.year());
        stmt.bind(4, date.month());
        stmt.bind(5, next_month.month());
        stmt.bind(6, previous_month.month());
        stmt.bind(7, date.year());
        stmt.bind(8, date.month());
        stmt.bind(9, date.day());

        while (stmt.executeStep()) {
            DayInfo info;
            int year = stmt.getColumn(0);
            int month = stmt.getColumn(1);
            int day = stmt.getColumn(2);
            info.date = date_time::date(year, month, day);
            info.percent_invalid = stmt.getColumn(3);

            return_value.push_back(info);
        }
    }

    return return_value;
}

DayInfo DataBase::select_info_about_date(std::string const& date_string)
{
    utils::Date date(date_string);

    DayInfo info;
    std::string sql_string = R"sql(
SELECT percent_invalid
FROM dates WHERE year = ? AND month = ? AND day = ?
    ORDER BY year, month, day
)sql";
    {
        SQLite::Statement stmt(db, sql_string);
        date.bind_sql(stmt, 1);

        while (stmt.executeStep()) {
            info.percent_invalid = stmt.getColumn(0);
        }
    }

    return info;
}
}
