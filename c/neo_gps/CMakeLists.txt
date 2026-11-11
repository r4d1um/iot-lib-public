cmake_minimum_required(VERSION 3.13)

# ESP-IDF: Ordner einfach nach components/neo_gps kopieren
if(ESP_PLATFORM)
    idf_component_register(SRCS "src/neo_gps.c" "src/neo_gps_filter.c" "src/neo_gps_time.c" "src/neo_gps_util.c"
                           INCLUDE_DIRS "src")
    return()
endif()

project(neo_gps C)

add_library(neo_gps STATIC src/neo_gps.c src/neo_gps_filter.c src/neo_gps_time.c src/neo_gps_util.c)
target_include_directories(neo_gps PUBLIC src)
set_target_properties(neo_gps PROPERTIES C_STANDARD 99)

# Zephyr / Pico-SDK / STM32CubeIDE-CMake:  add_subdirectory(neo_gps)
#                                          target_link_libraries(app neo_gps)
find_library(MATH_LIB m)
if(MATH_LIB)
    target_link_libraries(neo_gps PUBLIC ${MATH_LIB})
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_executable(test_neo_gps tests/test_neo_gps.c)
    target_link_libraries(test_neo_gps neo_gps)
    add_test(NAME neo_gps COMMAND test_neo_gps)
endif()
