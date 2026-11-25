cmake_minimum_required(VERSION 3.22)
project(TunEC VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# macOS only. 10.13 is the oldest target JUCE 8 supports; binaries are universal
# (Intel x86_64 + Apple Silicon arm64).
if(NOT APPLE)
  message(WARNING "TunEC is configured for macOS. Use -DTUNEC_BUILD_PLUGIN=OFF to build only the DSP tests elsewhere.")
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

option(TUNEC_BUILD_PLUGIN "Build the JUCE plugin" ON)
option(TUNEC_BUILD_TESTS  "Build the stand-alone DSP tests" ON)

# ---- DSP engine (pure C++, no JUCE) ----------------------------------------
add_library(tunec_dsp STATIC Source/DSP/PitchEngine.cpp)
target_include_directories(tunec_dsp PUBLIC Source)

if(TUNEC_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/EngineTest.cpp")
  enable_testing()
  add_executable(tunec_engine_test Tests/EngineTest.cpp)
  target_link_libraries(tunec_engine_test PRIVATE tunec_dsp)
  add_test(NAME engine COMMAND tunec_engine_test)
endif()

# ---- Plugin -----------------------------------------------------------------
if(TUNEC_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)

  set(TUNEC_FORMATS AU VST3 Standalone)
  # Optional AAX (Pro Tools): needs the Avid AAX SDK + PACE/iLok signing.
  if(DEFINED ENV{AAX_SDK_PATH} OR DEFINED AAX_SDK_PATH)
    if(NOT DEFINED AAX_SDK_PATH)
      set(AAX_SDK_PATH $ENV{AAX_SDK_PATH})
    endif()
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND TUNEC_FORMATS AAX)
  endif()

  juce_add_plugin(TunEC
    COMPANY_NAME "TunEC Audio"
    BUNDLE_ID com.tunecaudio.TunEC
    PLUGIN_MANUFACTURER_CODE Tnca
    PLUGIN_CODE Tnc1
    PRODUCT_NAME "TunEC"
    FORMATS ${TUNEC_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    VST3_CATEGORIES Fx "Pitch Shift"
    AAX_CATEGORY 0)

  target_sources(TunEC PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h)

  target_compile_definitions(TunEC PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

  target_link_libraries(TunEC
    PRIVATE
      tunec_dsp
      juce::juce_audio_utils
    PUBLIC
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags
      juce::juce_recommended_warning_flags)
endif()
