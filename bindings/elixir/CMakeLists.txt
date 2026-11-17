cmake_minimum_required(VERSION 3.28)
if(NOT TARGET OpenQR::openqr)
    project(OpenQRElixir LANGUAGES C)
    find_package(OpenQR CONFIG REQUIRED)
endif()
if(NOT EXISTS "${ERL_NIF_INCLUDE_DIR}/erl_nif.h")
    message(FATAL_ERROR "Set ERL_NIF_INCLUDE_DIR to the Erlang usr/include directory")
endif()
add_library(openqr_nif MODULE c_src/openqr_nif.c)
target_include_directories(openqr_nif PRIVATE "${ERL_NIF_INCLUDE_DIR}")
target_link_libraries(openqr_nif PRIVATE OpenQR::openqr)
if(MSVC)
    target_compile_definitions(openqr_nif PRIVATE OPENQR_DELAY_LOAD WIN32_LEAN_AND_MEAN UNICODE _UNICODE
        OPENQR_NATIVE_DLL_NAME="$<TARGET_FILE_NAME:OpenQR::openqr>")
    target_link_libraries(openqr_nif PRIVATE delayimp)
    target_link_options(openqr_nif PRIVATE "/DELAYLOAD:$<TARGET_FILE_NAME:OpenQR::openqr>" /DELAY:UNLOAD)
elseif(APPLE)
    target_link_options(openqr_nif PRIVATE "LINKER:-undefined,dynamic_lookup")
endif()
set(openqr_nif_rpath "$ORIGIN")
if(APPLE)
    set(openqr_nif_rpath "@loader_path")
endif()
set_target_properties(openqr_nif PROPERTIES PREFIX "" BUILD_RPATH "${openqr_nif_rpath}")
set(openqr_runtime_name "$<TARGET_FILE_NAME:OpenQR::openqr>")
if(UNIX)
    set(openqr_runtime_name "$<TARGET_SONAME_FILE_NAME:OpenQR::openqr>")
endif()
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/c_src/openqr_nif.c" openqr_nif_source_identity)
file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" openqr_nif_config_identity)
file(SHA256 "${ERL_NIF_INCLUDE_DIR}/erl_nif.h" openqr_nif_header_identity)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/c_src/openqr_nif.c" "${ERL_NIF_INCLUDE_DIR}/erl_nif.h")
set(openqr_nif_identity
    "source=${openqr_nif_source_identity}\ncmake=${openqr_nif_config_identity}\nerlang-nif=${openqr_nif_header_identity}\nc-compiler=${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}\nsystem=${CMAKE_SYSTEM_NAME}\nprocessor=${CMAKE_SYSTEM_PROCESSOR}\nconfiguration=$<CONFIG>\nc-flags=${CMAKE_C_FLAGS}\n")
foreach(configuration IN LISTS CMAKE_CONFIGURATION_TYPES CMAKE_BUILD_TYPE)
    string(TOUPPER "${configuration}" configuration_upper)
    string(APPEND openqr_nif_identity "c-${configuration}=${CMAKE_C_FLAGS_${configuration_upper}}\n")
endforeach()
string(APPEND openqr_nif_identity
    "compile-options=$<TARGET_PROPERTY:openqr_nif,COMPILE_OPTIONS>\ncompile-definitions=$<TARGET_PROPERTY:openqr_nif,COMPILE_DEFINITIONS>\nlink-options=$<TARGET_PROPERTY:openqr_nif,LINK_OPTIONS>\n")
set(openqr_nif_build_id_file "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/openqr-nif-build-id.txt")
file(GENERATE OUTPUT "${openqr_nif_build_id_file}" CONTENT "${openqr_nif_identity}")
add_custom_target(openqr_elixir ALL
    DEPENDS openqr_nif
    COMMAND ${CMAKE_COMMAND}
        "-DOPENQR_CORE_ID=${OpenQR_BUILD_ID_FILE}"
        "-DOPENQR_NIF_ID=${openqr_nif_build_id_file}"
        "-DOPENQR_NIF=$<TARGET_FILE:openqr_nif>"
        "-DOPENQR_RUNTIME=$<TARGET_FILE:OpenQR::openqr>"
        "-DOPENQR_RUNTIME_NAME=${openqr_runtime_name}"
        "-DOPENQR_PRIV=${CMAKE_CURRENT_SOURCE_DIR}/priv"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackageRuntime.cmake"
    VERBATIM)
