#include "approx/db.h"
#include "approx/laplace.h"
#include "utils/db.h"
#include "utils/error.h"
#include "utils/fmt_filesystem.h"
#include "utils/log.h"

#include <sqlite3.h>

namespace approx {
static auto logger = utils::create_logger("approx");

f64 DayInfo::distance(date_time::date const& other, f64 weight, bool use_denoised_data) const
{
    auto num_days = (f64)std::abs((other - date).days());
    return weight * num_days + (1 - weight) * (use_denoised_data ? percent_invalid_noise_removed : percent_invalid);
}

DataBase::DataBase(fs::path base_path)
    : db_path(std::move(base_path))
{
    db_path = db_path / "approximation.db";
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        throw utils::DBError("Failed to open db", rc, *logger);
    }
}

DataBase::~DataBase()
{
    int rc = sqlite3_close(db);
    if (rc != SQLITE_OK) {
        throw utils::DBError("Failed to close db", rc, *logger);
    }
}

void DataBase::write_approx_results(std::unordered_map<utils::Date, Status> const& results)
{
    logger->info("writing {} results to the database", db_path);

    std::string sql = R"sql(
CREATE TABLE IF NOT EXISTS dates(
    year INTEGER NOT NULL,
    month INTEGER NOT NULL,
    day INTEGER NOT NULL,
    clouds_computed INTEGER,
    shadows_computed INTEGER,
    percent_cloudy REAL,
    percent_shadows REAL,
    percent_invalid REAL,
    percent_invalid_noise_removed REAL,
    threshold_used_for_noise_removal REAL,
    PRIMARY KEY(year, month, day));
)sql";
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        throw utils::DBError("Failed to create dates table", rc, *logger);
    }

    sql = R"sql(
CREATE TABLE IF NOT EXISTS approximated_data(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    band_name TEXT,
    spatial INTEGER,
    temporal INTEGER,
    year INTEGER NOT NULL,
    month INTEGER NOT NULL,
    day INTEGER NOT NULL,
    FOREIGN KEY(year, month, day) REFERENCES dates(year, month, day));
)sql";
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        throw utils::DBError("Failed to create approximated_data table", rc, *logger);
    }

    for (auto const& [date, status] : results) {
        // Insert the basic data
        sql = R"sql(
INSERT INTO dates (year, month, day, clouds_computed, shadows_computed, percent_cloudy, percent_shadows, percent_invalid)
VALUES(?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(year, month, day) DO
UPDATE SET
    clouds_computed = excluded.clouds_computed,
    shadows_computed = excluded.shadows_computed,
    percent_cloudy = excluded.percent_cloudy,
    percent_shadows = excluded.percent_shadows,
    percent_invalid = excluded.percent_invalid;
)sql";
        {
            utils::StmtWrapper stmt(db, sql);
            int index = date.bind_sql(stmt.stmt, 1);
            sqlite3_bind_int(stmt.stmt, index, (int)status.clouds_computed);
            sqlite3_bind_int(stmt.stmt, index + 1, (int)status.shadows_computed);
            sqlite3_bind_double(stmt.stmt, index + 2, status.percent_clouds);
            if (status.percent_shadows.has_value())
                sqlite3_bind_double(stmt.stmt, index + 3, *status.percent_shadows);
            else
                sqlite3_bind_null(stmt.stmt, index + 3);
            sqlite3_bind_double(stmt.stmt, index + 4, status.percent_invalid);
            rc = sqlite3_step(stmt.stmt);
            if (rc != SQLITE_DONE) {
                throw utils::DBError("First insert failed", rc, *logger);
            }
        }

        for (auto const& band : status.bands_computed) {
            sql = R"sql(
INSERT OR REPLACE INTO approximated_data (band_name, spatial, temporal, year, month, day)
VALUES(?, ?, ?, ?, ?, ?)
)sql";
            utils::StmtWrapper stmt(db, sql);
            sqlite3_bind_text(stmt.stmt, 1, band.c_str(), (int)band.length(), SQLITE_STATIC);
            sqlite3_bind_int(stmt.stmt, 2, (int)true);
            sqlite3_bind_int(stmt.stmt, 3, (int)false);
            date.bind_sql(stmt.stmt, 4);
            rc = sqlite3_step(stmt.stmt);
            if (rc != SQLITE_DONE) {
                throw utils::DBError("Second insert failed", rc, *logger);
            }
        }
    }
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
}
