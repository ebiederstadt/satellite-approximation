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

    py::class_<std::filesystem::path>(m, "Path")
            .def(py::init<std::string>());
    py::implicitly_convertible<std::string, std::filesystem::path>();

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
            .def(py::init<>())
            .def_readwrite("nir_path", &remote_sensing::CloudParams::nir_path)
            .def_readwrite("clp_path", &remote_sensing::CloudParams::clp_path)
            .def_readwrite("cld_path", &remote_sensing::CloudParams::cld_path)
            .def_readwrite("scl_path", &remote_sensing::CloudParams::scl_path)
            .def_readwrite("rgb_path", &remote_sensing::CloudParams::rgb_path)
            .def_readwrite("view_zenith_path", &remote_sensing::CloudParams::view_zenith_path)
            .def_readwrite("view_azimuth_path", &remote_sensing::CloudParams::view_azimuth_path)
            .def_readwrite("sun_zenith_path", &remote_sensing::CloudParams::sun_zenith_path)
            .def_readwrite("sun_azimuth_path", &remote_sensing::CloudParams::sun_azimuth_path);

    py::class_<remote_sensing::SkipShadowDetection>(m, "SkipShadowDetection")
            .def(py::init<bool, double>())
            .def("__repr__", [](remote_sensing::SkipShadowDetection const &skipShadowDetection) {
                return fmt::format("<SkipShadowDetection: {} (threshold: {})>", skipShadowDetection.decision, skipShadowDetection.threshold);
            });

    m.def("get_diagonal_distance", &remote_sensing::get_diagonal_distance,
          "min_long"_a, "min_lat"_a, "max_long"_a, "max_lat"_a);
    m.def("detect", &remote_sensing::detect,
          "params"_a, "diagonal_distance"_a, "skip_shadow_detection"_a, "use_cache"_a);
    m.def("detect_in_folder", &remote_sensing::detect_in_folder,
          "folder_path"_a, "diagonal_distance"_a, "skipShadowDetection"_a, "use_cache"_a);
}
