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
    utils::GeoTIFF<f32> index_normalized;
};

// Time Series analysis
class Temporal {
public:
    explicit Temporal(DataBase& db);

    std::vector<TemporalValue> nir_for_location(fs::path const &base_folder, std::string const& date_string, LatLng pos, int max_results=15);
    std::vector<TemporalValue> swir_for_location(fs::path const &base_folder, std::string const& date_string, LatLng pos, int max_results=15);

private:
    DataBase& db;
    std::unordered_map<utils::Date, CacheData> cache;

    std::vector<TemporalValue> index_for_location(fs::path const &base_folder, std::string const& date_string, std::string tiff_name, LatLng pos, int max_results);
};
}
