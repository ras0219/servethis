vcpkg_fail_port_install(ON_TARGET "UWP" ON_ARCH "arm")

set(MICROHTTPD_VERSION 0.9.73)

vcpkg_download_distfile(ARCHIVE
    URLS
        "https://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-${MICROHTTPD_VERSION}.tar.gz"
        "https://www.mirrorservice.org/sites/ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-${MICROHTTPD_VERSION}.tar.gz"
    FILENAME "libmicrohttpd-${MICROHTTPD_VERSION}.tar.gz"
    SHA512 473996b087ac6734ab577a1c7681c6c0b0136e04e34e13c3b50fd758358c1516017ad79097e0c57792786f6dd0208834374c09238113efed13bb4be11ef649d3
)

vcpkg_extract_source_archive_ex(
    ARCHIVE "${ARCHIVE}"
    OUT_SOURCE_PATH SOURCE_PATH
    PATCHES runtimelibrary.patch
)
if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
    if (VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
        set(CFG_SUFFIX "dll")
    else()
        set(CFG_SUFFIX "static")
    endif()
    if(VCPKG_CRT_LINKAGE STREQUAL "dynamic")
        set(RL_SUFFIX "DLL")
    else()
        set(RL_SUFFIX "")
    endif()

    vcpkg_install_msbuild(
        SOURCE_PATH "${SOURCE_PATH}"
        PROJECT_SUBPATH w32/VS2015/libmicrohttpd.vcxproj
        RELEASE_CONFIGURATION "Release-${CFG_SUFFIX}"
        DEBUG_CONFIGURATION "Debug-${CFG_SUFFIX}"
        OPTIONS "/p:RuntimeLibraryDLL=${RL_SUFFIX}"
    )

    set(VERSION "${MICROHTTPD_VERSION}")
    set(includedir "\${prefix}/include")
    set(libdir "\${prefix}/lib")
    set(includedir "\${prefix}/include")
    configure_file(${SOURCE_PATH}/libmicrohttpd.pc.in "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/libmicrohttpd.pc" @ONLY)
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/lib/pkgconfig/libmicrohttpd.pc" " -lmicrohttpd" " -llibmicrohttpd")
    if(NOT VCPKG_BUILD_TYPE STREQUAL "release")
        configure_file(${SOURCE_PATH}/libmicrohttpd.pc.in "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/libmicrohttpd.pc" @ONLY)
        vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/libmicrohttpd.pc" " -lmicrohttpd" " -llibmicrohttpd_d")
    endif()

    file(GLOB MICROHTTPD_HEADERS "${SOURCE_PATH}/src/include/microhttpd*.h")
    file(COPY ${MICROHTTPD_HEADERS} DESTINATION "${CURRENT_PACKAGES_DIR}/include")
else()
    if(VCPKG_TARGET_IS_OSX AND VCPKG_LIBRARY_LINKAGE STREQUAL "static")
        set(ENV{LIBS} "$ENV{LIBS} -framework Foundation -framework AppKit") # TODO: Get this from the extracted cmake vars somehow
    endif()
    vcpkg_configure_make(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
            --disable-doc
            --disable-examples
            --disable-curl
            --disable-https
            --with-gnutls=no
    )

    vcpkg_install_make()

    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
endif()

vcpkg_fixup_pkgconfig()

file(INSTALL "${SOURCE_PATH}/COPYING" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
