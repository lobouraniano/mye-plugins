juce_add_plugin(Eco
    COMPANY_NAME "M.Y.E."
    COMPANY_WEBSITE "https://luppi.cargo.site/"
    BUNDLE_ID "com.mye.eco"
    PLUGIN_MANUFACTURER_CODE Myee
    PLUGIN_CODE Eco1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "MYE Eco"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Eco PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_compile_definitions(Eco PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Eco
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
