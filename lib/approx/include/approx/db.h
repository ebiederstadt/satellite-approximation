#pragma once

#include <boost/date_time/gregorian/gregorian.hpp>
#include <utils/date.h>
#include <utils/db.h>

namespace date_time = boost::gregorian;

namespace approx {
struct DayInfo {
    date_time::date date;
    f64 percent_invalid;
    f64 percent_invalid_noise_removed;

    [[nodiscard]] f64 distance(date_time::date const& other, f64 weight, bool use_denoised_data) const;
};

enum class ApproxMethod {
    Laplace,
    Poisson
};

class DataBase : public utils::DataBase {
public:
    explicit DataBase(fs::path base_path);

    int write_approx_results(std::string const& date_string, std::string const &band_name, ApproxMethod method, bool using_denoised);
    std::unordered_map<std::string, int> get_approx_status(std::string const& date_string, ApproxMethod method, bool using_denoised);
    std::vector<DayInfo> select_close_images(std::string const& date_string);
    DayInfo select_info_about_date(std::string const& date_string);

private:
    std::unique_ptr<utils::StmtWrapper> stmt_select = nullptr;
    std::unique_ptr<utils::StmtWrapper> stmt_insert = nullptr;

    void create_approx_table();
};
}