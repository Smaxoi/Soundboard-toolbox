#pragma once

#include <JuceHeader.h>
#include "SoundboardEngine.h"

//==============================================================================
/**
    文字轉語音模組：選語音 + 語速，輸入文字 -> 用 Windows 內建語音 (System.Speech)
    合成成 wav -> 透過共用的音訊引擎播放（一樣會送到 CABLE / 監聽）。
    語音清單是「動態」列出系統已安裝的，之後在 Windows 裝更多語音就會自動出現。
*/
class TTSComponent : public juce::Component
{
public:
    explicit TTSComponent (SoundboardEngine& engine);

    std::function<void()> onBack;        // 由外殼設定：返回主選單

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void             speak();
    juce::StringArray listVoices();                                                       // 背景執行緒
    juce::File       synthesizeToWav (const juce::String& text, const juce::String& voice, int rate);

    SoundboardEngine& engine;

    juce::TextButton backButton  { juce::CharPointer_UTF8 ("← 主選單") };
    juce::Label      title;
    juce::Label      voiceLabel, rateLabel;
    juce::ComboBox   voiceCombo;
    juce::Slider     rateSlider;
    juce::TextEditor editor;
    juce::TextButton speakButton { juce::CharPointer_UTF8 ("朗讀") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TTSComponent)
};
