#pragma once

#include <boost/date_time/gregorian/gregorian.hpp>
#include <utils/date.h>
#include <utils/db.h>

namespace fs = std::filesystem;
using namespace givde;
namespace date_time = boost::gregorian;

namespace remote_sensing {
struct Status {
    f64 percent_clouds = 0.0;
    std::optional<f64> percent_shadows;
    f64 percent_invalid = 0.0;
    bool clouds_computed = false;
    bool shadows_computed = false;
};

struct CloudStatus {
    date_time::date date;
    bool clouds_computed;

    [[nodiscard]] f64 distance(date_time::date const& other) const
    {
        return static_cast<f64>(std::abs((other - date).days()));
    }
};

class DataBase : utils::DataBase {
public:
    explicit DataBase(fs::path path);
    void write_detection_results(std::unordered_map<utils::Date, Status> const& results);
    void write_detection_result(utils::Date const& date, Status const& status) const;

    std::vector<CloudStatus> find_downloaded_dates();

private:
    void insert_into_table(utils::Date const& date, Status const& status) const;
};

std::unordered_map<utils::Date, Status> get_detection_results(fs::path base_folder);
}