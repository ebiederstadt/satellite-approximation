#include "utils/filesystem.h"

namespace utils {
    DirectoryContents find_directory_contents(fs::path const &path) {
        boost::regex expr{R"(\d{4}-\d{2}-\d{2})"};
        if (!boost::regex_match(path.filename().string(), expr)) {
            return DirectoryContents::NoSatelliteData;
        }
        fs::path candidate_path = path / fs::path("B04.tif");
        if (fs::exists(candidate_path)) {
            return DirectoryContents::MultiSpectral;
        } else {
            return DirectoryContents::Radar;
        }
    }
}