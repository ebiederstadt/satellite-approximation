#pragma once

#include <boost/date_time/gregorian/gregorian.hpp>
#include <utils/date.h>
#include <utils/db.h>
#include <utils/types.h>

namespace date_time = boost::gregorian;
using namespace utils;

namespace approx {
struct DayInfo {
    date_time::date date;
    f64 percent_invalid;

    [[nodiscard]] f64 distance(date_time::date const& other, f64 weight) const;
};

enum class ApproxMethod {
    Laplace,
    Poisson
};

class DataBase : public utils::DataBase {
public:
    explicit DataBase(fs::path base_path);

    int write_approx_results(std::string const& date_string, std::string const &band_name, ApproxMethod method);
    std::unordered_map<std::string, int> get_approx_status(std::string const& date_string, ApproxMethod method);
    std::vector<DayInfo> select_close_images(std::string const& date_string);
    DayInfo select_info_about_date(std::string const& date_string);

private:
    std::unique_ptr<SQLite::Statement> stmt_select = nullptr;
    std::unique_ptr<SQLite::Statement> stmt_insert = nullptr;

    void create_approx_table();
};
}