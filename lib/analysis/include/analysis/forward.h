#pragma once

#include <variant>

namespace analysis {
enum class Indices;
class DataBase;

struct UseApproximatedData;
struct UseRealData;
using DataChoices = std::variant<UseApproximatedData, UseRealData>;
}
