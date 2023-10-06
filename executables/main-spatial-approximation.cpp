#include <approx/poisson.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <utils/fmt_filesystem.h>
#include <utils/log.h>

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Log location: {}", utils::log_location());
    approx::MultiChannelImage image = approx::read_image("data/beach.jpg");
    approx::MultiChannelImage replacement_image = approx::read_image("data/dog.png");

    approx::blend_images_poisson(image, replacement_image, image.rows() - replacement_image.rows() - 10, 130, 255);

    approx::write_image(image.images, "data/test_output.png");
}