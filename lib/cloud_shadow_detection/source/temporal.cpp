#include "cloud_shadow_detection/temporal.h"
#include "cloud_shadow_detection/ImageOperations.h"
#include "cloud_shadow_detection/automatic_detection.h"

#include <utils/indices.h>

#include <utils/log.h>

namespace remote_sensing {
static auto logger = utils::create_logger("cloud_shadows::temporal");

Temporal::Temporal(remote_sensing::DataBase& db)
    : db(db)
{
}

static std::string band_name(Band band)
{
    switch (band){
    case Band::NIR:
        return "B08.tif";
    case Band::SWIR:
        return "B11.tif";
    default:
        throw utils::GenericError("Failed to map from band enum to tiff string", *logger);
    }
}

std::vector<TemporalValue> Temporal::band_for_location(fs::path const& base_folder, std::string const& date_string, Band band, givde::LatLng pos, int max_results)
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
        if (!cache.contains(current_date)) {
            cache[current_date] = {};
        }

        // 10,000 converts to bottom of atmosphere reflectance
        constexpr f32 norm_factor = 10000.0f;
        if (!cache[current_date].contains(band)) {
            cache[current_date][band] = utils::GeoTIFF<f32>(base_folder / current_date_string / band_name(band));
            cache[current_date][band].values = ImageOperations::normalize(cache[current_date][band].values, norm_factor);
        }

        if (!cloud_cache.contains(current_date)) {
            cloud_cache[current_date] = utils::GeoTIFF<u8>(base_folder / current_date_string / "cloud_mask.tif");
        }

        TemporalValue value;
        value.value = cache[current_date][band].valueAt(pos);
        value.clouds = cloud_cache[current_date].valueAt(pos);
        value.date = current_date;
        time_series.push_back(value);
        count += 1;
    }

    std::sort(time_series.begin(), time_series.end(), [](TemporalValue const& first, TemporalValue const& second) {
        return first.date < second.date;
    });
    return time_series;
}
}