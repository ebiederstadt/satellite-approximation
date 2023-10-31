#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/functional/hash.hpp>
#include <fmt/ostream.h>

namespace date_time = boost::gregorian;

namespace utils {
struct Date {
    int year = 0;
    int month = 0;
    int day = 0;

    explicit Date(date_time::date const& date);
    explicit Date(std::string const& date_string);
    Date() = default;

    bool operator==(Date const& other) const;
    bool operator<(Date const &other) const;
    friend std::ostream& operator<<(std::ostream& os, Date const& d);

    int bind_sql(SQLite::Statement& stmt, int start_index) const;
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
