#include "utils/date.h"

namespace utils {
Date::Date(date_time::date const& date)
    : year(date.year())
    , month(date.month())
    , day(date.day())
{
}

Date::Date(std::string const& date_string)
{
    auto date = date_time::from_simple_string(date_string);

    year = date.year();
    month = date.month();
    day = date.day();
}

bool Date::operator==(Date const& other) const
{
    return year == other.year && month == other.month && day == other.day;
}

bool Date::operator<(Date const& other) const
{
    date_time::date current(year, month, day);
    date_time::date compare(other.year, other.month, other.day);

    return current < compare;
}

std::ostream& operator<<(std::ostream& os, Date const& d)
{
    return os << d.year << '-' << std::setw(2) << std::setfill('0') << d.month << '-' << std::setw(2) << std::setfill('0') << d.day;
}

int Date::bind_sql(SQLite::Statement& stmt, int start_index) const
{
    stmt.bind(start_index, year);
    stmt.bind(start_index + 1, month);
    stmt.bind(start_index + 2, day);

    // Where you need to start binding things that come after this
    return start_index + 3;
}
}