#pragma once

#include <givde/types.hpp>
#include <utils/date.h>
#include <utils/geotiff.h>

#include "db.h"

using namespace givde;

namespace remote_sensing {
struct TimeSeries {
    std::vector<f32> values;
    std::vector<bool> clouds;
    std::vector<utils::Date> dates;
};

struct CacheData {
    utils::GeoTIFF<u8> clouds;
    utils::GeoTIFF<f32> nir_normalized;
};

// Time Series analysis
class Temporal {
public:
    explicit Temporal(DataBase& db);

    TimeSeries nir_for_location(fs::path const &base_folder, std::string const& date_string, LatLng pos, int max_results=15);

private:
    DataBase& db;
    std::unordered_map<utils::Date, CacheData> cache;
};
}
