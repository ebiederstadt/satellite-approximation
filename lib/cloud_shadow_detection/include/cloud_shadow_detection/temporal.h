#pragma once

#include <givde/types.hpp>
#include <utils/date.h>
#include <utils/geotiff.h>

#include "db.h"

using namespace givde;

namespace remote_sensing {
struct TemporalValue {
    f32 value;
    bool clouds;
    utils::Date date;
};

struct CacheData {
    utils::GeoTIFF<u8> clouds;
    utils::GeoTIFF<f32> nir_normalized;
    utils::GeoTIFF<f32> swir_normalized;
    utils::GeoTIFF<u8> water_test;
};

enum class Band {
    NIR,
    SWIR
};

// Time Series analysis
class Temporal {
public:
    explicit Temporal(DataBase& db);

    std::vector<TemporalValue> band_for_location(fs::path const& base_folder, std::string const& date_string, Band band, LatLng pos, int max_results = 15);

private:
    DataBase& db;
    std::unordered_map<utils::Date, std::unordered_map<Band, utils::GeoTIFF<f32>>> cache;
    std::unordered_map<utils::Date, utils::GeoTIFF<u8>> cloud_cache;
};
}
