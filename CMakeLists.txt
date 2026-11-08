cmake_minimum_required(VERSION 3.22)
project(OrbitReverb VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE: usa una copia locale (-DJUCE_DIR=...) oppure la scarica automaticamente ---
set(JUCE_DIR "" CACHE PATH "Percorso di una copia locale di JUCE (opzionale)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE_build)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(ORBIT_BUILD_TESTS "Compila anche i test offline" ON)

juce_add_plugin(OrbitReverb
    COMPANY_NAME "Orbit Audio"
    PRODUCT_NAME "ORBIT REVERB"
    PLUGIN_MANUFACTURER_CODE Orbt
    PLUGIN_CODE OrRv
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb)

set(ORBIT_SOURCES
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/ReverbEngine.cpp
    Source/dsp/OrbitMotion.cpp
    Source/gui/OrbitLookAndFeel.cpp
    Source/gui/OrbitWidgets.cpp)

target_sources(OrbitReverb PRIVATE ${ORBIT_SOURCES})

target_compile_definitions(OrbitReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(OrbitReverb
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(ORBIT_BUILD_TESTS)
    juce_add_console_app(OrbitTests PRODUCT_NAME "OrbitTests")
    target_sources(OrbitTests PRIVATE tests/OrbitTests.cpp ${ORBIT_SOURCES})
    target_compile_definitions(OrbitTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1
        ORBIT_TEST_BUILD=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(OrbitTests PRIVATE juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
