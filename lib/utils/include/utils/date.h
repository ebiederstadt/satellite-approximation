#pragma once

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/functional/hash.hpp>
#include <fmt/ostream.h>
#include <sqlite3.h>

namespace date_time = boost::gregorian;

namespace utils {
struct Date {
    int year = 0;
    int month = 0;
    int day = 0;

    explicit Date(date_time::date const& date);
    explicit Date(std::string const& date_string);

    bool operator==(Date const& other) const;
    friend std::ostream& operator<<(std::ostream& os, Date const& d);

    int bind_sql(sqlite3_stmt* stmt, int start_index) const;
};
}

namespace std {
template<>
struct hash<utils::Date> {
    auto operator()(utils::Date const& date) const -> size_t
    {
        size_t seed = 0;
        boost::hash_combine(seed, date.year);
        boost::hash_combine(seed, date.month);
        boost::hash_combine(seed, date.day);
        return seed;
    }
};
}

template<>
struct fmt::formatter<utils::Date> : ostream_formatter { };
