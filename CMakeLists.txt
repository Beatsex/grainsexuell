cmake_minimum_required(VERSION 3.22)
project(grainsexuell VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE Catch2)

# Sources shared by the plugin, the tests and the render tool. A list instead of a static
# library: JUCE module sources attach to each consuming target, so a library linked into the
# plugin would duplicate them. Core sources must not use JucePlugin_* macros (plugin target only).
# Add Source/dsp/*.cpp here as the DSP classes appear.
set(GRAIN_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Parameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/dsp/Capture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/dsp/Cloud.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/dsp/Mosaic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/dsp/Reverb.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/dsp/Tap.cpp)
set(GRAIN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set(GRAIN_JUCE_LIBS
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
set(GRAIN_JUCE_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

set(GRAIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND GRAIN_FORMATS AU)
endif()

juce_add_plugin(grainsexuell
    FORMATS ${GRAIN_FORMATS}
    PRODUCT_NAME "grainsexuell"
    COMPANY_NAME "grainsexuell"
    PLUGIN_MANUFACTURER_CODE Gsxl
    PLUGIN_CODE Gr01
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(grainsexuell PRIVATE ${GRAIN_CORE_SOURCES} Source/PluginEntry.cpp)
target_include_directories(grainsexuell PRIVATE ${GRAIN_INCLUDE_DIR})
target_compile_definitions(grainsexuell PUBLIC ${GRAIN_JUCE_DEFS})
target_link_libraries(grainsexuell PRIVATE ${GRAIN_JUCE_LIBS})

enable_testing()
foreach(sub tests tools/render)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${sub}/CMakeLists.txt)
        add_subdirectory(${sub})
    endif()
endforeach()
