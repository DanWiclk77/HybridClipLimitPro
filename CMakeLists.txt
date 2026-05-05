cmake_minimum_required(VERSION 3.15)

project(LoudnessCatalyst VERSION 1.0.0)

# Incluir JUCE
add_subdirectory(JUCE)

juce_add_plugin(LoudnessCatalyst
    COMPANY_NAME "AI Audio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "AIAU"
    PLUGIN_CODE "LDCT"
    FORMATS VST3
    PRODUCT_NAME "Loudness Catalyst")

juce_generate_juce_header(LoudnessCatalyst)

target_sources(LoudnessCatalyst
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/LoudnessEngine.h)

target_compile_definitions(LoudnessCatalyst
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(LoudnessCatalyst
    PRIVATE
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
        juce::juce_gui_extra)
