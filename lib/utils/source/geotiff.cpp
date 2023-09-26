#include "utils/geotiff.h"

#include <fmt/format.h>

namespace utils {
GDALDatasetWrapper::GDALDatasetWrapper(std::string path)
{
    dataset = reinterpret_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
    if (dataset == nullptr) {
        throw std::runtime_error(fmt::format("Unable to load dataset (tried {})", path));
    }
}

GDALDatasetWrapper::~GDALDatasetWrapper()
{
    GDALClose(dataset);
}
}
