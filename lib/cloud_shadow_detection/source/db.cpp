#include "cloud_shadow_detection/db.h"

#include <utils/db.h>
#include <utils/eigen.h>
#include <utils/error.h>
#include <utils/filesystem.h>
#include <utils/geotiff.h>
#include <utils/log.h>

namespace remote_sensing {
static auto logger = utils::create_logger("cloud_shadow_detection::db");

DataBase::DataBase(fs::path path)
    : utils::DataBase(std::move(path))
{
}

void DataBase::write_detection_results(std::unordered_map<utils::Date, Status> const& results)
{
    logger->debug("Writing {} results.", results.size());

    std::string sql = R"sql(
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
    for (auto const& [date, status] : results) {
        insert_into_table(date, status);
    }
}

void DataBase::write_detection_result(utils::Date const& date, Status const& status) const
{
    insert_into_table(date, status);
}

void DataBase::insert_into_table(utils::Date const& date, remote_sensing::Status const& status) const
{
    std::string sql = R"sql(
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
    SQLite::Statement stmt(db, sql);

    int index = date.bind_sql(stmt, 1);
    stmt.bind(index, (int)status.clouds_computed);
    stmt.bind(index + 1, (int)status.shadows_computed);
    stmt.bind(index + 2, status.percent_clouds);
    if (status.percent_shadows.has_value())
        stmt.bind(index + 3, *status.percent_shadows);
    else
        stmt.bind(index + 3);
    stmt.bind(index + 4, status.percent_invalid);
    int num = stmt.exec();
    logger->debug("Inserted {} values into db", num);
}

std::vector<CloudStatus> DataBase::find_downloaded_dates()
{
    std::vector<CloudStatus> results;
    std::string sql = "SELECT year, month, day, clouds_computed FROM dates";
    SQLite::Statement stmt(db, sql);
    while (stmt.executeStep()) {
        int year = stmt.getColumn(0);
        int month = stmt.getColumn(1);
        int day = stmt.getColumn(2);
        date_time::date date(year, month, day);
        results.push_back({ date, static_cast<bool>(stmt.getColumn(3).getInt()) });
    }

    return results;
}

std::unordered_map<utils::Date, Status> get_detection_results(fs::path base_folder)
{
    if (!is_directory(base_folder)) {
        logger->warn("Could not process: base folder is not a directory ({})", base_folder);
        return {};
    }

    std::unordered_map<utils::Date, Status> results;

    for (auto const& folder : fs::directory_iterator(base_folder)) {
        if (utils::find_directory_contents(folder) != utils::DirectoryContents::MultiSpectral) {
            continue;
        }

        Status status;
        utils::GeoTIFF<u16> cloud_tiff;
        utils::GeoTIFF<u16> shadow_tiff;

        if (fs::exists(folder / fs::path("cloud_mask.tif"))) {
            try {
                cloud_tiff = utils::GeoTIFF<u16>(folder.path() / "cloud_mask.tif");
                status.clouds_computed = true;
            } catch (std::runtime_error const& e) {
                logger->warn("Failed to open cloud file. Failed with error: {}", e.what());
            }
        }
        if (fs::exists(folder / fs::path("shadow_mask.tif"))) {
            try {
                shadow_tiff = utils::GeoTIFF<u16>(folder / fs::path("shadow_mask.tif"));
                status.shadows_computed = true;
            } catch (std::runtime_error const& e) {
                logger->warn("Failed to open shadow file. Failed with error: {}", e.what());
            }
        }
        if (!(status.clouds_computed || status.shadows_computed)) {
            logger->warn("Could not find mask data. Skipping dir: {}", folder.path());
            continue;
        }

        if (shadow_tiff.values.size() == 0) {
            shadow_tiff.values = MatX<u16>::Zero(cloud_tiff.values.rows(), cloud_tiff.values.cols());
        }
        MatX<bool> mask = cloud_tiff.values.cast<bool>().array() || shadow_tiff.values.cast<bool>().array();
        status.percent_clouds = utils::percent_non_zero(cloud_tiff.values);
        if (status.shadows_computed) {
            status.percent_shadows = utils::percent_non_zero(shadow_tiff.values);
        }
        status.percent_invalid = utils::percent_non_zero(mask);

        results.emplace(folder.path().filename().string(), status);
    }

    return results;
}
}