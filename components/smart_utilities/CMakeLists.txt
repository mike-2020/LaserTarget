FILE(GLOB SMART_UTILITY_SOURCES ${COMPONENT_DIR}/network/*.c 
                                ${COMPONENT_DIR}/network/*.cpp
                                ${COMPONENT_DIR}/common/*.c 
                                ${COMPONENT_DIR}/ota/*.cpp
                                ${COMPONENT_DIR}/sensor/*.c
                                )

idf_component_register(SRCS ${SMART_UTILITY_SOURCES}
                       INCLUDE_DIRS include
                       REQUIRES esp_wifi wpa_supplicant esp_https_ota esp_adc esp_http_client nvs_flash driver app_update esp_pm esp_timer)

