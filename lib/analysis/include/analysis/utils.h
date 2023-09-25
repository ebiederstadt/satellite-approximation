#pragma once

template<class... Ts>
struct Visitor : Ts... {
    using Ts::operator()...;
};

namespace analysis {
template<typename T, typename U>
bool contains(std::vector<T> const &vec, U const &item) {
    return std::find(vec.begin(), vec.end(), static_cast<T>(item)) != vec.end();
}
}
