#pragma once

#include <filesystem>
#include <unordered_map>
#include <utils/date.h>
#include <utils/noCopying.h>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <givde/types.hpp>

namespace fs = std::filesystem;
namespace date_time = boost::gregorian;
using namespace givde;

namespace approx {
struct Status;

struct DayInfo {
    date_time::date date;
    f64 percent_invalid;
    f64 percent_invalid_noise_removed;

    [[nodiscard]] f64 distance(date_time::date const &other, f64 weight, bool use_denoised_data) const;
};

class DataBase {
    MAKE_NONCOPYABLE(DataBase);
    MAKE_NONMOVABLE(DataBase);

public:
    explicit DataBase(fs::path base_path);
    ~DataBase();

    void write_approx_results(std::unordered_map<utils::Date, Status> const& results);
    std::vector<DayInfo> select_close_images(std::string const &date_string);
    DayInfo select_info_about_date(std::string const &date_string);

private:
    sqlite3* db = nullptr;
    fs::path db_path;
};
}