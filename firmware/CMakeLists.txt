cmake_minimum_required(VERSION 3.16)

# The firmware version comes from version.txt (ESP-IDF reads it automatically).
# Bump it and add a CHANGELOG.md entry before running the Release workflow.

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pocketos)
