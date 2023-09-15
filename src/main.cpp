#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/eigen.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "detectCloudsShadows.h"

namespace py = pybind11;
using namespace py::literals;

PYBIND11_MODULE(_core, m)
{
    m.doc() = "Cloud and shadow detection for sentinel satellite imagery";

    py::enum_<spdlog::level::level_enum>(m, "LogLevel")
            .value("Debug", spdlog::level::level_enum::debug)
            .value("Info", spdlog::level::level_enum::info)
            .value("Warn", spdlog::level::level_enum::warn)
            .value("Error", spdlog::level::level_enum::err)
            .value("Critical", spdlog::level::level_enum::critical);
    m.def("set_log_level", [](spdlog::level::level_enum level) {
        spdlog::set_level(level);
    });

    py::class_<remote_sensing::CloudParams>(m, "CloudParams")
            .def(py::init<>());

    py::class_<remote_sensing::SkipShadowDetection>(m, "SkipShadowDetection")
            .def(py::init<>())
            .def("__repr__", [](remote_sensing::SkipShadowDetection const &skipShadowDetection) {
                return fmt::format("<SkipShadowDetection: {} (threshold: {})>", skipShadowDetection.decision, skipShadowDetection.threshold);
            });

    m.def("get_diagonal_distance", &remote_sensing::get_diagonal_distance, "bbox"_a);
    m.def("detect", &remote_sensing::detect,
          "params"_a, "diagonal_distance"_a, "skip_shadow_detection"_a, "use_cache"_a);
}
