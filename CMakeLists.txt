cmake_minimum_required(VERSION 3.10)
project(efpsignal)

set(CMAKE_CXX_STANDARD 14)

find_package (Threads REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

#Include EFP
##[[
include(ExternalProject)
ExternalProject_Add(project_efp
        GIT_REPOSITORY https://bitbucket.org/unitxtra/efp.git
        GIT_SUBMODULES ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/efp
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/efp
        GIT_PROGRESS 1
        BUILD_COMMAND ""
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )
#]]

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/efp/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/json/)

add_library(efpsignal STATIC ${CMAKE_CURRENT_SOURCE_DIR}/efp/ElasticFrameProtocol.cpp efpsignal.cpp)

add_executable(efpsignalsimplextests simplex.cpp)
target_link_libraries(efpsignalsimplextests efpsignal Threads::Threads)

add_executable(efpsignalduplextests duplex.cpp)
target_link_libraries(efpsignalduplextests efpsignal Threads::Threads )