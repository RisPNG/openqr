file(GLOB_RECURSE openqr_identity_inputs CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "src/*.c" "src/*.cpp" "src/*.h" "src/*.hpp"
    "include/*.h" "third_party/*.c" "third_party/*.cpp"
    "third_party/*.h" "third_party/*.hpp"
    "cmake/*.cmake" "cmake/*.in")
list(APPEND openqr_identity_inputs CMakeLists.txt)
list(SORT openqr_identity_inputs)
set(openqr_source_identity "")
foreach(input IN LISTS openqr_identity_inputs)
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${input}" input_hash)
    string(APPEND openqr_source_identity "${input}=${input_hash}\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
endforeach()
string(SHA256 openqr_source_identity "${openqr_source_identity}")

set(openqr_build_identity
    "OpenQR=${PROJECT_VERSION}\nsource=${openqr_source_identity}\nsystem=${CMAKE_SYSTEM_NAME}\nprocessor=${CMAKE_SYSTEM_PROCESSOR}\npointer-size=${CMAKE_SIZEOF_VOID_P}\nc-compiler=${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}\ncxx-compiler=${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}\nplatform=${CMAKE_GENERATOR_PLATFORM}\ntoolset=${CMAKE_GENERATOR_TOOLSET}\nconfiguration=$<CONFIG>\n")
foreach(variable IN ITEMS CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_SHARED_LINKER_FLAGS
    CMAKE_MSVC_RUNTIME_LIBRARY CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET
    CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    string(APPEND openqr_build_identity "${variable}=${${variable}}\n")
endforeach()
foreach(configuration IN LISTS CMAKE_CONFIGURATION_TYPES CMAKE_BUILD_TYPE)
    string(TOUPPER "${configuration}" configuration_upper)
    foreach(language IN ITEMS C CXX SHARED_LINKER)
        string(APPEND openqr_build_identity
            "${language}-${configuration}=${CMAKE_${language}_FLAGS_${configuration_upper}}\n")
    endforeach()
endforeach()
string(APPEND openqr_build_identity
    "compile-options=$<TARGET_PROPERTY:openqr,COMPILE_OPTIONS>\ncompile-definitions=$<TARGET_PROPERTY:openqr,COMPILE_DEFINITIONS>\nlink-options=$<TARGET_PROPERTY:openqr,LINK_OPTIONS>\n")
set(OpenQR_BUILD_ID_FILE "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/openqr-build-id.txt")
file(GENERATE OUTPUT "${OpenQR_BUILD_ID_FILE}" CONTENT "${openqr_build_identity}")
