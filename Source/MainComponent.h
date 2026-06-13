#pragma once

#include <JuceHeader.h>
#include "SoundboardComponent.h"
#include "TTSComponent.h"

//==============================================================================
/**
    App 主選單視窗。點功能會各自開一個「獨立的跳出視窗」，可同時開多個。
    各功能模組在 app 啟動時就建立、一直存活（所以共用的音訊引擎與全域熱鍵持續有效）；
    關閉功能視窗只是隱藏它，不會銷毀模組。
*/
class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    class FeatureWindow;                                  // 功能用的獨立視窗（定義在 .cpp）

    void openFeature (std::unique_ptr<FeatureWindow>& win, juce::Component& content, const juce::String& title);
    void openSoundboard();
    void openTTS();

    // 主選單 UI
    juce::Label      titleLabel, subtitle;
    juce::TextButton sbBtn, ttsBtn, vcBtn;

    // 功能模組（整個 app 生命週期都在 -> 共用引擎不會被銷毀）
    std::unique_ptr<SoundboardComponent> soundboard;
    std::unique_ptr<TTSComponent>        tts;
    std::unique_ptr<FeatureWindow>       soundboardWin, ttsWin;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
