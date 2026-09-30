cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(deos_core_tests)

target_sources(app PRIVATE src/main.c)
target_link_libraries(app PRIVATE deos_core)

# We also need to add the deos_core source folder if it's not a global zephyr module
add_subdirectory(../../lib/deos_core deos_core_lib)
