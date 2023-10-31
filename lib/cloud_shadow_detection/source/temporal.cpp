#include "cloud_shadow_detection/temporal.h"
#include "cloud_shadow_detection/ImageOperations.h"
#include "cloud_shadow_detection/automatic_detection.h"

#include <utils/log.h>

namespace remote_sensing {
static auto logger = utils::create_logger("cloud_shadows::temporal");

Temporal::Temporal(remote_sensing::DataBase& db)
    : db(db)
{
}

std::vector<TemporalValue> Temporal::nir_for_location(fs::path const& base_folder, std::string const& date_string, givde::LatLng pos, int max_results)
{
    return index_for_location(base_folder, date_string, Index::NIR, pos, max_results);
}

std::vector<TemporalValue> Temporal::swir_for_location(fs::path const& base_folder, std::string const& date_string, LatLng pos, int max_results)
{
    return index_for_location(base_folder, date_string, Index::SWIR, pos, max_results);
}

std::vector<TemporalValue> Temporal::index_for_location(fs::path const& base_folder, std::string const& date_string, Index index, LatLng pos, int max_results)
{
    auto downloaded_dates = db.find_downloaded_dates();
    date_time::date date = date_time::from_simple_string(date_string);

    // Sort the days according to their distance from the current date
    std::sort(downloaded_dates.begin(), downloaded_dates.end(), [&date](CloudStatus const& first, CloudStatus const& second) {
        return first.distance(date) < second.distance(date);
    });

    std::vector<TemporalValue> time_series;
    int count = 0;
    for (auto& downloaded_date : downloaded_dates) {
        if (count >= max_results) {
            break;
        }

        std::string current_date_string = date_time::to_iso_extended_string(downloaded_date.date);
        if (!downloaded_date.clouds_computed) {
            detect_clouds(base_folder / current_date_string, db);
        }

        utils::Date current_date(downloaded_date.date);
        if (cache.contains(current_date)) {
            TemporalValue value;
            if (index == Index::NIR) {
                value.value = cache.at(current_date).nir_normalized.valueAt(pos);
            } else if (index == Index::SWIR) {
                value.value = cache.at(current_date).swir_normalized.valueAt(pos);
            } else {
                throw utils::GenericError("Failed to map index name to known value", *logger);
            }
            value.date = current_date;
            value.clouds = cache.at(current_date).clouds.valueAt(pos);
            time_series.push_back(value);
            count += 1;
            continue;
        }

        CacheData data;
        data.clouds = utils::GeoTIFF<u8>(base_folder / current_date_string / "cloud_mask.tif");
        data.nir_normalized = utils::GeoTIFF<f32>(base_folder / current_date_string / "B08.tif");
        data.swir_normalized = utils::GeoTIFF<f32>(base_folder / current_date_string / "B11.tif");
        // 10,000 converts to bottom of atmosphere reflectance
        constexpr f32 norm_factor = 10000.0f;
        data.nir_normalized.values = ImageOperations::normalize(data.nir_normalized.values, norm_factor);
        data.swir_normalized.values = ImageOperations::normalize(data.swir_normalized.values, norm_factor);

        cache.emplace(current_date, data);

        TemporalValue value;
        if (index == Index::NIR) {
            value.value = data.nir_normalized.valueAt(pos);
        } else if (index == Index::SWIR) {
            value.value = data.swir_normalized.valueAt(pos);
        } else {
            throw utils::GenericError("Failed to map index name to known value", *logger);
        }
        value.date = current_date;
        bool cloudy = static_cast<bool>(data.clouds.valueAt(pos));
        value.clouds = cloudy;
        time_series.push_back(value);
        count += 1;
    }

    std::sort(time_series.begin(), time_series.end(), [](TemporalValue const& first, TemporalValue const& second) {
        return first.date < second.date;
    });
    return time_series;
}
}