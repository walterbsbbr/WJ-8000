cmake_minimum_required(VERSION 3.22)
project(DW8000 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")

# ── JUCE ────────────────────────────────────────────────────────────────────
add_subdirectory(JUCE)

# ── Factory bank binary data ─────────────────────────────────────────────────
juce_add_binary_data(DW8000Factory
    HEADER_NAME "DW8000FactoryData.h"
    NAMESPACE   DW8000FactoryData
    SOURCES
        "Resources/Factory/FactoryBank.syx"
)

# ── Image binary data ────────────────────────────────────────────────────────
juce_add_binary_data(DW8000Images
    HEADER_NAME "DW8000ImageData.h"
    NAMESPACE   DW8000ImageData
    SOURCES
        "Resources/Images/KEYS.png"
)

# ── Wavetable binary data ────────────────────────────────────────────────────
juce_add_binary_data(DW8000Wavetables
    HEADER_NAME "DW8000WavetableData.h"
    NAMESPACE   DW8000WavetableData
    SOURCES
        "Resources/Wavetables/DW8000 1.wav"
        "Resources/Wavetables/DW8000 2.wav"
        "Resources/Wavetables/DW8000 3.wav"
        "Resources/Wavetables/DW8000 4.wav"
        "Resources/Wavetables/DW8000 5.wav"
        "Resources/Wavetables/DW8000 6.wav"
        "Resources/Wavetables/DW8000 7.wav"
        "Resources/Wavetables/DW8000 8.wav"
        "Resources/Wavetables/DW8000 9.wav"
        "Resources/Wavetables/DW8000 10.wav"
        "Resources/Wavetables/DW8000 11.wav"
        "Resources/Wavetables/DW8000 12.wav"
        "Resources/Wavetables/DW8000 13.wav"
        "Resources/Wavetables/DW8000 14.wav"
        "Resources/Wavetables/DW8000 15.wav"
        "Resources/Wavetables/DW8000 16.wav"
)

# ── Plugin target ────────────────────────────────────────────────────────────
juce_add_plugin(DW8000
    COMPANY_NAME             "PersonalStudy"
    PLUGIN_MANUFACTURER_CODE Ps00
    PLUGIN_CODE              Dw80
    FORMATS                  AU VST3 Standalone
    PRODUCT_NAME             "WJ-8000"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        TRUE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          "Instrument" "Synth"
    AU_MAIN_TYPE             "kAudioUnitType_MusicDevice"
    HARDENED_RUNTIME_ENABLED TRUE
    MICROPHONE_PERMISSION_ENABLED FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
)

# ── Sources ──────────────────────────────────────────────────────────────────
target_sources(DW8000 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h

    Source/Synth/DW8000Patch.cpp
    Source/Synth/DW8000Patch.h
    Source/Synth/DW8000Voice.cpp
    Source/Synth/DW8000Voice.h
    Source/Synth/DW8000Engine.cpp
    Source/Synth/DW8000Engine.h
    Source/Synth/DW8000MG.cpp
    Source/Synth/DW8000MG.h
    Source/Synth/DW8000Delay.cpp
    Source/Synth/DW8000Delay.h
    Source/Synth/DW8000Arpeggiator.cpp
    Source/Synth/DW8000Arpeggiator.h

    Source/Synth/DWGS/DWGSWavetableBank.cpp
    Source/Synth/DWGS/DWGSWavetableBank.h
    Source/Synth/DWGS/DWGSOscillator.cpp
    Source/Synth/DWGS/DWGSOscillator.h

    Source/Synth/Filter/NJM2069.cpp
    Source/Synth/Filter/NJM2069.h

    Source/Synth/Envelope/ADBSSR.cpp
    Source/Synth/Envelope/ADBSSR.h

    Source/MIDI/DW8000SysEx.cpp
    Source/MIDI/DW8000SysEx.h
)

# ── JUCE module dependencies ─────────────────────────────────────────────────
target_link_libraries(DW8000
    PRIVATE
        DW8000Wavetables
        DW8000Images
        DW8000Factory
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ── Compile options ──────────────────────────────────────────────────────────
target_compile_options(DW8000 PRIVATE
    -O2
    -ffast-math
    $<$<CXX_COMPILER_ID:AppleClang,Clang>:-march=native>
)

# ── Definitions ──────────────────────────────────────────────────────────────
target_compile_definitions(DW8000 PUBLIC
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
