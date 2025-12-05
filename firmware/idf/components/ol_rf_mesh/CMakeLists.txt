set(SRC_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../src)

set(srcs
    ${SRC_ROOT}/config.cpp
    ${SRC_ROOT}/logging.cpp
    ${SRC_ROOT}/spi_bus.cpp
    ${SRC_ROOT}/adc.cpp
    ${SRC_ROOT}/sensors.cpp
    ${SRC_ROOT}/mesh.cpp
    ${SRC_ROOT}/mesh_encode.cpp
    ${SRC_ROOT}/crypto.cpp
    ${SRC_ROOT}/ota.cpp
    ${SRC_ROOT}/fault.cpp
    ${SRC_ROOT}/model_inference.cpp
    ${SRC_ROOT}/tasks.cpp
    ${SRC_ROOT}/watchdog.cpp
    ${SRC_ROOT}/radio_driver.cpp
    idf_app_main.cpp
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../../include
    REQUIRES freertos esp_hw_support esp_system esp_event esp_wifi esp_netif esp_adc driver
)

target_compile_definitions(${COMPONENT_TARGET} PRIVATE OL_FREERTOS OL_HW_WDT)
