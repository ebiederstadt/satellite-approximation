from __future__ import annotations
from conan import ConanFile

class Recipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps", "VirtualRunEnv"

    def layout(self):
        self.folders.generators = "conan"

    def requirements(self):
        dependencies = [
            "pybind11/2.10.1",
            "spdlog/1.12.0",
            "opencv/4.9.0",
            "boost/1.82.0",
            "range-v3/0.12.0",
            "magic_enum/0.9.3",
            "glm/cci.20230113",
            "opencl-headers/2023.04.17",
            "opencl-icd-loader/2023.04.17"
        ]
        for dependency in dependencies:
            self.requires(dependency)
        self.requires("zlib/1.3", override=True)

    def configure(self):
        self.options["opencv"].with_ffmpeg = False
        self.options["opencv"].with_webp = False
