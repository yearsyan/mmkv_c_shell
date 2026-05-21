from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


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
    exports_sources = (
        "CMakeLists.txt",
        "include/*",
        "src/*",
        "tests/*",
        "MMKV/Core/CMakeLists.txt",
        "MMKV/Core/*.cpp",
        "MMKV/Core/*.h",
        "MMKV/Core/*.hpp",
        "MMKV/Core/aes/*",
        "MMKV/Core/crc32/*",
        "MMKV/LICENSE.TXT",
    )
    package_type = "static-library"

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
