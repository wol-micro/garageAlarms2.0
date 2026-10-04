# Logic node firmware (ESP32-S3 host; its ESP32-H2 runs Espressif's RCP firmware).
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
# Shared components plus the logic-only ones (config, rules, notify, ui).
set(COMPONENTS main
    ga_config proto mesh delivery membership storage sensors sys
    config rules notify ui)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(logic-s3)
