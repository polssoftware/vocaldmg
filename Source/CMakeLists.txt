cmake_minimum_required(VERSION 3.22)
project(VocalDMG VERSION 1.3.0 LANGUAGES C CXX)
set(JUCE_DIR "" CACHE PATH "Path to JUCE source")
if(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(VocalDMG COMPANY_NAME "polS Software"
    BUNDLE_ID software.pols.vocaldmg PLUGIN_MANUFACTURER_CODE PolS PLUGIN_CODE VcDm
    FORMATS VST3 PRODUCT_NAME "VOCAL DMG" VST3_CATEGORIES Fx)
target_sources(VocalDMG PRIVATE Plugin.cpp)
target_compile_features(VocalDMG PRIVATE cxx_std_17)
target_compile_definitions(VocalDMG PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(VocalDMG PRIVATE juce::juce_audio_utils
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
