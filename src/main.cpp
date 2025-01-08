#include <fmt/std.h>
#include <pybind11/eigen.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <spdlog/spdlog.h>

#include <cloud_shadow_detection/automatic_detection.h>

#include <approx/laplace.h>
#include <approx/poisson.h>
#include <utils/log.h>

namespace py = pybind11;
using namespace py::literals;

PYBIND11_MODULE(_core, m)
{
    m.doc() = "Data processing for sentinel satellite imagery";

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
        spdlog::info("Logging set to level: {}", to_string_view(level));
        spdlog::info("Log location: {}", utils::log_location());
    });

    py::class_<remote_sensing::CloudParams>(m, "CloudParams")
        .def(py::init<>());

    py::class_<remote_sensing::SkipShadowDetection>(m, "SkipShadowDetection")
        .def(py::init<>())
        .def("__repr__", [](remote_sensing::SkipShadowDetection const& skipShadowDetection) {
            return fmt::format("<SkipShadowDetection: {} (threshold: {})>", skipShadowDetection.decision, skipShadowDetection.threshold);
        });

    m.def("get_diagonal_distance", &remote_sensing::get_diagonal_distance, "min_long"_a, "min_lat"_a, "max_long"_a, "max_lat"_a);
    m.def("detect", &remote_sensing::detect,
        "params"_a, "diagonal_distance"_a, "skip_shadow_detection"_a, "use_cache"_a);

    m.def(
        "filling_missing_portions_smooth_boundaries", [](MatX<f64>& input_image, MatX<bool> const& invalid_pixels) {
            approx::fill_missing_portion_smooth_boundary(input_image, invalid_pixels);
            return input_image;
        },
        py::arg("input_image").noconvert(), py::arg("invalid_pixels").noconvert());
    m.def(
        "blend_images_poisson",
        py::overload_cast<std::vector<MatX<f64>> const&, std::vector<MatX<f64>> const&, MatX<bool> const&, f64, std::optional<int>>(&approx::blend_images_poisson),
        "input_image"_a, "replacement_image"_a, "invalid_mask"_a, "tolerance"_a = 1e-6, "max_iterations"_a = std::nullopt);
}
