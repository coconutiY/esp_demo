# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

set(COMPONENT_DIRS
    "$ENV{IDF_PATH}/components"
    "${CMAKE_SOURCE_DIR}/main"
    "${CMAKE_SOURCE_DIR}/main/components"
)
if(EXISTS "${CMAKE_SOURCE_DIR}/managed_components")
    list(APPEND COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/managed_components")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-s3)

