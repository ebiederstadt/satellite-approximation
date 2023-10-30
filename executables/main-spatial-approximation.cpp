#include "approx/laplace.h"
#include <approx/poisson.h>
#include <gdal/gdal_priv.h>
#include <spdlog/spdlog.h>
#include <utils/geotiff.h>
#include <utils/log.h>

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Log location: {}", utils::log_location());
    GDALAllRegister();

    fs::path base_folder = "/home/ebiederstadt/Documents/sentinel_cache/bbox-111.9314176_56.921209032_-111.6817217_57.105787570/2019-05-22";
    std::vector<fs::path> bands = { "B02.tif", "B03.tif", "B04.tif", "B08.tif", "B11.tif" };

    approx::MultiChannelImage input_images {};
    approx::MultiChannelImage replacement_images {};

    for (auto const& band : bands) {
        fs::path tiff_path = base_folder / band;
        utils::GeoTIFF<f64> tiff(tiff_path);
        input_images.images.push_back(tiff.values);
    }

    approx::DataBase db(base_folder.parent_path());
    std::string folder_with_good_images = approx::find_good_close_image("2019-05-22", false, 0.2, db);

    fs::path replacement_path = base_folder.parent_path() / fs::path(folder_with_good_images);

    for (auto const& band : bands) {
        fs::path tiff_path = replacement_path / band;
        utils::GeoTIFF<f64> tiff(tiff_path);
        replacement_images.images.push_back(tiff.values);

        spdlog::debug("Using {} to replace images", tiff_path);
    }

    MatX<bool> invalid_mask = utils::GeoTIFF<u8>(base_folder / "cloud_shadows_noise_removed.tif").values.cast<bool>();

    approx::blend_images_poisson(input_images, replacement_images, invalid_mask);

    utils::GeoTIFF<f64> template_tiff(base_folder / "B04.tif");
    if (!fs::exists(base_folder / "poisson_approximated")) {
        spdlog::info("Creating directory");
        fs::create_directory(base_folder / "poisson_approximated");
    }
    for (size_t i = 0; i < input_images.images.size(); ++i) {
        template_tiff.values = input_images.images[i];
        template_tiff.write(base_folder / "poisson_approximated" / bands[i]);
    }
}
