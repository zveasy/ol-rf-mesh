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
    src/watchdog.cpp
    src/radio_driver.cpp
)

target_include_directories(ol_rf_mesh PRIVATE include)
option(ENABLE_HW_WDT "Enable hardware watchdog integration (e.g., ESP-IDF task WDT)" OFF)
set(WDT_INCLUDE_HINT "" CACHE PATH "Override path to esp_task_wdt.h (e.g., <idf>/components/esp_hw_support/include)")
if(ENABLE_HW_WDT)
    if(NOT DEFINED IDF_PATH AND DEFINED ENV{IDF_PATH})
        set(IDF_PATH $ENV{IDF_PATH})
        message(STATUS "Using IDF_PATH from environment: ${IDF_PATH}")
    endif()
    find_path(ESP_TASK_WDT_INCLUDE
        NAMES esp_task_wdt.h
        HINTS
            ${WDT_INCLUDE_HINT}
            ${IDF_PATH}/components/esp_hw_support/include
            ${IDF_PATH}/components/esp_system/include
            ${IDF_PATH}/components/esp_common/include
    )
    if(NOT ESP_TASK_WDT_INCLUDE)
        message(FATAL_ERROR "ENABLE_HW_WDT=ON but esp_task_wdt.h not found. Set IDF_PATH (export.sh) or WDT_INCLUDE_HINT, or disable ENABLE_HW_WDT.")
    endif()
endif()
set(WDT_DEFINE $<IF:$<BOOL:${ENABLE_HW_WDT}>,OL_HW_WDT,>)
set(TASK_WDT_DEPS ol_rf_mesh)

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

add_executable(test_mesh_golden
    tests/test_mesh_golden.cpp
    src/mesh_encode.cpp
    src/mesh.cpp
    src/crypto.cpp
)
target_include_directories(test_mesh_golden PRIVATE include)
add_test(NAME test_mesh_golden COMMAND test_mesh_golden)

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

add_executable(test_mesh_send_handler
    tests/test_mesh_send_handler.cpp
    src/mesh.cpp
    src/mesh_encode.cpp
    src/crypto.cpp
)
target_include_directories(test_mesh_send_handler PRIVATE include)
add_test(NAME test_mesh_send_handler COMMAND test_mesh_send_handler)

add_executable(test_mesh_convergence
    tests/test_mesh_convergence.cpp
    src/mesh.cpp
    src/mesh_encode.cpp
    src/crypto.cpp
)
target_include_directories(test_mesh_convergence PRIVATE include)
add_test(NAME test_mesh_convergence COMMAND test_mesh_convergence)

add_executable(test_mesh_churn
    tests/test_mesh_churn.cpp
    src/mesh.cpp
    src/mesh_encode.cpp
    src/crypto.cpp
)
target_include_directories(test_mesh_churn PRIVATE include)
add_test(NAME test_mesh_churn COMMAND test_mesh_churn)

add_executable(test_mesh_ttl_retry
    tests/test_mesh_ttl_retry.cpp
    src/mesh.cpp
    src/mesh_encode.cpp
    src/crypto.cpp
)
target_include_directories(test_mesh_ttl_retry PRIVATE include)
add_test(NAME test_mesh_ttl_retry COMMAND test_mesh_ttl_retry)

add_executable(test_hw_smoke
    tests/test_hw_smoke.cpp
    src/adc.cpp
    src/sensors.cpp
    src/mesh.cpp
    src/mesh_encode.cpp
    src/crypto.cpp
    src/watchdog.cpp
    src/radio_driver.cpp
)
target_include_directories(test_hw_smoke PRIVATE include)
add_test(NAME test_hw_smoke COMMAND test_hw_smoke)

add_executable(test_task_map
    tests/test_task_map.cpp
    src/tasks.cpp
    src/config.cpp
    src/adc.cpp
    src/sensors.cpp
    src/mesh.cpp
    src/mesh_encode.cpp
    src/crypto.cpp
    src/model_inference.cpp
    src/ota.cpp
    src/fault.cpp
    src/watchdog.cpp
)
target_include_directories(test_task_map PRIVATE include)
add_test(NAME test_task_map COMMAND test_task_map)

# Propagate hardware watchdog define to all targets that touch watchdog.cpp
foreach(tgt ol_rf_mesh test_task_map test_mesh_send_handler)
    target_compile_definitions(${tgt} PRIVATE ${WDT_DEFINE})
endforeach()

add_executable(test_model_contract
    tests/test_model_contract.cpp
)
target_include_directories(test_model_contract PRIVATE include)
add_test(NAME test_model_contract COMMAND test_model_contract)

if(ENABLE_HW_WDT)
    foreach(tgt ol_rf_mesh test_task_map test_mesh_send_handler)
        target_include_directories(${tgt} PRIVATE ${ESP_TASK_WDT_INCLUDE})
    endforeach()
endif()
