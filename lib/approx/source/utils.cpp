#include "approx/utils.h"

#include <opencv2/opencv.hpp>
#include <utils/error.h>
#include <utils/log.h>

namespace approx {
static auto logger = utils::create_logger("approx::utils");
static f64 gamma = 2.2;

MultiChannelImage::MultiChannelImage(size_t channels, Eigen::Index rows, Eigen::Index cols)
{
    images.insert(images.end(), channels, MatX<f64>::Zero(rows, cols));
}

MultiChannelImage read_image(fs::path path)
{
    cv::Mat image = cv::imread(path.c_str(), cv::IMREAD_COLOR);
    if (image.empty()) {
        throw utils::IOError("Failed to open image", path);
    }

    MultiChannelImage output(3, (Eigen::Index)image.rows, (Eigen::Index)image.cols);

    for (int row = 0; row < image.rows; ++row) {
        for (int col = 0; col < image.cols; ++col) {
            cv::Vec3b pixel = image.at<cv::Vec3b>(row, col);
            output[0](row, col) = std::pow(pixel[2] / 255.0, 1.0 / gamma);
            output[1](row, col) = std::pow(pixel[1] / 255.0, 1.0 / gamma);
            output[2](row, col) = std::pow(pixel[0] / 255.0, 1.0 / gamma);
        }
    }

    return output;
}

std::optional<cv::Mat> image_list_to_cv(std::vector<MatX<f64>> const& channels)
{
    if (channels.size() != 3) {
        logger->warn("Image with less than 3 channels is not supported. ({} channels provided)", channels.size());
        return {};
    }

    cv::Mat red_channel(channels[0].rows(), channels[0].cols(), CV_8UC1);
    cv::Mat green_channel(channels[1].rows(), channels[1].cols(), CV_8UC1);
    cv::Mat blue_channel(channels[2].rows(), channels[2].cols(), CV_8UC1);

    for (int row = 0; row < channels[0].rows(); ++row) {
        for (int col = 0; col < channels[0].cols(); ++col) {
            red_channel.at<uchar>(row, col) = static_cast<uchar>(std::pow(channels[0](row, col), gamma) * 255.0);
            green_channel.at<uchar>(row, col) = static_cast<uchar>(std::pow(channels[1](row, col), gamma) * 255.0);
            blue_channel.at<uchar>(row, col) = static_cast<uchar>(std::pow(channels[2](row, col), gamma) * 255);
        }
    }

    cv::Mat final_image;
    cv::merge(std::vector<cv::Mat> { blue_channel, green_channel, red_channel }, final_image);
    return final_image;
}

void write_image(const std::vector<MatX<f64>>& channels, fs::path const& output_path)
{
    auto final_image_or_none = image_list_to_cv(channels);
    if (!final_image_or_none.has_value()) {
        return;
    }
    cv::imwrite(output_path.c_str(), final_image_or_none.value());
}
}