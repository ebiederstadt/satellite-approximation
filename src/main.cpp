#include <pybind11/eigen.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <spdlog/spdlog.h>

#include <approx/laplace.h>
#include <approx/poisson.h>
#include <utils/types.h>
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

    m.def(
        "filling_missing_portions_smooth_boundaries", [](MatX<f64>& input_image, MatX<bool> const& invalid_pixels) {
            approx::fill_missing_portion_smooth_boundary(input_image, invalid_pixels);
            return input_image;
        },
        py::arg("input_image").noconvert(), py::arg("invalid_pixels").noconvert());
    m.def(
        "blend_images_poisson",
        py::overload_cast<std::vector<MatX<f64>> const&, std::vector<MatX<f64>> const&, MatX<bool> const&>(&approx::blend_images_poisson),
        "input_image"_a, "replacment_image"_a, "invalid_mask"_a);
}
