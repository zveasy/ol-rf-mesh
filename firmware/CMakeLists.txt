cmake_minimum_required(VERSION 3.16)
project(ol_rf_mesh_firmware CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
