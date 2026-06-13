#include "TTSComponent.h"

namespace
{
    inline juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
}

//==============================================================================
TTSComponent::TTSComponent (SoundboardEngine& e) : engine (e)
{
    backButton.onClick = [this] { if (onBack) onBack(); };
    addAndMakeVisible (backButton);

    title.setText (utf8 ("文字轉語音"), juce::dontSendNotification);
    title.setFont (juce::Font (22.0f, juce::Font::bold));
    addAndMakeVisible (title);

    voiceLabel.setText (utf8 ("語音"), juce::dontSendNotification);
    voiceLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (voiceLabel);

    voiceCombo.setTextWhenNoChoicesAvailable (utf8 ("（找不到語音）"));
    voiceCombo.setTextWhenNothingSelected (utf8 ("（載入語音中…）"));
    addAndMakeVisible (voiceCombo);

    rateLabel.setText (utf8 ("語速"), juce::dontSendNotification);
    rateLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (rateLabel);

    rateSlider.setRange (-10.0, 10.0, 1.0);
    rateSlider.setValue (0.0, juce::dontSendNotification);
    rateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    rateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 22);
    addAndMakeVisible (rateSlider);

    editor.setMultiLine (true);
    editor.setReturnKeyStartsNewLine (true);
    editor.setFont (juce::Font (18.0f));
    editor.setTextToShowWhenEmpty (utf8 ("在這裡輸入要朗讀的文字…"), juce::Colours::grey);
    addAndMakeVisible (editor);

    speakButton.onClick = [this] { speak(); };
    addAndMakeVisible (speakButton);

    setSize (560, 460);   // 獨立視窗的預設大小

    // 背景列出系統已安裝的語音，填進下拉選單
    juce::Thread::launch ([this]
    {
        auto names = listVoices();
        juce::MessageManager::callAsync ([this, names]
        {
            voiceCombo.clear (juce::dontSendNotification);
            int id = 1;
            for (auto& n : names)
                voiceCombo.addItem (n, id++);
            if (voiceCombo.getNumItems() > 0)
                voiceCombo.setSelectedId (1, juce::dontSendNotification);
        });
    });
}

void TTSComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e22));
}

void TTSComponent::resized()
{
    auto r = getLocalBounds().reduced (12);

    auto top = r.removeFromTop (34);
    backButton.setBounds (top.removeFromLeft (84));
    top.removeFromLeft (8);
    title.setBounds (top);

    r.removeFromTop (8);
    auto ctrl = r.removeFromTop (28);
    voiceLabel.setBounds (ctrl.removeFromLeft (44));
    voiceCombo.setBounds (ctrl.removeFromLeft (230));
    ctrl.removeFromLeft (12);
    rateLabel.setBounds (ctrl.removeFromLeft (44));
    rateSlider.setBounds (ctrl);

    r.removeFromTop (8);
    auto bottom = r.removeFromBottom (44);
    speakButton.setBounds (bottom.removeFromRight (140).reduced (0, 4));
    r.removeFromBottom (8);
    editor.setBounds (r);
}

//==============================================================================
void TTSComponent::speak()
{
    const auto text = editor.getText().trim();
    if (text.isEmpty())
        return;

    const auto voice = voiceCombo.getText();
    const int  rate  = (int) rateSlider.getValue();

    speakButton.setEnabled (false);
    speakButton.setButtonText (utf8 ("合成中…"));

    juce::Thread::launch ([this, text, voice, rate]
    {
        const auto wav = synthesizeToWav (text, voice, rate);

        juce::String err;
        AudioClip::Ptr clip = (wav != juce::File()) ? engine.loadClip (wav, err) : nullptr;
        if (wav != juce::File())
            wav.deleteFile();

        juce::MessageManager::callAsync ([this, clip]
        {
            speakButton.setEnabled (true);
            speakButton.setButtonText (utf8 ("朗讀"));

            if (clip != nullptr)
                engine.trigger (clip, 1.0f);
            else
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                    utf8 ("文字轉語音"),
                    utf8 ("合成失敗。請確認系統有可用的語音引擎。"));
        });
    });
}

juce::StringArray TTSComponent::listVoices()
{
    const juce::String script =
        "Add-Type -AssemblyName System.Speech; "
        "(New-Object System.Speech.Synthesis.SpeechSynthesizer).GetInstalledVoices() "
        "| ForEach-Object { $_.VoiceInfo.Name }";

    juce::StringArray cmd { "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script };
    juce::ChildProcess proc;
    juce::StringArray names;
    if (proc.start (cmd))
    {
        names.addLines (proc.readAllProcessOutput());
        names.removeEmptyStrings();
        for (auto& n : names)
            n = n.trim();
    }
    return names;
}

juce::File TTSComponent::synthesizeToWav (const juce::String& text, const juce::String& voice, int rate)
{
    auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory);
    const auto id = juce::String::toHexString (juce::Random::getSystemRandom().nextInt64());
    auto txt = temp.getChildFile ("tts_" + id + ".txt");
    auto wav = temp.getChildFile ("tts_" + id + ".wav");

    txt.replaceWithText (text);   // UTF-8

    juce::String script;
    script << "Add-Type -AssemblyName System.Speech; "
           << "$t = [IO.File]::ReadAllText('" << txt.getFullPathName() << "', [Text.Encoding]::UTF8); "
           << "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; ";
    if (voice.isNotEmpty())
        script << "try { $s.SelectVoice('" << voice.replace ("'", "''") << "') } catch {}; ";
    script << "$s.Rate = " << juce::String (rate) << "; "
           << "$s.SetOutputToWaveFile('" << wav.getFullPathName() << "'); "
           << "$s.Speak($t); $s.Dispose();";

    juce::StringArray cmd { "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script };
    juce::ChildProcess proc;
    if (proc.start (cmd))
        proc.waitForProcessToFinish (30000);

    txt.deleteFile();
    return (wav.existsAsFile() && wav.getSize() > 0) ? wav : juce::File();
}
