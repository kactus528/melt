cmake_minimum_required(VERSION 3.22)
project(Melt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: uses ./JUCE if present, otherwise downloads it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# When cross-compiling, the VST3 manifest helper can't run on the build machine.
if(CMAKE_CROSSCOMPILING)
    set(MELT_MANIFEST FALSE)
else()
    set(MELT_MANIFEST TRUE)
endif()

# Mac also gets an Audio Unit (for Logic / GarageBand).
if(APPLE)
    set(MELT_FORMATS VST3 AU Standalone)
else()
    set(MELT_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Melt
    COMPANY_NAME "Andrew"
    PRODUCT_NAME "Melt"
    BUNDLE_ID com.andrew.melt
    PLUGIN_MANUFACTURER_CODE Andw
    PLUGIN_CODE Melt
    FORMATS ${MELT_FORMATS}
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion Delay
    VST3_AUTO_MANIFEST ${MELT_MANIFEST}
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Melt PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Melt PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MINGW)
    # MSVC links these automatically via #pragma comment(lib); MinGW needs them listed.
    target_link_libraries(Melt PRIVATE
        version ws2_32 shlwapi winmm wininet imm32 comdlg32 ole32 oleaut32 uuid gdi32 user32
        shell32 rpcrt4 dxgi d2d1 dwrite d3d11 dwmapi setupapi winhttp crypt32
        uxtheme dbghelp ksuser dcomp sapi)
    # comctl32 is deliberately not linked: see Source/MinGWCompat.cpp
    target_sources(Melt PRIVATE Source/MinGWCompat.cpp)
endif()

target_link_libraries(Melt
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
