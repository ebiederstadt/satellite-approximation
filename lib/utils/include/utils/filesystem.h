#include <filesystem>
#include <boost/regex.hpp>

namespace fs = std::filesystem;

namespace utils {
    enum DirectoryContents {
        NoSatelliteData,
        MultiSpectral,
        Radar
    };

    DirectoryContents find_directory_contents(fs::path const &path);
}
