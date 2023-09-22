#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/eigen.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <givde/types.hpp>
#include <gdal/gdal_priv.h>

#include <cloud_shadow_detection/automatic_detection.h>
#include <spatial_approximation/approx.h>

namespace py = pybind11;
using namespace py::literals;
using namespace givde;

PYBIND11_MODULE(_core, m) {
    m.doc() = "Data processing for sentinel satellite imagery";

    GDALAllRegister();

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
        spdlog::info("Logging set to level: {}", to_short_c_str(level));
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
                return fmt::format("<SkipShadowDetection: {} (threshold: {})>", skipShadowDetection.decision,
                                   skipShadowDetection.threshold);
            });

    m.def("get_diagonal_distance", &remote_sensing::get_diagonal_distance,
          "min_long"_a, "min_lat"_a, "max_long"_a, "max_lat"_a);
    m.def("detect", &remote_sensing::detect,
          "params"_a, "diagonal_distance"_a, "skip_shadow_detection"_a, "use_cache"_a);
    m.def("detect_in_folder", &remote_sensing::detect_in_folder,
          "folder_path"_a, "diagonal_distance"_a, "skipShadowDetection"_a, "use_cache"_a);

    m.def("filling_missing_portions_smooth_boundaries", [](MatX<f64> &input_image, MatX<bool> const &invalid_pixels) {
            spatial_approximation::fill_missing_portion_smooth_boundary(input_image, invalid_pixels);
            return input_image;
        },
          "input_image"_a, "invalid_pixels"_a);
    py::class_<spatial_approximation::ConnectedComponents>(m, "ConnectedComponents")
            .def(py::init<MatX<i32>, std::unordered_map<i32, std::vector<spatial_approximation::index_t>>>());

    m.def("find_connected_components", &spatial_approximation::find_connected_components, "invalid_mask"_a);

    py::class_<spatial_approximation::Status>(m, "Status")
            .def_readonly("percent_clouds", &spatial_approximation::Status::percent_clouds)
            .def_readonly("percent_shadows", &spatial_approximation::Status::percent_shadows)
            .def_readonly("band_computation_status", &spatial_approximation::Status::band_computation_status);
    m.def("fill_missing_data_folder", &spatial_approximation::fill_missing_data_folder,
          "base_folder"_a, "band_names"_a, "use_cache"_a, "skip_threshold"_a);
}
