cmake_minimum_required(VERSION 3.22)
project(Wav2FlacYeah VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(Wav2FlacYeah
    COMPANY_NAME    "Wav2FlacYeah"
    PRODUCT_NAME    "Wav2FlacYeah"
    VERSION         "1.0.0"
    BUNDLE_ID       "com.wav2flacyeah.app"
)

target_sources(Wav2FlacYeah PRIVATE
    src/Main.cpp
    src/MainWindow.cpp
    src/ConverterComponent.cpp
    src/ConversionThread.cpp
)

target_compile_definitions(Wav2FlacYeah PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Wav2FlacYeah,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Wav2FlacYeah,JUCE_VERSION>"
)

target_link_libraries(Wav2FlacYeah PRIVATE
    juce::juce_audio_formats
    juce::juce_audio_basics
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
