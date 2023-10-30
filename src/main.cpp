#include <fmt/format.h>
#include <gdal/gdal_priv.h>
#include <givde/types.hpp>
#include <pybind11/eigen.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <spdlog/spdlog.h>

#include <analysis/sis.h>
#include <analysis/utils.h>
#include <approx/laplace.h>
#include <cloud_shadow_detection/automatic_detection.h>
#include <cloud_shadow_detection/db.h>
#include <cloud_shadow_detection/temporal.h>
#include <utils/date.h>
#include <utils/log.h>

namespace py = pybind11;
using namespace py::literals;
using namespace givde;

PYBIND11_MODULE(_core, m)
{
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
        spdlog::info("Logging set to level: {}", to_string_view(level));
        spdlog::info("Log location: {}", utils::log_location());
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
        .def("__repr__", [](remote_sensing::SkipShadowDetection const& skipShadowDetection) {
            return fmt::format("<SkipShadowDetection: {} (threshold: {})>", skipShadowDetection.decision,
                skipShadowDetection.threshold);
        });

    m.def("get_diagonal_distance", &remote_sensing::get_diagonal_distance,
        "min_long"_a, "min_lat"_a, "max_long"_a, "max_lat"_a);
    m.def("detect", &remote_sensing::detect,
        "params"_a, "diagonal_distance"_a, "skip_shadow_detection"_a, "use_cache"_a);
    m.def("detect_in_folder", &remote_sensing::detect_in_folder,
        "folder_path"_a, "diagonal_distance"_a, "skipShadowDetection"_a, "use_cache"_a);

    m.def(
        "filling_missing_portions_smooth_boundaries", [](MatX<f64>& input_image, MatX<bool> const& invalid_pixels) {
            approx::fill_missing_portion_smooth_boundary(input_image, invalid_pixels);
            return input_image;
        },
        "input_image"_a, "invalid_pixels"_a);

    py::enum_<utils::Indices>(m, "Indices")
        .value("NDVI", utils::Indices::NDVI)
        .value("NDMI", utils::Indices::NDMI)
        .value("mNDWI", utils::Indices::mNDWI)
        .value("SWI", utils::Indices::SWI);

    py::class_<analysis::UseApproximatedData>(m, "UseApproximatedData")
        .def(py::init<>());

    py::class_<analysis::UseRealData>(m, "UseRealData")
        .def(py::init<>())
        .def_readwrite("exclude_cloudy_pixels", &analysis::UseRealData::exclude_cloudy_pixels)
        .def_readwrite("exclude_shadow_pixels", &analysis::UseRealData::exclude_shadow_pixels)
        .def("__repr__", [](analysis::UseRealData const& useRealData) {
            return fmt::format("UseRealData: <Exclude clouds: {}, Exclude shadows: {}>", useRealData.exclude_cloudy_pixels, useRealData.exclude_shadow_pixels);
        });
    m.def("single_image_summary", &analysis::single_image_summary,
        "base_path"_a, "use_cache"_a, "start_year"_a, "end_year"_a, "index"_a, "threshold"_a, "data_choices"_a);

    py::class_<utils::Date>(m, "Date")
        .def(py::init<std::string const&>())
        .def_readonly("year", &utils::Date::year)
        .def_readonly("month", &utils::Date::month)
        .def_readonly("day", &utils::Date::day)
        .def("__repr__", [](utils::Date& self) {
            return fmt::format("{}", self);
        });

    py::class_<remote_sensing::DataBase>(m, "DataBase")
        .def(py::init<fs::path>());

    py::class_<remote_sensing::TimeSeries>(m, "TimeSeries")
        .def(py::init<>())
        .def_readonly("values", &remote_sensing::TimeSeries::values)
        .def_readonly("clouds", &remote_sensing::TimeSeries::clouds)
        .def_readonly("dates", &remote_sensing::TimeSeries::dates);

    py::class_<remote_sensing::Temporal>(m, "Temporal")
        .def(py::init<remote_sensing::DataBase&>())
        .def("nir_for_location", [](remote_sensing::Temporal& self, fs::path const& base_folder, std::string const& date_string, f64 lat, f64 lng, int max_results) {
            return self.nir_for_location(base_folder, date_string, { lat, lng }, max_results);
        }, "base_folder"_a, "date_string"_a, "lat"_a, "lng"_a, "max_results"_a=15);
}
