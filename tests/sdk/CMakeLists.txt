cmake_minimum_required(VERSION 3.28)
project(OpenQRConsumer LANGUAGES C)

find_package(OpenQR CONFIG REQUIRED)
add_executable(openqr_consumer ../api_test.c)
set_target_properties(openqr_consumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_link_libraries(openqr_consumer PRIVATE OpenQR::openqr)
if(WIN32)
  add_custom_command(TARGET openqr_consumer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:openqr_consumer> $<TARGET_FILE_DIR:openqr_consumer>
    COMMAND_EXPAND_LISTS)
endif()

enable_testing()
add_test(NAME installed_api COMMAND openqr_consumer)
find_program(openqr_cli NAMES openqr HINTS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES bin NO_DEFAULT_PATH REQUIRED)
add_test(NAME installed_cli COMMAND "${openqr_cli}" --help)
