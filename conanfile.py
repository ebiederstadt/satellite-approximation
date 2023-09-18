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
            "givdetypes/0.6.1",
            "lyra/1.6.1",
            "tomlplusplus/3.3.0",
            "eigen/3.4.0",
            "spdlog/1.12.0",
            "fmt/10.1.1",
            "nlohmann_json/3.11.2",
            "glm/cci.20230113",
            "libtiff/4.5.0",
            "boost/1.82.0",
            "opencl-headers/2023.04.17",
            "opencl-icd-loader/2023.04.17",
            "range-v3/0.12.0",
            "doctest/2.4.11"
        ]
        for dependency in dependencies:
            self.requires(dependency)
