#include "approx/laplace.h"
#include <approx/poisson.h>
#include <gdal/gdal_priv.h>
#include <spdlog/spdlog.h>
#include <utils/fmt_filesystem.h>
#include <utils/log.h>

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Log location: {}", utils::log_location());
    GDALAllRegister();

    fs::path base_folder = "/home/ebiederstadt/Documents/sentinel_cache/bbox-111.9314176_56.921209032_-111.6817217_57.105787570";
    approx::DataBase db(base_folder);
    approx::find_good_close_image("2022-08-22", true, 0.5, db);

//    approx::MultiChannelImage image = approx::read_image("data/poisson_filling_results/beach.png");
//    approx::MultiChannelImage replacement_image = approx::read_image("data/poisson_filling_results/dog.png");
//
//    int start_row = 2008;
//    int start_col = 748;
//    approx::blend_images_poisson(image, replacement_image, start_row, start_col, 255);
//
//    approx::write_image(image.images, "data/dog_sand.png");
//
//    start_row = image.rows() - replacement_image.rows() - 10;
//    start_col = 130;
//    image = approx::read_image("data/poisson_filling_results/beach.png");
//    approx::blend_images_poisson(image, replacement_image, start_row, start_col, 255);
//    approx::write_image(image.images, "data/dog_water.png");
//
//
//    image = approx::read_image("data/poisson_filling_results/beach.png");
//    approx::highlight_area_replaced(image, replacement_image, start_row, start_col, { 255.0, 0.0, 0.0 });
//    approx::write_image(image.images, "data/areas_replaced.png");
}