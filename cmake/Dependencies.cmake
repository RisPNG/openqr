include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ZXING_READERS ON CACHE BOOL "" FORCE)
set(ZXING_WRITERS OFF CACHE STRING "" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZXING_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_C_API OFF CACHE BOOL "" FORCE)
foreach(format 1D AZTEC DATAMATRIX MAXICODE PDF417)
    set(ZXING_ENABLE_${format} OFF CACHE BOOL "" FORCE)
endforeach()
set(ZXING_ENABLE_QRCODE ON CACHE BOOL "" FORCE)
FetchContent_Declare(zxing
    URL https://codeload.github.com/zxing-cpp/zxing-cpp/tar.gz/f19f7c6e0509fe0eeb822bf35c41b0f637dec290
    URL_HASH SHA256=a487e8680ecf4028038f03e2fac2107e2a491bae4d2a11ddeb1bf2ab521e77ac
    EXCLUDE_FROM_ALL)

set(BLEND2D_STATIC ON CACHE BOOL "" FORCE)
set(BLEND2D_NO_JIT ON CACHE BOOL "" FORCE)
set(BLEND2D_NO_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(blend2d
    URL https://codeload.github.com/blend2d/blend2d/tar.gz/58ca9460b4138af6e793184682e9324e0a8053b2
    URL_HASH SHA256=b7887f581dc1340ce4c7198a4ccc96c79fd832fe099367db065c3b9ff5dfffd8
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(zxing blend2d)
if(MSVC)
    get_target_property(blend2d_compile_options blend2d COMPILE_OPTIONS)
    list(FILTER blend2d_compile_options EXCLUDE REGEX "^[-/]MP$")
    set_property(TARGET blend2d PROPERTY COMPILE_OPTIONS "${blend2d_compile_options}")
endif()
