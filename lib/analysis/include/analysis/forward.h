#pragma once

#include <variant>

namespace analysis {
class DataBase;

struct UseApproximatedData;
struct UseRealData;
using DataChoices = std::variant<UseApproximatedData, UseRealData>;
}
