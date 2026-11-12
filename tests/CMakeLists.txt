juce_add_console_app(MyeDspTest PRODUCT_NAME "MyeDspTest")
target_sources(MyeDspTest PRIVATE main.cpp
    ../plugins/estribo/src/Plugin.cpp ../plugins/martillo/src/Plugin.cpp
    ../plugins/yunque/src/Plugin.cpp ../plugins/oido/src/Plugin.cpp)
target_include_directories(MyeDspTest PRIVATE ${PROJECT_SOURCE_DIR}/common)
target_compile_definitions(MyeDspTest PRIVATE MYE_TEST=1 JucePlugin_VersionString="test"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(MyeDspTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
