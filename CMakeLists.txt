cmake_minimum_required(VERSION 3.22)
project(MYE_PLUGINS VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary (Apple Silicon + Intel) on macOS
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE: use a local checkout if JUCE_DIR is given, otherwise fetch it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

add_subdirectory(plugins/eco)
