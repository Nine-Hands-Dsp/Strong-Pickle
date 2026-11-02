# Outil hors plugin : compile le processeur + la fenetre de STRONG PICKLE sous Linux et exporte des captures PNG (apercu, notice)
cmake_minimum_required(VERSION 3.22)
project(PickleRender VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${JUCE_DIR} JUCE)
set(W ${CMAKE_CURRENT_SOURCE_DIR}/../..)
juce_add_binary_data(StrongPickleAssets SOURCES
  ${W}/background.jpg ${W}/toggle_strip.png ${W}/glow_yellow.png ${W}/pickle_glow.png ${W}/Poppins-Medium.ttf)
juce_add_console_app(PickleRender PRODUCT_NAME "PickleRender")
target_sources(PickleRender PRIVATE main.cpp ${W}/PluginProcessor.cpp ${W}/PluginEditor.cpp)
target_include_directories(PickleRender PRIVATE ${W})
target_compile_definitions(PickleRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0 JucePlugin_Name="Strong Pickle" JucePlugin_Build_VST3=0
  JucePlugin_VersionString="1.1.0" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(PickleRender PRIVATE StrongPickleAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
