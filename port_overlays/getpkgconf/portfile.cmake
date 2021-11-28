set(VCPKG_POLICY_EMPTY_PACKAGE enabled)
configure_file("${CMAKE_CURRENT_LIST_DIR}/vcpkg-host-pkgconf-config.cmake" "${CURRENT_PACKAGES_DIR}/share//vcpkg-host-pkgconf/vcpkg-host-pkgconf-config.cmake" @ONLY)
