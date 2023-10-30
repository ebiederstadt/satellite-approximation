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

TimeSeries Temporal::nir_for_location(fs::path const& base_folder, std::string const& date_string, givde::LatLng pos)
{
    auto downloaded_dates = db.find_downloaded_dates();
    date_time::date date = date_time::from_simple_string(date_string);

    // Sort the days according to their distance from the current date
    std::sort(downloaded_dates.begin(), downloaded_dates.end(), [&date](CloudStatus const& first, CloudStatus const& second) {
        return first.distance(date) < second.distance(date);
    });

    // Try to find at least 15 non-cloudy dates
    TimeSeries time_series;
    int count = 0;
    int total_desired = 15;
    for (auto &downloaded_date : downloaded_dates) {
        if (count >= total_desired) {
            break;
        }

        if (!downloaded_date.clouds_computed) {
            detect_clouds(base_folder / date_time::to_iso_extended_string(downloaded_date.date), db);
        }

        utils::Date current_date(downloaded_date.date);
        if (cache.contains(current_date)) {
            time_series.values.push_back(cache.at(current_date).nir_normalized.valueAt(pos));
            time_series.dates.push_back(current_date);
            auto cloudy = static_cast<bool>(cache.at(current_date).clouds.valueAt(pos));
            time_series.clouds.push_back(cloudy);
            if (!cloudy) {
                count += 1;
            }
            continue;
        }

        // Try to find at least 15 non-cloudy-samples
        CacheData data;
        data.clouds = utils::GeoTIFF<u8>(base_folder / date_string / "cloud_mask.tif");
        data.nir_normalized = utils::GeoTIFF<f32>(base_folder / date_string / "B08.tif");
        data.nir_normalized.values = ImageOperations::normalize(data.nir_normalized.values, std::numeric_limits<u16>::max());

        cache.emplace(current_date, data);

        time_series.values.push_back(data.nir_normalized.valueAt(pos));
        time_series.dates.push_back(current_date);
        bool cloudy = static_cast<bool>(data.clouds.valueAt(pos));
        time_series.clouds.push_back(cloudy);
        if (!cloudy) {
            count += 1;
        }
    }

    return time_series;
}
}