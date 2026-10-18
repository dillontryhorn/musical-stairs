cmake_minimum_required(VERSION 3.22)

set(SUPPORTED_TARGETS esp32s3)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_now_test_sender)
