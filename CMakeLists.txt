cmake_minimum_required(VERSION 3.22)

project(VixBasior VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE
# Pobierz JUCE (np. git submodule w folderze "JUCE" obok tego pliku) albo
# ustaw JUCE_DIR na już zainstalowaną kopię JUCE 7/8.
#   git clone https://github.com/juce-framework/JUCE.git
# ---------------------------------------------------------------------------
add_subdirectory(JUCE)

juce_add_plugin(VixBasior
    COMPANY_NAME "VixBasior Audio"
    PLUGIN_MANUFACTURER_CODE Vixb
    PLUGIN_CODE Vix1
    FORMATS VST3 Standalone
    PRODUCT_NAME "VixBasior"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Instrument" "Synth"
)

target_compile_definitions(VixBasior
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DSP_USE_SHARED_FFTW=0
)

target_sources(VixBasior
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Analyzer/VixaAnalyzer.cpp
)

target_include_directories(VixBasior PRIVATE Source)

target_link_libraries(VixBasior
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_processors
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# SIMD / optymalizacja (MSVC + Windows x64)
if (MSVC)
    target_compile_options(VixBasior PRIVATE /arch:AVX2 /fp:fast)
endif()
