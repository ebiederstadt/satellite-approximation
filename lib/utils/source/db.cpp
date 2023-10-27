#include "utils/db.h"
#include "utils/date.h"
#include "utils/error.h"
#include "utils/log.h"

namespace utils {
static auto logger = utils::create_logger("utils::db");

StmtWrapper::StmtWrapper(sqlite3* db, std::string const& sql)
{
    int rc = sqlite3_prepare_v2(db, sql.c_str(), (int)sql.length(), &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->error("Creating statement failed with error: {}", sqlite3_errmsg(db));
        throw utils::DBError("Failed to prepare statement", rc, *logger);
    }
}

StmtWrapper::~StmtWrapper() noexcept(false)
{
    int rc = sqlite3_finalize(stmt);
    if (rc != SQLITE_OK) {
        throw utils::DBError("Failed to finalize prepared statement", rc);
    }
}

StmtWrapper& StmtWrapper::operator=(StmtWrapper&& other) noexcept
{
    stmt = other.stmt;
    return *this;
}

DataBase::DataBase(fs::path base_path)
    : db_path(std::move(base_path))
{
    db_path = db_path / "approximation.db";
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        throw utils::DBError("Failed to open db", rc, *logger);
    }

    create_table();

    std::string sql = R"sql(
SELECT clouds_computed, shadows_computed, percent_invalid, percent_invalid_noise_removed
FROM dates WHERE year=? AND month=? AND day=?;
)sql";
    stmt = std::make_unique<utils::StmtWrapper>(db, sql);
}

DataBase::~DataBase() noexcept(false)
{
    int rc = sqlite3_close(db);
    if (rc != SQLITE_OK) {
        if (rc == SQLITE_ERROR) {
            throw utils::DBError("Failed to close db", rc, *logger);
        } else {
            logger->warn("Non-fatal error occurred when closing db: {}", rc);
        }
    }
}

CloudShadowStatus DataBase::get_status(std::string const& date_string)
{
    utils::Date date(date_string);
    date.bind_sql(stmt->stmt, 1);
    int rc = sqlite3_step(stmt->stmt);
    if (rc != SQLITE_ROW) {
        logger->error(
            "Failed to find date of interest: {}. Ran query: {}, rc={}", date, sqlite3_expanded_sql(stmt->stmt), rc);
        return {};
    }
    CloudShadowStatus status;
    status.clouds_exist = (bool)sqlite3_column_int(stmt->stmt, 0);
    status.shadows_exist = (bool)sqlite3_column_int(stmt->stmt, 1);
    status.percent_invalid = sqlite3_column_double(stmt->stmt, 2);
    status.percent_invalid_denoised = sqlite3_column_double(stmt->stmt, 3);

    sqlite3_reset(stmt->stmt);
    return status;
}

void DataBase::create_table()
{
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
}
}