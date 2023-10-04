#include "approx/results.h"
#include "approx/laplace.h"
#include "utils/fmt_filesystem.h"
#include "utils/log.h"

#include <iostream>
#include <sqlite3.h>

auto logger = utils::create_logger("approx");

namespace approx {
bool write_results_to_db(fs::path const& base_folder, std::unordered_map<utils::Date, Status> const& results)
{
    logger->info("writing {} results to the database", base_folder);
    fs::path db_path = base_folder / fs::path("approximation.db");
    sqlite3* db;

    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        logger->error("Failed to open db: {}", db_path);
        return false;
    }
    logger->debug("Opened file: {}", db_path);
    char* err_msg = nullptr;
    sqlite3_stmt* stmt;

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
    PRIMARY KEY(year, month, day));
)sql";
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK)
        goto fail;

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
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK)
        goto fail;

    for (auto const& [date, status] : results) {
        // Insert the basic data
        sql = R"sql(
INSERT OR REPLACE INTO dates (year, month, day, clouds_computed, shadows_computed, percent_cloudy, percent_shadows, percent_invalid)
VALUES(?, ?, ?, ?, ?, ?, ?, ?)
)sql";
        sqlite3_prepare_v2(db, sql.c_str(), (int)sql.length(), &stmt, nullptr);
        int index = date.bind_sql(stmt, 1);
        sqlite3_bind_int(stmt, index, (int)status.clouds_computed);
        sqlite3_bind_int(stmt, index + 1, (int)status.shadows_computed);
        sqlite3_bind_double(stmt, index + 2, status.percent_clouds);
        if (status.percent_shadows.has_value())
            sqlite3_bind_double(stmt, index + 3, *status.percent_shadows);
        else
            sqlite3_bind_null(stmt, index + 3);
        sqlite3_bind_double(stmt, index + 4, status.percent_invalid);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            logger->error("First INSERT failed: {} (Error code: {})", sqlite3_errmsg(db), rc);
            goto fail;
        }
        sqlite3_finalize(stmt);

        for (auto const& band : status.bands_computed) {
            sql = R"sql(
INSERT OR REPLACE INTO approximated_data (band_name, spatial, temporal, year, month, day)
VALUES(?, ?, ?, ?, ?, ?)
)sql";
            sqlite3_prepare_v2(db, sql.c_str(), (int)sql.length(), &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, band.c_str(), (int)band.length(), SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, (int)true);
            sqlite3_bind_int(stmt, 3, (int)false);
            date.bind_sql(stmt, 4);
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger->error("Second INSERT failed: {}", sqlite3_errmsg(db));
                goto fail;
            }
            sqlite3_finalize(stmt);
        }
    }

    sqlite3_close(db);
    return true;

fail:
    if (err_msg != nullptr) {
        logger->error("SQL Error: {}", err_msg);
        sqlite3_free(err_msg);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return false;
}
}
