cmake_minimum_required(VERSION 3.22)
project(MYE_PLUGINS VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE: local checkout via -DJUCE_DIR=..., otherwise fetched
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

# mye_add_plugin(<Target> <Product name> <4-char code> <VST3 category>)
# Reads the version from plugins/<target lowercase>/VERSION
function(mye_add_plugin TARGET PRODUCT CODE CATEGORY)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PLUGIN_VERSION LIMIT_COUNT 1)
    string(TOLOWER ${TARGET} LOWER)
    juce_add_plugin(${TARGET}
        VERSION ${PLUGIN_VERSION}
        COMPANY_NAME "M.Y.E."
        COMPANY_WEBSITE "https://github.com/lobouraniano/mye-plugins"
        BUNDLE_ID "com.mye.${LOWER}"
        PLUGIN_MANUFACTURER_CODE Myee
        PLUGIN_CODE ${CODE}
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "${PRODUCT}"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES Fx ${CATEGORY}
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(${TARGET} PRIVATE src/Plugin.cpp)
    target_include_directories(${TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/common)
    target_compile_definitions(${TARGET} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${TARGET}
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endfunction()

add_subdirectory(plugins/estribo)
add_subdirectory(plugins/martillo)
add_subdirectory(plugins/yunque)
add_subdirectory(plugins/oido)

option(MYE_BUILD_TESTS "Build offline DSP test" OFF)
if(MYE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
