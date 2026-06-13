#include "MainComponent.h"

namespace
{
    inline juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
}

//==============================================================================
//  FeatureWindow —— 各功能的獨立跳出視窗（關閉=隱藏，不銷毀內容）
//==============================================================================
class MainComponent::FeatureWindow : public juce::DocumentWindow
{
public:
    FeatureWindow (const juce::String& name, juce::Component* content)
        : DocumentWindow (name,
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                              .findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentNonOwned (content, true);   // 內容由 MainComponent 擁有，視窗只負責顯示
        setResizable (true, true);
        centreWithSize (getWidth(), getHeight());
    }

    void closeButtonPressed() override { setVisible (false); }   // 只隱藏，保留模組與其狀態

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FeatureWindow)
};

//==============================================================================
MainComponent::MainComponent()
{
    titleLabel.setText (utf8 ("VoiceMod 工具箱"), juce::dontSendNotification);
    titleLabel.setFont (juce::Font (28.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    subtitle.setText (utf8 ("選一個功能（會開成獨立視窗，可同時開多個）"), juce::dontSendNotification);
    subtitle.setJustificationType (juce::Justification::centred);
    subtitle.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (subtitle);

    sbBtn.setButtonText (utf8 ("音效板"));
    sbBtn.onClick = [this] { openSoundboard(); };
    addAndMakeVisible (sbBtn);

    ttsBtn.setButtonText (utf8 ("文字轉語音"));
    ttsBtn.onClick = [this] { openTTS(); };
    addAndMakeVisible (ttsBtn);

    vcBtn.setButtonText (utf8 ("變聲器（即將推出）"));
    vcBtn.setEnabled (false);
    addAndMakeVisible (vcBtn);

    // 模組在啟動時就建立（音效板擁有引擎、TTS 共用它）
    soundboard = std::make_unique<SoundboardComponent>();
    tts        = std::make_unique<TTSComponent> (soundboard->getEngine());

    soundboard->onBack = [this] { if (soundboardWin != nullptr) soundboardWin->setVisible (false); };
    tts->onBack        = [this] { if (ttsWin != nullptr)        ttsWin->setVisible (false); };

    setSize (460, 440);
}

MainComponent::~MainComponent() = default;

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e22));
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (30);
    titleLabel.setBounds (r.removeFromTop (46));
    subtitle.setBounds (r.removeFromTop (24));
    r.removeFromTop (20);

    const int bw = juce::jmin (320, r.getWidth());
    const int cx = r.getCentreX();
    auto place = [&] (juce::Button& b)
    {
        auto row = r.removeFromTop (54);
        b.setBounds (cx - bw / 2, row.getY(), bw, 48);
        r.removeFromTop (14);
    };

    place (sbBtn);
    place (ttsBtn);
    place (vcBtn);
}

//==============================================================================
void MainComponent::openFeature (std::unique_ptr<FeatureWindow>& win, juce::Component& content, const juce::String& title)
{
    if (win == nullptr)
        win = std::make_unique<FeatureWindow> (title, &content);

    win->setVisible (true);
    win->toFront (true);
}

void MainComponent::openSoundboard() { openFeature (soundboardWin, *soundboard, utf8 ("VoiceMod 音效板")); }
void MainComponent::openTTS()        { openFeature (ttsWin,        *tts,        utf8 ("VoiceMod 文字轉語音")); }
