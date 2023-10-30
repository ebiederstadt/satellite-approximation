#include "utils/db.h"
#include "utils/date.h"
#include "utils/error.h"
#include "utils/log.h"

namespace utils {
static auto logger = utils::create_logger("utils::db");

DataBase::DataBase(fs::path base_path)
    : db(base_path / "approximation.db", SQLite::OPEN_CREATE | SQLite::OPEN_READWRITE)
    , stmt(db, "SELECT clouds_computed, shadows_computed, percent_invalid FROM dates WHERE year=? AND month=? AND day=?;")
{
    create_table();
}

CloudShadowStatus DataBase::get_status(std::string const& date_string)
{
    utils::Date date(date_string);
    date.bind_sql(stmt, 1);
    while (stmt.executeStep()) {
        CloudShadowStatus status;
        status.clouds_exist = static_cast<bool>(stmt.getColumn(0).getInt());
        status.shadows_exist = static_cast<bool>(stmt.getColumn(1).getInt());
        status.percent_invalid = stmt.getColumn(2);
        return status;
    }
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
    PRIMARY KEY(year, month, day));
)sql";

    db.exec(sql);
}
}