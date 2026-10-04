# Collector firmware (ESP32-H2, native Zigbee stack).
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
# Shared components only: the logic-only config, rules, notify, ui stay out.
set(COMPONENTS main
    ga_config proto mesh delivery membership storage sensors sys)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(collector-h2)

# REQUIRES pull components in transitively, so COMPONENTS alone cannot keep the
# logic-only actors out: fail the build if any of them ended up here.
idf_build_get_property(ga_build_components BUILD_COMPONENTS)
foreach(ga_logic_only config rules notify ui)
    if(ga_logic_only IN_LIST ga_build_components)
        message(FATAL_ERROR "logic-only component '${ga_logic_only}' is in the collector build")
    endif()
endforeach()
