cmake_minimum_required(VERSION 3.22)

set(SUPPORTED_TARGETS esp32 esp32s3)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(step_sensor)
