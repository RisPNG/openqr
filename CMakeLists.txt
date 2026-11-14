cmake_minimum_required(VERSION 3.28)
project(OpenQR VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(OPENQR_BUILD_TESTS "Build OpenQR behavioral tests" ON)
option(OPENQR_BUILD_CLI "Build the OpenQR command-line application" ON)
option(OPENQR_BUILD_ELIXIR "Build the Elixir dirty-scheduler NIF" OFF)

include(GNUInstallDirs)
include(cmake/Dependencies.cmake)
include(cmake/Imaging.cmake)

add_library(openqr SHARED
    src/api.cpp
    src/engine.cpp
    src/planner.cpp
    src/render.cpp
    third_party/qrcodegen.cpp)
add_library(OpenQR::openqr ALIAS openqr)
target_include_directories(openqr PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src third_party)
target_compile_definitions(openqr PRIVATE OPENQR_BUILDING OPENQR_VERSION="${PROJECT_VERSION}")
target_link_libraries(openqr PRIVATE openqr_imaging ZXing::ZXing blend2d::blend2d)
if(WIN32)
    target_link_libraries(openqr PRIVATE ws2_32)
    target_compile_definitions(openqr PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
set_target_properties(openqr PROPERTIES
    VERSION ${PROJECT_VERSION} SOVERSION 1
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
if(MSVC)
    target_compile_options(openqr PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(openqr PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(UNIX AND NOT APPLE)
    target_link_options(openqr PRIVATE "LINKER:--exclude-libs,ALL")
elseif(APPLE)
    file(STRINGS include/openqr/openqr.h openqr_exports REGEX "^OPENQR_API ")
    set(openqr_export_symbols "")
    foreach(declaration IN LISTS openqr_exports)
        string(REGEX MATCH "openqr_[a-z_]+\\(" symbol "${declaration}")
        string(REGEX REPLACE "\\($" "" symbol "${symbol}")
        string(APPEND openqr_export_symbols "_${symbol}\n")
    endforeach()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/OpenQR.exports" "${openqr_export_symbols}")
    target_link_options(openqr PRIVATE "LINKER:-exported_symbols_list,${CMAKE_CURRENT_BINARY_DIR}/OpenQR.exports")
endif()

if(OPENQR_BUILD_CLI)
    add_executable(openqr_cli src/cli.cpp)
    target_link_libraries(openqr_cli PRIVATE OpenQR::openqr)
    set_target_properties(openqr_cli PROPERTIES OUTPUT_NAME openqr)
    if(MSVC)
        target_sources(openqr_cli PRIVATE src/cli.manifest)
    endif()
    if(APPLE)
        set_target_properties(openqr_cli PROPERTIES INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
    elseif(UNIX)
        set_target_properties(openqr_cli PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()
endif()

if(OPENQR_BUILD_TESTS)
    enable_testing()
    add_executable(openqr_api_test tests/api_test.c)
    target_link_libraries(openqr_api_test PRIVATE OpenQR::openqr)
    add_test(NAME api COMMAND openqr_api_test)
    add_executable(openqr_image_test tests/image_test.cpp)
    target_include_directories(openqr_image_test PRIVATE src)
    target_compile_definitions(openqr_image_test PRIVATE OPENQR_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(openqr_image_test PRIVATE openqr_imaging)
    add_test(NAME image COMMAND openqr_image_test)
    add_executable(openqr_planner_test tests/planner_test.cpp src/planner.cpp third_party/qrcodegen.cpp)
    target_include_directories(openqr_planner_test PRIVATE src third_party tests)
    target_link_libraries(openqr_planner_test PRIVATE openqr_imaging)
    add_test(NAME planner COMMAND openqr_planner_test)
endif()

if(OPENQR_BUILD_ELIXIR)
    set(OPENQR_ELIXIR_BINDING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bindings/elixir" CACHE PATH "Elixir binding source directory")
    add_subdirectory("${OPENQR_ELIXIR_BINDING_DIR}" bindings/elixir)
endif()

include(CMakePackageConfigHelpers)
install(TARGETS openqr EXPORT OpenQRTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT OpenQR
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT OpenQR
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT OpenQR)
if(OPENQR_BUILD_CLI)
    install(TARGETS openqr_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT OpenQR)
endif()
install(DIRECTORY include/openqr DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT OpenQR)
install(EXPORT OpenQRTargets NAMESPACE OpenQR:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenQR COMPONENT OpenQR)
configure_package_config_file(cmake/OpenQRConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/OpenQRConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenQR)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/OpenQRConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/OpenQRConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/OpenQRConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenQR COMPONENT OpenQR)
install(FILES README.md THIRD_PARTY.md DESTINATION share/openqr COMPONENT OpenQR)
install(DIRECTORY bindings/ DESTINATION share/openqr/bindings COMPONENT OpenQR
    PATTERN "_build" EXCLUDE PATTERN "target" EXCLUDE PATTERN "priv" EXCLUDE
    PATTERN "bin" EXCLUDE PATTERN "obj" EXCLUDE PATTERN "__pycache__" EXCLUDE)
install(FILES "${zxing_SOURCE_DIR}/LICENSE" DESTINATION share/openqr/licenses/zxing COMPONENT OpenQR)
install(FILES "${blend2d_SOURCE_DIR}/LICENSE.md" DESTINATION share/openqr/licenses/blend2d COMPONENT OpenQR)
install(FILES third_party/LICENSE.qrcodegen DESTINATION share/openqr/licenses/qrcodegen COMPONENT OpenQR)
install(FILES third_party/LICENSE.numpy-random third_party/LICENSE.pocketfft
    third_party/LICENSE.cpython third_party/LICENSE.mt19937 third_party/LICENSE.numpy-seed-sequence
    third_party/planner-sources.json third_party/pcg64.h
    DESTINATION share/openqr/licenses COMPONENT OpenQR)
install(FILES third_party/python/LICENSE third_party/python/LICENSE.unicode third_party/python/NOTICE
    DESTINATION share/openqr/licenses/python COMPONENT OpenQR)
