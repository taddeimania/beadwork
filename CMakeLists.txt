cmake_minimum_required(VERSION 3.22)

# macOS 11+. Builds for the machine you're on by default; for a universal
# binary pass -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64". Set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Beadwork VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------------
# By default JUCE is downloaded on first configure. To use a local checkout:
#   cmake -B build -DBEADWORK_JUCE_DIR=/path/to/JUCE
set(BEADWORK_JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")

if(BEADWORK_JUCE_DIR)
    add_subdirectory(${BEADWORK_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin -------------------------------------------------------------------
# Copy the built plugin into the system VST3 folder after each build:
#   macOS   ~/Library/Audio/Plug-Ins/VST3
#   Linux   ~/.vst3
#   Windows C:\Program Files\Common Files\VST3  (needs an administrator shell,
#           so it is off by default there)
if(WIN32)
    set(_beadwork_copy_default OFF)
else()
    set(_beadwork_copy_default ON)
endif()
option(BEADWORK_COPY_PLUGIN "Install the plugin into the system VST3 folder after building" ${_beadwork_copy_default})

juce_add_plugin(Beadwork
    COMPANY_NAME                "Taddei"
    BUNDLE_ID                   com.taddei.beadwork
    PLUGIN_MANUFACTURER_CODE    Tadd
    PLUGIN_CODE                 Bdwk
    PRODUCT_NAME                "Beadwork"
    FORMATS                     VST3
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Generator
    COPY_PLUGIN_AFTER_BUILD     ${BEADWORK_COPY_PLUGIN})

set(BEADWORK_ENGINE_SOURCES
    Source/Engine/Engine.cpp)

set(BEADWORK_SOURCES
    ${BEADWORK_ENGINE_SOURCES}
    Source/State.cpp
    Source/PresetManager.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Widgets.cpp
    Source/UI/Lane.cpp
    Source/UI/Bank.cpp)

target_sources(Beadwork PRIVATE ${BEADWORK_SOURCES})

target_include_directories(Beadwork PRIVATE Source)

target_compile_definitions(Beadwork PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Beadwork
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Engine tests (no JUCE needed) --------------------------------------------
enable_testing()
add_executable(BeadworkEngineTests Tests/EngineTests.cpp ${BEADWORK_ENGINE_SOURCES})
target_include_directories(BeadworkEngineTests PRIVATE Source)
add_test(NAME engine COMMAND BeadworkEngineTests)

# --- Optional dev tool --------------------------------------------------------
#   BeadworkSnapshot  render the panel to a PNG, print a kit's MIDI as a grid,
#                     or round-trip a session (headless)
option(BEADWORK_BUILD_TOOLS "Build the snapshot tool" OFF)
if(BEADWORK_BUILD_TOOLS)
    juce_add_console_app(BeadworkSnapshot PRODUCT_NAME "BeadworkSnapshot")
    target_sources(BeadworkSnapshot PRIVATE Tools/Snapshot.cpp ${BEADWORK_SOURCES})
    target_include_directories(BeadworkSnapshot PRIVATE Source)
    target_compile_definitions(BeadworkSnapshot PRIVATE
        JucePlugin_Name="Beadwork"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BeadworkSnapshot PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
