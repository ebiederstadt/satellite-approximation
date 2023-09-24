#include "analysis/db.h"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace analysis {
    DataBase::DataBase(fs::path const &base_path) {
        fs::path db_path = base_path / fs::path("approximation.db");
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            throw std::runtime_error(fmt::format("Failed to open database. Looking for file: {}", db_path.string()));
        }
        sql = "SELECT (clouds_computed, shadows_computed) FROM dates WHERE date='?';";
        sqlite3_prepare_v2(db, sql.c_str(), (int) sql.length(), &stmt, nullptr);
    }

    DataBase::~DataBase() {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    CloudShadowStatus DataBase::get_status(std::string_view date) {
        sqlite3_bind_text(stmt, 1, date.data(), (int) date.length(), SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            spdlog::error("Failed to find date of interest: ", date);
            return {};
        }
        auto cloud_status = (bool) sqlite3_column_int(stmt, 0);
        auto shadow_status = (bool) sqlite3_column_int(stmt, 1);

        return {cloud_status, shadow_status};
    }
}
