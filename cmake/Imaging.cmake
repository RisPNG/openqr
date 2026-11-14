include(FetchContent)
include(ExternalProject)

set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(openqr_zlib
  URL https://codeload.github.com/madler/zlib/tar.gz/99623ef7097352da7993ba614161fd6a311e046e
  URL_HASH SHA256=f0d590dd90243a12f551ee13359bf6c634249c55b316fa2b7dbecabe7ae9a904
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(openqr_zlib)
if(NOT TARGET ZLIB::ZLIB)
  add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()
set(ZLIB_LIBRARY zlibstatic CACHE STRING "" FORCE)
set(ZLIB_INCLUDE_DIR "${openqr_zlib_SOURCE_DIR}" CACHE PATH "" FORCE)

set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(openqr_png
  URL https://codeload.github.com/pnggroup/libpng/tar.gz/7ef5852db99ec23c1ee8a0638d2ad4111871087c
  URL_HASH SHA256=6bc0e27dd3b16da9ad4bbb6c82ad9f6eb0a6830e004c88ef1b178a9a1823dd71
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
block()
  set(SKIP_INSTALL_ALL ON)
  FetchContent_MakeAvailable(openqr_png)
endblock()
FetchContent_GetProperties(openqr_png)
target_include_directories(png_static PRIVATE "${openqr_zlib_BINARY_DIR}")

foreach(option IN ITEMS ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO LIBWEBPMUX WEBPMUX EXTRAS FUZZTEST)
  set(WEBP_BUILD_${option} OFF CACHE BOOL "" FORCE)
endforeach()
set(WEBP_LINK_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(openqr_webp
  URL https://codeload.github.com/webmproject/libwebp/tar.gz/7c382bd1c50c292af29409c9300835ba2084bc3a
  URL_HASH SHA256=463343726d0fc3a57fbb5d284f3edf0dd9dd60c21197cbe86425efe3806a96d7
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(openqr_webp)

set(GIF_ENABLE_STATIC ON CACHE BOOL "" FORCE)
set(GIF_ENABLE_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(openqr_gif
  URL https://codeload.github.com/TheAssemblyArmada/giflib/tar.gz/1c1b13edef9cfece92c388c37a25d22c07c3cef7
  URL_HASH SHA256=2cc5fe4eeffb75fe094f72b71b8fafdd9d1daaea66bab7c18f2442ac2a76121c
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(openqr_gif)

set(openqr_jpeg_prefix "${CMAKE_BINARY_DIR}/_deps/jpeg")
if(MSVC)
  set(openqr_jpeg_library "${openqr_jpeg_prefix}/lib/jpeg-static.lib")
else()
  set(openqr_jpeg_library "${openqr_jpeg_prefix}/lib/libjpeg.a")
endif()
set(openqr_jpeg_platform_args)
if(MSVC)
  if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY OR CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
    list(APPEND openqr_jpeg_platform_args -DWITH_CRT_DLL=ON)
  else()
    list(APPEND openqr_jpeg_platform_args -DWITH_CRT_DLL=OFF)
  endif()
endif()
foreach(variable IN ITEMS CMAKE_TOOLCHAIN_FILE CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_MSVC_RUNTIME_LIBRARY CMAKE_C_COMPILER)
  if(DEFINED ${variable})
    string(REPLACE ";" "|" openqr_jpeg_value "${${variable}}")
    list(APPEND openqr_jpeg_platform_args "-D${variable}=${openqr_jpeg_value}")
  endif()
endforeach()

install(FILES "${openqr_zlib_SOURCE_DIR}/LICENSE" DESTINATION share/openqr/licenses/zlib COMPONENT OpenQR)
install(FILES "${openqr_png_SOURCE_DIR}/LICENSE.md" DESTINATION share/openqr/licenses/libpng COMPONENT OpenQR)
install(FILES "${openqr_webp_SOURCE_DIR}/COPYING" "${openqr_webp_SOURCE_DIR}/PATENTS"
  DESTINATION share/openqr/licenses/libwebp COMPONENT OpenQR)
install(FILES "${openqr_gif_SOURCE_DIR}/COPYING" DESTINATION share/openqr/licenses/giflib COMPONENT OpenQR)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/../third_party/pillow/LICENSE"
  "${CMAKE_CURRENT_LIST_DIR}/../third_party/pillow/ORIGIN" DESTINATION share/openqr/licenses/pillow COMPONENT OpenQR)
ExternalProject_Add(openqr_jpeg_build
  URL https://codeload.github.com/libjpeg-turbo/libjpeg-turbo/tar.gz/3ef3367eb68aabbc25ce91bf2136c87ed4ec7218
  URL_HASH SHA256=a2faffd5fb6f3d9c5db1c649875ba3b227af9cd894b5b50f72f53fbe6479e69c
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  INSTALL_DIR "${openqr_jpeg_prefix}"
  LIST_SEPARATOR |
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_BUILD_TYPE=$<CONFIG> -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF
    -DWITH_TOOLS=OFF -DWITH_TESTS=OFF -DWITH_SIMD=OFF
    ${openqr_jpeg_platform_args}
  BUILD_BYPRODUCTS "${openqr_jpeg_library}")
ExternalProject_Get_Property(openqr_jpeg_build SOURCE_DIR)
install(FILES "${SOURCE_DIR}/LICENSE.md" "${SOURCE_DIR}/README.ijg"
  DESTINATION share/openqr/licenses/libjpeg-turbo COMPONENT OpenQR)
file(MAKE_DIRECTORY "${openqr_jpeg_prefix}/include")
add_library(openqr_jpeg STATIC IMPORTED GLOBAL)
set_target_properties(openqr_jpeg PROPERTIES
  IMPORTED_LOCATION "${openqr_jpeg_library}"
  INTERFACE_INCLUDE_DIRECTORIES "${openqr_jpeg_prefix}/include")
add_dependencies(openqr_jpeg openqr_jpeg_build)

add_library(openqr_imaging STATIC
  "${CMAKE_CURRENT_LIST_DIR}/../src/image.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../src/png_codec.c"
  "${CMAKE_CURRENT_LIST_DIR}/../src/jpeg_decode.c"
  "${CMAKE_CURRENT_LIST_DIR}/../third_party/pillow/QuantOctree.c")
set_target_properties(openqr_imaging PROPERTIES POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
target_compile_features(openqr_imaging PUBLIC cxx_std_20)
target_compile_features(openqr_imaging PRIVATE c_std_11)
target_include_directories(openqr_imaging PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../third_party/pillow"
  "${openqr_png_SOURCE_DIR}" "${openqr_png_BINARY_DIR}"
  "${openqr_webp_SOURCE_DIR}/src" "${openqr_zlib_BINARY_DIR}")
target_link_libraries(openqr_imaging PRIVATE png_static ZLIB::ZLIB webpdemux gif_static_lib openqr_jpeg)
add_dependencies(openqr_imaging openqr_jpeg_build)
foreach(target IN ITEMS zlibstatic png_static gif_object_lib webp webpdemux webpdecoder sharpyuv webpdecode webpdsp webpdspdecode webputils webputilsdecode)
  if(TARGET ${target})
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
endforeach()
