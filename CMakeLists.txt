cmake_minimum_required(VERSION 3.22)

project(VoiceModSoundboard VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 散佈用：靜態連結 MSVC 執行階段 -> 產出的 exe 不需要 Visual C++ Redistributable
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------------------
# 自動下載 JUCE（第一次 build 會從 GitHub 拉下來，需幾分鐘）
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4          # 抓取失敗的話改成 7.0.12 或 master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# 音效板：一個有視窗的 GUI 程式
# ---------------------------------------------------------------------------
juce_add_gui_app(Soundboard
    PRODUCT_NAME "Soundboard"
    COMPANY_NAME "VoiceMod")

# 讓我們可以用 #include <JuceHeader.h> 一次帶入所有模組（對初學者最省事）
juce_generate_juce_header(Soundboard)

target_sources(Soundboard PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/SoundboardEngine.cpp
    Source/SoundboardComponent.cpp
    Source/GlobalHotkeys.cpp
    Source/TTSComponent.cpp)

target_compile_definitions(Soundboard PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Soundboard,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Soundboard,JUCE_VERSION>")

# MSVC 在繁中系統預設用 Big5(950) 讀原始碼，會把 UTF-8 的中文註解讀壞 ->
# 強制用 UTF-8 讀檔，根治那一整排 C4819 與連帶的語法錯誤。
if (MSVC)
    target_compile_options(Soundboard PRIVATE /utf-8)
endif()

target_link_libraries(Soundboard PRIVATE
    juce::juce_audio_utils       # 音訊裝置 + 裝置選擇 UI
    juce::juce_audio_formats     # 讀 wav/mp3/flac/ogg
    juce::juce_dsp               # 麥克風降噪用的高通濾波
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# 註：Source/VoiceEngine.* 是先前的「即時變聲效果」引擎，暫時停在這、還沒納入這個 app。
