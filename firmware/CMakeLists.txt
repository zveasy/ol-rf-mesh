cmake_minimum_required(VERSION 3.16)
project(ol_rf_mesh_firmware CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to a release-optimized host build unless explicitly overridden.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(DEFAULT_BUILD_TYPE "RelWithDebInfo")
    message(STATUS "No CMAKE_BUILD_TYPE specified; defaulting to ${DEFAULT_BUILD_TYPE}")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# Apply consistent warnings; rely on CMake defaults for optimization levels per build type.
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(ol_rf_mesh
    src/main.cpp
    src/config.cpp
    src/logging.cpp
    src/spi_bus.cpp
    src/adc.cpp
    src/sensors.cpp
    src/mesh.cpp
    src/mesh_encode.cpp
    src/crypto.cpp
    src/ota.cpp
    src/fault.cpp
    src/model_inference.cpp
    src/tasks.cpp
)

target_include_directories(ol_rf_mesh PRIVATE include)

include(CTest)
enable_testing()

add_executable(test_model_inference
    tests/test_model_inference.cpp
    src/model_inference.cpp
)

target_include_directories(test_model_inference PRIVATE include)

add_test(NAME test_model_inference COMMAND test_model_inference)

add_executable(test_mesh_routing
    tests/test_mesh_routing.cpp
    src/mesh.cpp
    src/mesh_encode.cpp
    src/crypto.cpp
)

target_include_directories(test_mesh_routing PRIVATE include)

add_test(NAME test_mesh_routing COMMAND test_mesh_routing)

option(ENABLE_FUZZ_TESTS "Build host fuzz-like encode/decode tests" ON)

if(ENABLE_FUZZ_TESTS)
    add_executable(test_mesh_codec_fuzz
        tests/test_mesh_codec_fuzz.cpp
        src/mesh_encode.cpp
        src/mesh.cpp
        src/crypto.cpp
    )
    target_include_directories(test_mesh_codec_fuzz PRIVATE include)
    add_test(NAME test_mesh_codec_fuzz COMMAND test_mesh_codec_fuzz)
endif()

add_executable(test_mesh_retry
    tests/test_mesh_retry.cpp
    src/mesh_encode.cpp
    src/mesh.cpp
    src/crypto.cpp
)
target_include_directories(test_mesh_retry PRIVATE include)
add_test(NAME test_mesh_retry COMMAND test_mesh_retry)

add_executable(test_mesh_roundtrip
    tests/test_mesh_roundtrip.cpp
    src/mesh_encode.cpp
    src/mesh.cpp
    src/crypto.cpp
)
target_include_directories(test_mesh_roundtrip PRIVATE include)
add_test(NAME test_mesh_roundtrip COMMAND test_mesh_roundtrip)

add_executable(test_mesh_security
    tests/test_mesh_security.cpp
    src/mesh_encode.cpp
    src/mesh.cpp
    src/crypto.cpp
)
target_include_directories(test_mesh_security PRIVATE include)
add_test(NAME test_mesh_security COMMAND test_mesh_security)

add_executable(test_model_contract
    tests/test_model_contract.cpp
)
target_include_directories(test_model_contract PRIVATE include)
add_test(NAME test_model_contract COMMAND test_model_contract)
