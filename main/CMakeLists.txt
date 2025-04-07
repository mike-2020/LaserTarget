set(COMPONENT_REQUIRES audio_flash_tone audio_stream audio_pipeline audio_hal esp_peripherals)
#set(COMPONENT_EMBED_TXTFILES ../mp3/test.mp3)

idf_component_register(SRC_DIRS "." 
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES "")

