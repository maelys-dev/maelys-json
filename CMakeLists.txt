# SPDX-License-Identifier: MPL-2.0
# maelys-json. Consumed either as an installed library (make install +
# pkg-config) or with add_subdirectory from a sibling checkout:
#
#   set(MAELYS_JSON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../maelys-json"
#       CACHE PATH "Path to the maelys-json source tree")
#   add_subdirectory("${MAELYS_JSON_SOURCE_DIR}"
#                    "${CMAKE_CURRENT_BINARY_DIR}/maelys-json" EXCLUDE_FROM_ALL)
#   target_link_libraries(consumer PUBLIC maelys::json)
#
# The Makefile remains the reference build; tools/check-cmake-sources.sh
# keeps the two source lists identical.
cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" MAELYS_JSON_RELEASE_VERSION
     LIMIT_COUNT 1)
project(maelys_json VERSION ${MAELYS_JSON_RELEASE_VERSION} LANGUAGES C)
message(STATUS "maelys-json release: ${MAELYS_JSON_RELEASE_VERSION}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(MAELYS_JSON_IS_TOP_LEVEL ON)
else()
    set(MAELYS_JSON_IS_TOP_LEVEL OFF)
endif()
option(MAELYS_JSON_BUILD_TESTS "Build the maelys-json tests" ${MAELYS_JSON_IS_TOP_LEVEL})
option(MAELYS_JSON_WERROR "Treat warnings as errors" OFF)

set(MAELYS_JSON_SOURCES
    src/common.c
    src/utf8.c
    src/keyset.c
    src/parser.c
    src/document.c
    src/helpers.c
    src/writer.c
    src/serialize.c
    src/stdio.c)

add_library(maelys_json STATIC ${MAELYS_JSON_SOURCES})
add_library(maelys::json ALIAS maelys_json)
target_include_directories(maelys_json PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_options(maelys_json PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes
    -Wconversion -Wsign-conversion -Wcast-qual -Wformat=2 -Wvla -Wundef
    $<$<BOOL:${MAELYS_JSON_WERROR}>:-Werror>)
set_target_properties(maelys_json PROPERTIES
    C_VISIBILITY_PRESET hidden
    OUTPUT_NAME maelys-json
    EXPORT_NAME json
    VERSION ${PROJECT_VERSION})

if(MAELYS_JSON_BUILD_TESTS)
    enable_testing()
    add_executable(maelys_json_tests
        tests/main.c tests/test_parser.c tests/test_reader.c
        tests/test_writer.c tests/test_vectors.c tests/test_conformance.c)
    target_link_libraries(maelys_json_tests PRIVATE maelys::json)
    add_test(NAME maelys_json COMMAND maelys_json_tests)
    set_tests_properties(maelys_json PROPERTIES
        ENVIRONMENT "MAELYS_JSON_VECTORS=${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors")
endif()

# Installation: find_package(maelys-json CONFIG) then maelys::json, or
# pkg-config maelys-json. tools/check-cmake-install.sh exercises both.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(MAELYS_JSON_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/maelys-json")
configure_file(pkgconfig/maelys-json.pc.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/maelys-json.pc" @ONLY)
configure_package_config_file(cmake/maelys-json-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/maelys-json-config.cmake"
    INSTALL_DESTINATION "${MAELYS_JSON_CMAKE_DIR}")
# Before 1.0 a minor bump may break the API, so 0.x.y is compatible only
# within the same minor version.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(MAELYS_JSON_COMPATIBILITY SameMinorVersion)
else()
    set(MAELYS_JSON_COMPATIBILITY SameMajorVersion)
endif()
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/maelys-json-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ${MAELYS_JSON_COMPATIBILITY})
install(TARGETS maelys_json EXPORT maelys_json_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/maelys DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/maelys-json.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(EXPORT maelys_json_targets NAMESPACE maelys::
    FILE maelys-json-targets.cmake
    DESTINATION "${MAELYS_JSON_CMAKE_DIR}")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/maelys-json-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/maelys-json-config-version.cmake"
    DESTINATION "${MAELYS_JSON_CMAKE_DIR}")
