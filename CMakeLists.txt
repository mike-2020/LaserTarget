cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include($ENV{ADF_PATH}/CMakeLists.txt)

#add_compile_options("-DIP_PORTMAP_MAX=255")

string(REGEX REPLACE ".*/\(.*\)" "\\1" CURDIR ${CMAKE_CURRENT_SOURCE_DIR})
project(${CURDIR})


fatfs_create_rawflash_image("mp3" "./mp3/" FLASH_IN_PROJECT)