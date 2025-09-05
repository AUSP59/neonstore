from conan import ConanFile
class NeonStoreSystem(ConanFile):
    name = "neonstoresystem"
    version = "1.0.0"
    license = "Apache-2.0"
    description = "World-class C++ store management library and CLI"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"
    exports_sources = "*"
    def layout(self):
        from conan.tools.cmake import cmake_layout
        cmake_layout(self)
    def requirements(self):
        pass
    def build(self):
        from conan.tools.cmake import CMake
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    def package(self):
        from conan.tools.cmake import CMake
        cmake = CMake(self)
        cmake.install()
