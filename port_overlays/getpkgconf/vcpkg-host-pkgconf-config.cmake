find_program(PKG_CONFIG_EXECUTABLE NAMES pkgconf PATH "${CMAKE_CURRENT_LIST_DIR}/../../../x64-windows-static-md/tools/pkgconf")

include(CMakeFindDependencyMacro)

find_dependency(PkgConfig)
