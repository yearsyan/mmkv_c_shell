import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class MmkvCConan(ConanFile):
    name = "mmkv_c"
    version = "2.4.0"
    license = "BSD-3-Clause"
    description = "Static C facade package for Tencent MMKV"
    topics = ("mmkv", "key-value", "static-library", "c-api")
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "fPIC": [True, False],
        "force_posix": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "fPIC": True,
        "force_posix": True,
        "build_tests": False,
    }
    package_type = "static-library"

    def export_sources(self):
        copy(self, "CMakeLists.txt", self.recipe_folder, self.export_sources_folder)
        copy(self, "*", os.path.join(self.recipe_folder, "include"), os.path.join(self.export_sources_folder, "include"))
        copy(self, "*", os.path.join(self.recipe_folder, "src"), os.path.join(self.export_sources_folder, "src"))
        copy(self, "*", os.path.join(self.recipe_folder, "tests"), os.path.join(self.export_sources_folder, "tests"))

        mmkv_folder = os.path.join(self.recipe_folder, "MMKV")
        mmkv_export_folder = os.path.join(self.export_sources_folder, "MMKV")
        copy(self, "LICENSE.TXT", mmkv_folder, mmkv_export_folder)

        core_folder = os.path.join(mmkv_folder, "Core")
        core_export_folder = os.path.join(mmkv_export_folder, "Core")
        copy(self, "CMakeLists.txt", core_folder, core_export_folder)
        for filename in sorted(os.listdir(core_folder)):
            if filename.endswith((".cpp", ".h", ".hpp")):
                copy(self, filename, core_folder, core_export_folder, keep_path=False)
        copy(self, "*", os.path.join(core_folder, "aes"), os.path.join(core_export_folder, "aes"))
        copy(self, "*", os.path.join(core_folder, "crc32"), os.path.join(core_export_folder, "crc32"))

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def package_id(self):
        del self.info.settings.compiler.cppstd

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MMKV_C_FORCE_POSIX"] = bool(self.options.force_posix)
        tc.variables["MMKV_C_BUILD_TESTS"] = bool(self.options.build_tests)
        fpic = self.options.get_safe("fPIC")
        if fpic is not None:
            tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = bool(fpic)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if self.options.build_tests:
            cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "mmkv_c")
        self.cpp_info.set_property("cmake_target_name", "mmkv_c::mmkv_c")
        self.cpp_info.libs = ["mmkv_c", "core"]

        if self.settings.os in ["Android", "iOS", "Macos", "Linux"]:
            self.cpp_info.system_libs.append("z")
        if self.settings.os == "Linux":
            self.cpp_info.system_libs.append("pthread")
