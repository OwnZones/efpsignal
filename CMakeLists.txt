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
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/efp --config ${CMAKE_BUILD_TYPE} --target efp
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )
add_library(efp STATIC IMPORTED)
set_property(TARGET efp PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/efp/libefp.a)
add_dependencies(efp project_efp)
#]]

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/efp/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/json/)

#[[
file(GLOB base "${CMAKE_CURRENT_SOURCE_DIR}/efp/ElasticFrameProtocol.cpp")
add_library(efp STATIC ${base})
target_link_libraries(efp Threads::Threads)
]]

add_library(efpsignal STATIC efpsignal.cpp)
target_link_libraries(efpsignal efp Threads::Threads)

add_executable(efpsignalsimplextests simplex.cpp)
target_link_libraries(efpsignalsimplextests efpsignal Threads::Threads)

add_executable(efpsignalduplextests duplex.cpp)
target_link_libraries(efpsignalduplextests efpsignal Threads::Threads )