#include "spatial_approximation/results.h"
#include "spatial_approximation/approx.h"
#include "utils/fmt_filesystem.h"
#include "utils/"

#include <iostream>
#include <spdlog/spdlog.h>
#include <sqlite3.h>

auto logger = spdlog::get("main");

namespace spatial_approximation {
bool write_results_to_db(fs::path const& base_folder, std::unordered_map<std::string, Status> const& results)
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
    date TEXT PRIMARY KEY,
    clouds_computed INTEGER,
    shadows_computed INTEGER,
    percent_cloudy REAL,
    percent_shadows REAL,
    percent_invalid REAL);
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
    date_id STRING,
    FOREIGN KEY(date_id) REFERENCES dates(date));
)sql";
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK)
        goto fail;

    for (auto const& [date, status] : results) {
        // Insert the basic data
        sql = R"sql(
INSERT INTO dates (date, clouds_computed, shadows_computed, percent_cloudy, percent_shadows, percent_invalid)
VALUES(?, ?, ?, ?, ?, ?)
)sql";
        sqlite3_prepare_v2(db, sql.c_str(), (int)sql.length(), &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, date.c_str(), (int)date.length(), SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, (int)status.clouds_computed);
        sqlite3_bind_int(stmt, 3, (int)status.shadows_computed);
        sqlite3_bind_double(stmt, 4, status.percent_clouds);
        if (status.percent_shadows.has_value())
            sqlite3_bind_double(stmt, 5, *status.percent_shadows);
        else
            sqlite3_bind_null(stmt, 5);
        sqlite3_bind_double(stmt, 6, status.percent_invalid);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            logger->error("First INSERT failed: {}", sqlite3_errmsg(db));
            goto fail;
        }
        sqlite3_finalize(stmt);

        for (auto const& band : status.bands_computed) {
            sql = R"sql(
INSERT INTO approximated_data (band_name, spatial, temporal, date_id)
VALUES(?, ?, ?, ?)
)sql";
            sqlite3_prepare_v2(db, sql.c_str(), (int)sql.length(), &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, band.c_str(), (int)band.length(), SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, (int)true);
            sqlite3_bind_int(stmt, 3, (int)false);
            sqlite3_bind_text(stmt, 4, date.c_str(), (int)date.length(), SQLITE_STATIC);
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
