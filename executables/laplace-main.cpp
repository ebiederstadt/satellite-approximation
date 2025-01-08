#include <filesystem>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>

#include <approx/laplace.h>
#include <fmt/std.h>
#include <utils/log.h>

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    if (argc != 4) {
        spdlog::error("Usage: {} <base_image> <invalid_image> <output_path>", argv[0]);
        return -1;
    }
    fs::path file(argv[1]);
    fs::path replacement_file(argv[2]);
    fs::path output_path(argv[3]);

    if (!fs::exists(file)) {
        spdlog::error("{} does not exist", file);
        return -1;
    }
    if (!fs::exists(replacement_file)) {
        spdlog::error("{} does not exist", replacement_file);
        return -1;
    }

    spdlog::info("Logs are stored in: {}", utils::log_location());
    spdlog::set_level(spdlog::level::debug);

    cv::Mat image = cv::imread(file, cv::IMREAD_COLOR);
    cv::Mat invalid_areas = cv::imread(replacement_file, cv::IMREAD_COLOR);

    spdlog::info("Starting laplace");
    cv::Mat res = approx::apply_laplace(image, invalid_areas, 220);
    spdlog::info("Finished. Writing file");
    cv::imwrite(output_path, res);

    return 0;
}