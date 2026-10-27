cmake_minimum_required(VERSION 3.22)

project(AvelTime VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
endif()
if(MSVC)
    # Static runtime: the plugin runs without the Visual C++ Redistributable.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE: -DAVEL_JUCE_DIR=<path>, or ../JUCE next to the project, or downloaded automatically.
if(DEFINED AVEL_JUCE_DIR)
    add_subdirectory(${AVEL_JUCE_DIR} JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(AVEL_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(AVEL_FORMATS VST3 Standalone)
else()
    set(AVEL_FORMATS VST3)
endif()

juce_add_plugin(AvelTime
    COMPANY_NAME "Avel"
    COMPANY_WEBSITE "https://github.com/johntok0001"
    BUNDLE_ID com.avel.time
    PLUGIN_MANUFACTURER_CODE Avel
    PLUGIN_CODE AvTm
    FORMATS ${AVEL_FORMATS}
    PRODUCT_NAME "Avel Time"
    DESCRIPTION "Stereo creative delay"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    HARDENED_RUNTIME_ENABLED TRUE)

juce_add_binary_data(AvelTimeFonts SOURCES
    Resources/Fonts/Inter-Regular.otf
    Resources/Fonts/Inter-Medium.otf
    Resources/Fonts/Inter-SemiBold.otf)

set(AVEL_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/UI/Views.cpp)

set(AVEL_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
if(UNIX AND NOT APPLE)
    list(APPEND AVEL_DEFINES JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

set(AVEL_MODULES
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics)

target_sources(AvelTime PRIVATE ${AVEL_SOURCES})
target_include_directories(AvelTime PRIVATE Source)
target_compile_definitions(AvelTime PUBLIC ${AVEL_DEFINES})
target_link_libraries(AvelTime
    PRIVATE ${AVEL_MODULES} AvelTimeFonts
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline test harness (not part of the release build)
option(AVEL_BUILD_TESTS "Build the offline test harness" OFF)
if(AVEL_BUILD_TESTS)
    juce_add_console_app(AvelTimeTest PRODUCT_NAME "AvelTimeTest")
    target_sources(AvelTimeTest PRIVATE Tests/AvelTimeTest.cpp ${AVEL_SOURCES})
    target_include_directories(AvelTimeTest PRIVATE Source)
    target_compile_definitions(AvelTimeTest PRIVATE ${AVEL_DEFINES} JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Avel Time" JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(AvelTimeTest PRIVATE ${AVEL_MODULES} AvelTimeFonts juce::juce_recommended_config_flags)
endif()
