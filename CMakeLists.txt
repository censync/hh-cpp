cmake_minimum_required(VERSION 3.16)

project(hh
    VERSION 1.1.0
    DESCRIPTION "Humanized Hash: deterministic images of blockchain addresses and hashes"
    LANGUAGES CXX
)

# PROJECT_IS_TOP_LEVEL needs CMake 3.21; compute it for 3.16.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(HH_IS_TOP_LEVEL ON)
else()
    set(HH_IS_TOP_LEVEL OFF)
endif()

option(HH_BUILD_TESTS          "Build the hh test suite"                    ${HH_IS_TOP_LEVEL})
option(HH_BUILD_EXAMPLES       "Build the hh example programs"              ${HH_IS_TOP_LEVEL})
option(HH_INSTALL              "Generate the hh install rules"              ${HH_IS_TOP_LEVEL})
option(HH_WARNINGS_AS_ERRORS   "Treat compiler warnings as errors"          OFF)
option(HH_BUILD_LAB            "Build the design lab in tools/lab (never installed)" OFF)

if(HH_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
include(cmake/hhWarnings.cmake)

set(HH_SOURCES
    src/bmp.cpp
    src/c_api.cpp
    src/checksum.cpp
    src/contrast.cpp
    src/derive.cpp
    src/error.cpp
    src/features.cpp
    src/hmac.cpp
    src/jpeg.cpp
    src/pbkdf2.cpp
    src/png.cpp
    src/raster.cpp
    src/sha256.cpp
    src/wipe.cpp
)

add_library(hh ${HH_SOURCES})
add_library(hh::hh ALIAS hh)

target_compile_features(hh PUBLIC cxx_std_17)
target_include_directories(hh
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(hh PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_compile_definitions(hh PRIVATE HH_BUILDING)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(hh PUBLIC HH_SHARED)
    set_target_properties(hh PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
endif()
hh_set_warnings(hh)

if(HH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HH_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(HH_BUILD_LAB)
    add_subdirectory(tools/lab)
endif()

if(HH_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS hh
        EXPORT  hhTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
            PATTERN "*.hpp"
            PATTERN "*.h"
    )
    install(EXPORT hhTargets
        FILE        hhTargets.cmake
        NAMESPACE   hh::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hh
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hhConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/hhConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hh
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/hhConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/hhConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/hhConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hh
    )

    if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        set(HH_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    else()
        set(HH_PC_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
    endif()
    if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
        set(HH_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
    else()
        set(HH_PC_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif()
    # The prefix is found from the location of hh.pc, so an installed tree can be moved
    # and `cmake --install --prefix` works. A C program that links the static library
    # needs the C++ runtime as well.
    if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        set(HH_PC_PREFIX_FROM_PCDIR "../../..")
    else()
        file(RELATIVE_PATH HH_PC_PREFIX_FROM_PCDIR
            "/prefix/${CMAKE_INSTALL_LIBDIR}/pkgconfig" "/prefix")
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND (APPLE OR ANDROID))
        set(HH_PC_LIBS_PRIVATE "-lc++")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(HH_PC_LIBS_PRIVATE "-lstdc++")
    else()
        set(HH_PC_LIBS_PRIVATE "")
    endif()
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hh.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/hh.pc"
        @ONLY
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/hh.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
    install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/hh")
endif()
