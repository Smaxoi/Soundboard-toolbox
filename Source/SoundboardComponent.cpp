#include "SoundboardComponent.h"

namespace
{
    // JUCE 的 String(const char*) 會把位元組當 Latin-1；中文要用 UTF-8 解才正確。
    inline juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
}

//==============================================================================
//  HotkeyButton —— 可綁全域熱鍵的按鈕（停止鈕用）
//==============================================================================
SoundboardComponent::HotkeyButton::HotkeyButton (const juce::String& text)
    : juce::TextButton (text)
{
    setWantsKeyboardFocus (true);
}

void SoundboardComponent::HotkeyButton::clicked()
{
    if (onTriggered) onTriggered();
}

void SoundboardComponent::HotkeyButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showMenu();
    else juce::TextButton::mouseDown (e);   // 正常左鍵點擊
}

bool SoundboardComponent::HotkeyButton::keyPressed (const juce::KeyPress& kp)
{
    if (! awaitingHotkey)
        return false;

    if (kp == juce::KeyPress::escapeKey) { awaitingHotkey = false; repaint(); return true; }

    int vk = 0, mods = 0;
    if (GlobalHotkeys::keyPressToVk (kp, vk, mods))
    {
        awaitingHotkey = false;
        hotkeyVk = vk; hotkeyMods = mods;
        if (onHotkeyChanged) onHotkeyChanged (vk, mods);
        repaint();
    }
    return true;
}

void SoundboardComponent::HotkeyButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    juce::TextButton::paintButton (g, over, down);

    if (awaitingHotkey)
    {
        g.setColour (juce::Colours::yellow);
        g.setFont (11.0f);
        g.drawText (utf8 ("按鍵…"), getLocalBounds(), juce::Justification::centred);
    }
    else if (hotkeyVk != 0)
    {
        g.setColour (juce::Colours::yellow.withAlpha (0.9f));
        g.setFont (10.0f);
        g.drawText (GlobalHotkeys::describe (hotkeyVk, hotkeyMods),
                    getLocalBounds().reduced (3), juce::Justification::bottomRight);
    }
}

void SoundboardComponent::HotkeyButton::showMenu()
{
    juce::PopupMenu m;
    m.addItem (1, hotkeyVk == 0 ? utf8 ("設定熱鍵…") : utf8 ("變更熱鍵…"));
    m.addItem (2, utf8 ("清除熱鍵"), hotkeyVk != 0);
    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
    {
        if (r == 1)      { awaitingHotkey = true; grabKeyboardFocus(); repaint(); }
        else if (r == 2) { hotkeyVk = 0; hotkeyMods = 0; if (onHotkeyChanged) onHotkeyChanged (0, 0); repaint(); }
    });
}

//==============================================================================
//  Pad —— 單一音效格
//==============================================================================
SoundboardComponent::Pad::Pad (SoundboardComponent& o) : owner (o)
{
    setWantsKeyboardFocus (true);   // 設定熱鍵時要能接收按鍵
}

void SoundboardComponent::Pad::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (4.0f);

    g.setColour (clip != nullptr ? juce::Colour (0xff3a6ea5)    // 有音效：藍
                                 : juce::Colour (0xff2a2a2e));   // 空的：深灰
    g.fillRoundedRectangle (bounds, 8.0f);

    if (flash > 0.0f)   // 按下時的高亮，會淡出
    {
        g.setColour (juce::Colours::white.withAlpha (0.55f * flash));
        g.fillRoundedRectangle (bounds, 8.0f);
    }

    const juce::String label = (clip == nullptr) ? juce::String ("+")
                             : (customName.isNotEmpty() ? customName : clip->name);
    g.setColour (juce::Colours::white.withAlpha (clip != nullptr ? 0.95f : 0.4f));
    g.setFont (clip != nullptr ? 15.0f : 30.0f);
    g.drawFittedText (label, getLocalBounds().reduced (8), juce::Justification::centred, 3);

    // 左上角顯示綁定的熱鍵
    if (hotkeyVk != 0)
    {
        g.setColour (juce::Colours::yellow.withAlpha (0.9f));
        g.setFont (12.0f);
        g.drawText (GlobalHotkeys::describe (hotkeyVk, hotkeyMods),
                    getLocalBounds().reduced (7).removeFromTop (16),
                    juce::Justification::topLeft);
    }

    // 迴圈標記（右上角 ↻）
    if (loopMode)
    {
        g.setColour (isLooping ? juce::Colours::limegreen : juce::Colours::white.withAlpha (0.55f));
        g.setFont (16.0f);
        g.drawText (utf8 ("↻"), getLocalBounds().reduced (7).removeFromTop (18),
                    juce::Justification::topRight);
    }

    // 音量非 100% -> 右下角顯示倍率
    if (clip != nullptr && std::abs (padGain - 1.0f) > 0.01f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (11.0f);
        g.drawText (juce::String (padGain, 2) + "x",
                    getLocalBounds().reduced (6), juce::Justification::bottomRight);
    }

    // 正在迴圈：綠色外框；拖放游標在上方：黃色外框
    if (isLooping)
    {
        g.setColour (juce::Colours::limegreen);
        g.drawRoundedRectangle (bounds, 8.0f, 2.0f);
    }
    if (dragOver)
    {
        g.setColour (juce::Colours::yellow);
        g.drawRoundedRectangle (bounds, 8.0f, 2.0f);
    }

    // 等待使用者按鍵時的提示
    if (awaitingHotkey)
    {
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 8.0f);
        g.setColour (juce::Colours::yellow);
        g.setFont (14.0f);
        g.drawFittedText (utf8 ("按一個鍵…\n(可加 Ctrl/Alt/Shift；Esc 取消)"),
                          getLocalBounds().reduced (6), juce::Justification::centred, 3);
    }
}

void SoundboardComponent::Pad::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())   // 右鍵 / 觸控長按
        showMenu();
    else
        clicked();
}

void SoundboardComponent::Pad::clicked()
{
    if (clip == nullptr)
        loadFile();   // 空格 -> 載入
    else
        play();       // 有音效 -> 播放
}

void SoundboardComponent::Pad::assign (AudioClip::Ptr clipToUse, const juce::File& sourceFile)
{
    clip = clipToUse;
    file = sourceFile;
    customName = {};        // 載入新音效時清掉舊的自訂名稱（loadPads 之後會再還原）
    isLooping = false;
    repaint();
}

void SoundboardComponent::Pad::setCustomName (const juce::String& n)
{
    customName = n;
    repaint();
}

void SoundboardComponent::Pad::clearSound()
{
    clip = nullptr;
    file = juce::File();
    customName = {};
    padGain = 1.0f;
    loopMode = false;
    isLooping = false;
    repaint();
}

void SoundboardComponent::Pad::fullClear()
{
    clip = nullptr;
    file = juce::File();
    customName = {};
    padGain = 1.0f;
    loopMode = false;
    isLooping = false;
    hotkeyVk = 0;
    hotkeyMods = 0;
    repaint();
}

void SoundboardComponent::Pad::play()
{
    if (clip == nullptr)
        return;

    if (loopMode)
    {
        if (isLooping) { owner.engine.stopClip (clip.get()); isLooping = false; }   // 再按一下：停
        else           { owner.engine.trigger (clip, padGain, true); isLooping = true; }
    }
    else
    {
        owner.engine.trigger (clip, padGain, false);
    }

    flash = 1.0f;          // 視覺回饋：亮一下，再由 timer 淡出
    startTimerHz (60);
    repaint();
}

void SoundboardComponent::Pad::onStopped()
{
    isLooping = false;
    repaint();
}

void SoundboardComponent::Pad::setPadGain (float g)
{
    padGain = juce::jlimit (0.0f, 2.0f, g);
}

void SoundboardComponent::Pad::setLoopMode (bool b)
{
    loopMode = b;
    if (! b) isLooping = false;
    repaint();
}

void SoundboardComponent::Pad::loadFromFile (const juce::File& f)
{
    juce::String err;
    auto loaded = owner.engine.loadClip (f, err);
    if (loaded == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, utf8 ("載入失敗"), err);
        return;
    }
    assign (loaded, f);
    owner.savePads();
}

void SoundboardComponent::Pad::showVolumePopup()
{
    auto slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    slider->setSize (220, 36);
    slider->setRange (0.0, 2.0, 0.01);
    slider->setValue (padGain, juce::dontSendNotification);
    slider->setTextValueSuffix ("x");

    auto* raw = slider.get();
    raw->onValueChange = [this, raw] { setPadGain ((float) raw->getValue()); owner.savePads(); repaint(); };

    juce::CallOutBox::launchAsynchronously (std::move (slider), getScreenBounds(), nullptr);
}

bool SoundboardComponent::Pad::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav")  || f.endsWithIgnoreCase (".mp3") || f.endsWithIgnoreCase (".flac")
         || f.endsWithIgnoreCase (".ogg")  || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff")
         || f.endsWithIgnoreCase (".m4a")  || f.endsWithIgnoreCase (".mp4") || f.endsWithIgnoreCase (".aac")
         || f.endsWithIgnoreCase (".mov")  || f.endsWithIgnoreCase (".wma"))
            return true;
    return false;
}

void SoundboardComponent::Pad::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    if (files.size() > 0)
        loadFromFile (juce::File (files[0]));
}

void SoundboardComponent::Pad::fileDragEnter (const juce::StringArray&, int, int) { dragOver = true;  repaint(); }
void SoundboardComponent::Pad::fileDragExit  (const juce::StringArray&)           { dragOver = false; repaint(); }

void SoundboardComponent::Pad::timerCallback()
{
    flash -= 0.06f;
    if (flash <= 0.0f) { flash = 0.0f; stopTimer(); }
    repaint();
}

void SoundboardComponent::Pad::setHotkey (int vk, int mods)
{
    hotkeyVk   = vk;
    hotkeyMods = mods;
    repaint();
}

void SoundboardComponent::Pad::showMenu()
{
    juce::PopupMenu m;
    m.addItem (1, clip == nullptr ? utf8 ("載入音效…") : utf8 ("替換音效…"));
    m.addItem (5, utf8 ("重新命名…"), clip != nullptr);
    m.addItem (6, utf8 ("音量…"),     clip != nullptr);
    m.addItem (7, utf8 ("循環播放"),  clip != nullptr, loopMode);   // 打勾=已開
    m.addItem (2, utf8 ("清除音效"),  clip != nullptr);
    m.addSeparator();
    m.addItem (3, hotkeyVk == 0 ? utf8 ("設定熱鍵…") : utf8 ("變更熱鍵…"));
    m.addItem (4, utf8 ("清除熱鍵"), hotkeyVk != 0);

    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
    {
        if      (result == 1) loadFile();
        else if (result == 5) renamePad();
        else if (result == 6) showVolumePopup();
        else if (result == 7) { setLoopMode (! loopMode); owner.savePads(); }
        else if (result == 2) { clearSound(); owner.savePads(); }
        else if (result == 3) captureHotkey();
        else if (result == 4) owner.assignHotkey (*this, 0, 0);
    });
}

void SoundboardComponent::Pad::loadFile()
{
    chooser = std::make_unique<juce::FileChooser> (
        utf8 ("選擇音效檔（支援 wav/mp3/flac/ogg；mp4/m4a 需要 ffmpeg）"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff;*.m4a;*.mp4;*.aac;*.mov;*.wma");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        const auto chosen = fc.getResult();
        if (chosen != juce::File())
            loadFromFile (chosen);
    });
}

void SoundboardComponent::Pad::captureHotkey()
{
    awaitingHotkey = true;     // 進入「等待按鍵」模式，由 keyPressed 接手
    grabKeyboardFocus();
    repaint();
}

bool SoundboardComponent::Pad::keyPressed (const juce::KeyPress& kp)
{
    if (! awaitingHotkey)
        return false;          // 平常不攔截按鍵

    if (kp == juce::KeyPress::escapeKey)
    {
        awaitingHotkey = false;
        repaint();
        return true;
    }

    int vk = 0, mods = 0;
    if (GlobalHotkeys::keyPressToVk (kp, vk, mods))
    {
        awaitingHotkey = false;
        owner.assignHotkey (*this, vk, mods);   // 設定 -> 重新註冊 -> 存檔
        repaint();
    }
    return true;
}

void SoundboardComponent::Pad::renamePad()
{
    if (clip == nullptr)
        return;

    const juce::String cur = customName.isNotEmpty() ? customName : clip->name;

    auto* aw = new juce::AlertWindow (utf8 ("重新命名"),
                                      utf8 ("輸入這顆音效要顯示的名稱（留空＝用檔名）："),
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", cur);
    aw->addButton (utf8 ("確定"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton (utf8 ("取消"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        if (r == 1)
        {
            setCustomName (aw->getTextEditorContents ("name").trim());
            owner.savePads();
        }
    }), true);   // deleteWhenDismissed
}

//==============================================================================
//  SoundboardComponent —— 主畫面
//==============================================================================
SoundboardComponent::SoundboardComponent()
{
    // 設定檔：記住每一格綁了哪個音效檔 + 熱鍵
    juce::PropertiesFile::Options opts;
    opts.applicationName     = "VoiceModSoundboard";
    opts.filenameSuffix      = "settings";
    opts.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (opts);

    // 用上次存的裝置設定開啟音訊（記住麥克風、CABLE Input 等選擇）
    engine.initialiseAudio (settings->getValue ("audioDevice"));
    engine.getDeviceManager().addChangeListener (this);

    // 麥克風直通：把真麥克風疊進輸出，這樣 Discord 同時收到「你的聲音 + 音效」
    const bool micOn = settings->getBoolValue ("micPassthrough", true);
    engine.setMicPassthrough (micOn);
    micToggle.setToggleState (micOn, juce::dontSendNotification);
    micToggle.onClick = [this]
    {
        const bool on = micToggle.getToggleState();
        engine.setMicPassthrough (on);
        settings->setValue ("micPassthrough", on);
        settings->saveIfNeeded();
    };
    addAndMakeVisible (micToggle);

    // 監聽：把「只有音效」播到系統預設耳機，自己聽得到音效但聽不到自己人聲
    const bool monOn = settings->getBoolValue ("monitor", false);
    engine.setMonitorEnabled (monOn);
    monitorToggle.setToggleState (monOn, juce::dontSendNotification);
    monitorToggle.onClick = [this]
    {
        const bool on = monitorToggle.getToggleState();
        engine.setMonitorEnabled (on);
        settings->setValue ("monitor", on);
        settings->saveIfNeeded();
    };
    addAndMakeVisible (monitorToggle);

    // 麥克風降噪開關
    const bool nrOn = settings->getBoolValue ("noiseReduction", true);
    engine.setNoiseReduction (nrOn);
    nrToggle.setToggleState (nrOn, juce::dontSendNotification);
    nrToggle.onClick = [this]
    {
        const bool on = nrToggle.getToggleState();
        engine.setNoiseReduction (on);
        settings->setValue ("noiseReduction", on);
        settings->saveIfNeeded();
    };
    addAndMakeVisible (nrToggle);

    // 雜音門檻：低於此音量就視為雜音、自動靜音。對著你的麥克風微調。
    const double thr = settings->getDoubleValue ("gateThreshold", 0.02);
    engine.setGateThreshold ((float) thr);
    gateLabel.setText (utf8 ("雜音門檻"), juce::dontSendNotification);
    gateLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (gateLabel);

    gateSlider.setRange (0.0, 0.10, 0.001);
    gateSlider.setValue (thr, juce::dontSendNotification);
    gateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    gateSlider.onValueChange = [this]
    {
        const double t = gateSlider.getValue();
        engine.setGateThreshold ((float) t);
        settings->setValue ("gateThreshold", t);
        settings->saveIfNeeded();
    };
    addAndMakeVisible (gateSlider);

    // 全域熱鍵被按下 -> 觸發對應索引的格子
    hotkeys.onHotkey = [this] (int id)
    {
        if (id == kStopHotkeyId) { stopAllAction(); return; }
        if (id >= 0 && id < pads.size())
            pads[id]->play();
    };

    backButton.onClick = [this] { if (onBack) onBack(); };
    addAndMakeVisible (backButton);

    title.setText (utf8 ("音效板"), juce::dontSendNotification);
    title.setFont (juce::Font (22.0f, juce::Font::bold));
    addAndMakeVisible (title);

    masterLabel.setText (utf8 ("總音量"), juce::dontSendNotification);
    masterLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (masterLabel);

    masterVolume.setRange (0.0, 1.0, 0.01);
    masterVolume.setSkewFactorFromMidPoint (0.25);   // 對數(感知)曲線，拉起來才跟手
    masterVolume.setValue (engine.getMasterGain(), juce::dontSendNotification);
    masterVolume.setSliderStyle (juce::Slider::LinearHorizontal);
    masterVolume.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);
    masterVolume.onValueChange = [this] { engine.setMasterGain ((float) masterVolume.getValue()); };
    addAndMakeVisible (masterVolume);

    clearAllButton.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
            utf8 ("清除全部"), utf8 ("確定要清除所有格子的音效與熱鍵嗎？"),
            utf8 ("清除"), utf8 ("取消"), nullptr,
            juce::ModalCallbackFunction::create ([this] (int r) { if (r == 1) clearAllPads(); }));
    };
    addAndMakeVisible (clearAllButton);

    deviceButton.onClick = [this] { openDeviceSettings(); };
    addAndMakeVisible (deviceButton);

    // 停止全部（按鈕 + 可綁全域熱鍵）
    stopButton.onTriggered = [this] { stopAllAction(); };
    stopButton.onHotkeyChanged = [this] (int vk, int mods)
    {
        stopHotkeyVk = vk; stopHotkeyMods = mods;
        refreshHotkeys();
        if (settings != nullptr)
        {
            settings->setValue ("stopKey", vk);
            settings->setValue ("stopMod", mods);
            settings->saveIfNeeded();
        }
    };
    stopHotkeyVk   = settings->getIntValue ("stopKey", 0);
    stopHotkeyMods = settings->getIntValue ("stopMod", 0);
    stopButton.setHotkey (stopHotkeyVk, stopHotkeyMods);
    addAndMakeVisible (stopButton);

    // 頁面列控制項
    prevPageBtn.setButtonText (utf8 ("◀"));
    prevPageBtn.onClick = [this] { setCurrentPage (currentPage - 1); };
    addAndMakeVisible (prevPageBtn);

    nextPageBtn.setButtonText (utf8 ("▶"));
    nextPageBtn.onClick = [this] { setCurrentPage (currentPage + 1); };
    addAndMakeVisible (nextPageBtn);

    addPageBtn.setButtonText (utf8 ("＋頁"));
    addPageBtn.onClick = [this] { addPage(); };
    addAndMakeVisible (addPageBtn);

    delPageBtn.setButtonText (utf8 ("刪頁"));
    delPageBtn.onClick = [this] { deletePage(); };
    addAndMakeVisible (delPageBtn);

    pageIndicator.onStep = [this] (int d) { setCurrentPage (currentPage + d); };
    addAndMakeVisible (pageIndicator);

    pageNameLabel.setJustificationType (juce::Justification::centred);
    pageNameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    pageNameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff2a2a2e));
    pageNameLabel.setEditable (false, true, false);   // 雙擊頁名即可改名
    pageNameLabel.onTextChange = [this]
    {
        if (currentPage < pageNames.size())
        {
            pageNames.set (currentPage, pageNameLabel.getText());
            savePads();
        }
    };
    addAndMakeVisible (pageNameLabel);

    // 讀取頁數與頁名
    numPages = juce::jmax (1, settings->getIntValue ("numPages", 1));
    for (int p = 0; p < numPages; ++p)
        pageNames.add (settings->getValue ("pageName" + juce::String (p), defaultPageName (p)));

    // 建立所有頁的格子（先隱藏，setCurrentPage 再顯示目前頁）
    for (int i = 0; i < numPages * padsPerPage; ++i)
    {
        auto* p = new Pad (*this);
        pads.add (p);
        addChildComponent (p);
    }

    loadPads();        // 載回所有頁的音效 + 熱鍵
    refreshHotkeys();  // 註冊全域熱鍵

    currentPage = juce::jlimit (0, numPages - 1, settings->getIntValue ("currentPage", 0));
    setCurrentPage (currentPage);

    setSize (640, 670);
}

SoundboardComponent::~SoundboardComponent()
{
    engine.getDeviceManager().removeChangeListener (this);
    hotkeys.unregisterAll();
}

void SoundboardComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // 音訊裝置設定有變動 -> 存起來，下次開啟自動沿用（免得每次重選 CABLE）
    if (settings != nullptr)
    {
        settings->setValue ("audioDevice", engine.getAudioStateXml());
        settings->saveIfNeeded();
    }
}

void SoundboardComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e22));
}

void SoundboardComponent::resized()
{
    auto r = getLocalBounds().reduced (12);

    // 第一列：返回 + 標題 + 停止 + 清除全部 + 裝置設定
    auto topRow = r.removeFromTop (34);
    backButton.setBounds (topRow.removeFromLeft (84));
    topRow.removeFromLeft (6);
    title.setBounds (topRow.removeFromLeft (90));
    deviceButton.setBounds (topRow.removeFromRight (124));
    topRow.removeFromRight (6);
    clearAllButton.setBounds (topRow.removeFromRight (80));
    topRow.removeFromRight (6);
    stopButton.setBounds (topRow.removeFromRight (52));

    // 第二列：總音量
    auto volRow = r.removeFromTop (30);
    masterLabel.setBounds (volRow.removeFromLeft (55));
    masterVolume.setBounds (volRow.reduced (6, 0));

    // 第三列：麥克風直通 + 監聽 + 降噪
    auto toggleRow = r.removeFromTop (28);
    micToggle.setBounds (toggleRow.removeFromLeft (110));
    monitorToggle.setBounds (toggleRow.removeFromLeft (170));
    nrToggle.setBounds (toggleRow.removeFromLeft (120));

    // 第四列：雜音門檻
    auto gateRow = r.removeFromTop (30);
    gateLabel.setBounds (gateRow.removeFromLeft (70));
    gateSlider.setBounds (gateRow.reduced (6, 0));

    // 頁面控制列：◀ ▶ 名稱 ＋頁 刪頁
    auto pageRow = r.removeFromTop (30);
    prevPageBtn.setBounds (pageRow.removeFromLeft (40));
    nextPageBtn.setBounds (pageRow.removeFromLeft (40));
    pageRow.removeFromLeft (6);
    delPageBtn.setBounds (pageRow.removeFromRight (64));
    addPageBtn.setBounds (pageRow.removeFromRight (64));
    pageNameLabel.setBounds (pageRow.reduced (6, 2));

    // 圓點列：自己一整條，占滿寬度
    pageIndicator.setBounds (r.removeFromTop (22));

    r.removeFromTop (8);

    // 其餘空間：目前頁的按鈕格
    const int gap   = 8;
    const int cellW = (r.getWidth()  - gap * (cols - 1)) / cols;
    const int cellH = (r.getHeight() - gap * (rows - 1)) / rows;

    const int base = currentPage * padsPerPage;
    for (int slot = 0; slot < padsPerPage; ++slot)
    {
        const int idx = base + slot;
        if (idx >= pads.size())
            break;

        const int cx = slot % cols;
        const int cy = slot / cols;
        pads[idx]->setBounds (r.getX() + cx * (cellW + gap),
                              r.getY() + cy * (cellH + gap),
                              cellW, cellH);
    }
}

void SoundboardComponent::openDeviceSettings()
{
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        engine.getDeviceManager(),
        0, 2,        // 輸入：選你的真麥克風
        0, 2,        // 輸出：選 CABLE Input
        false, false,
        true,        // 聲道以「立體聲對」顯示
        false);
    selector->setSize (450, 430);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector.release());
    o.dialogTitle               = utf8 ("音訊裝置設定（輸出選 CABLE Input 就會送進 Discord）");
    o.dialogBackgroundColour    = juce::Colour (0xff2a2a2e);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar         = true;
    o.resizable                 = false;
    o.launchAsync();
}

void SoundboardComponent::stopAllAction()
{
    engine.stopAll();
    for (auto* p : pads)
        p->onStopped();          // 重設各格的迴圈狀態
}

//==============================================================================
//  存檔 / 載入 / 清空 / 熱鍵註冊
//==============================================================================
void SoundboardComponent::savePads()
{
    if (settings == nullptr)
        return;

    // 頁面資訊
    settings->setValue ("numPages", numPages);
    settings->setValue ("currentPage", currentPage);
    for (int p = 0; p < numPages; ++p)
        settings->setValue ("pageName" + juce::String (p), pageNames[p]);
    for (int p = numPages; p < numPages + 8; ++p)       // 清掉多餘頁名
        settings->removeValue ("pageName" + juce::String (p));

    // 每一格
    for (int i = 0; i < pads.size(); ++i)
    {
        const auto key = juce::String (i);
        settings->setValue ("pad"  + key, pads[i]->getFile().getFullPathName());
        settings->setValue ("name" + key, pads[i]->getCustomName());
        settings->setValue ("key"  + key, pads[i]->getHotkeyVk());
        settings->setValue ("mod"  + key, pads[i]->getHotkeyMods());
        settings->setValue ("gain" + key, (double) pads[i]->getPadGain());
        settings->setValue ("loop" + key, pads[i]->getLoopMode());
    }
    // 刪頁後清掉多餘的格子鍵
    for (int i = pads.size(); i < pads.size() + padsPerPage; ++i)
    {
        const auto key = juce::String (i);
        settings->removeValue ("pad"  + key);
        settings->removeValue ("name" + key);
        settings->removeValue ("key"  + key);
        settings->removeValue ("mod"  + key);
        settings->removeValue ("gain" + key);
        settings->removeValue ("loop" + key);
    }

    settings->saveIfNeeded();
}

void SoundboardComponent::loadPads()
{
    if (settings == nullptr)
        return;

    for (int i = 0; i < pads.size(); ++i)
    {
        const auto key = juce::String (i);

        const auto path = settings->getValue ("pad" + key);
        if (path.isNotEmpty())
        {
            const juce::File f (path);
            if (f.existsAsFile())                 // 檔案被移走/刪掉就略過
            {
                juce::String err;
                if (auto c = engine.loadClip (f, err))
                {
                    pads[i]->assign (c, f);
                    const auto nm = settings->getValue ("name" + key);
                    if (nm.isNotEmpty())
                        pads[i]->setCustomName (nm);   // 還原自訂名稱
                }
            }
        }

        const int vk   = settings->getIntValue ("key" + key, 0);
        const int mods = settings->getIntValue ("mod" + key, 0);
        if (vk != 0)
            pads[i]->setHotkey (vk, mods);

        pads[i]->setPadGain ((float) settings->getDoubleValue ("gain" + key, 1.0));
        pads[i]->setLoopMode (settings->getBoolValue ("loop" + key, false));
    }
}

void SoundboardComponent::clearAllPads()
{
    for (auto* p : pads)
        p->fullClear();

    refreshHotkeys();
    savePads();
}

bool SoundboardComponent::refreshHotkeys()
{
    hotkeys.unregisterAll();

    bool allOk = true;
    for (int i = 0; i < pads.size(); ++i)
    {
        const int vk = pads[i]->getHotkeyVk();
        if (vk != 0)
            if (! hotkeys.registerHotkey (i, vk, pads[i]->getHotkeyMods()))
                allOk = false;
    }

    if (stopHotkeyVk != 0)                       // 「停止全部」的熱鍵
        if (! hotkeys.registerHotkey (kStopHotkeyId, stopHotkeyVk, stopHotkeyMods))
            allOk = false;

    return allOk;
}

void SoundboardComponent::assignHotkey (Pad& pad, int vk, int mods)
{
    pad.setHotkey (vk, mods);
    const bool ok = refreshHotkeys();
    savePads();

    if (vk != 0 && ! ok)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
            utf8 ("熱鍵設定"),
            utf8 ("這個按鍵可能已被其他程式或其他格子佔用，換一個再試。"));
}

//==============================================================================
//  多頁
//==============================================================================
juce::String SoundboardComponent::defaultPageName (int page) const
{
    return utf8 ("頁面 ") + juce::String (page + 1);
}

void SoundboardComponent::setCurrentPage (int page)
{
    if (numPages <= 0)
        return;

    currentPage = juce::jlimit (0, numPages - 1, page);

    // 只顯示目前頁的格子
    for (int i = 0; i < pads.size(); ++i)
        pads[i]->setVisible (i / padsPerPage == currentPage);

    if (currentPage < pageNames.size())
        pageNameLabel.setText (pageNames[currentPage], juce::dontSendNotification);
    pageIndicator.setPageInfo (numPages, currentPage);

    prevPageBtn.setEnabled (currentPage > 0);
    nextPageBtn.setEnabled (currentPage < numPages - 1);
    delPageBtn.setEnabled (numPages > 1);

    if (settings != nullptr)
    {
        settings->setValue ("currentPage", currentPage);
        settings->saveIfNeeded();
    }

    resized();   // 重新排版目前頁
}

void SoundboardComponent::addPage()
{
    pageNames.add (defaultPageName (numPages));
    for (int i = 0; i < padsPerPage; ++i)
    {
        auto* p = new Pad (*this);
        pads.add (p);
        addChildComponent (p);
    }
    ++numPages;

    savePads();
    setCurrentPage (numPages - 1);   // 跳到新頁
}

void SoundboardComponent::deletePage()
{
    if (numPages <= 1)               // 至少保留一頁
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
            utf8 ("刪除頁面"), utf8 ("至少要保留一頁。"));
        return;
    }

    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
        utf8 ("刪除頁面"), utf8 ("確定刪除這一頁嗎？（含這頁的音效與熱鍵）"),
        utf8 ("刪除"), utf8 ("取消"), nullptr,
        juce::ModalCallbackFunction::create ([this] (int r) { if (r == 1) doDeleteCurrentPage(); }));
}

void SoundboardComponent::doDeleteCurrentPage()
{
    if (numPages <= 1)
        return;

    const int base = currentPage * padsPerPage;
    pads.removeRange (base, padsPerPage);   // OwnedArray 會刪除這些 Pad（並自動脫離父元件）
    pageNames.remove (currentPage);
    --numPages;

    refreshHotkeys();   // 索引位移了，重新註冊
    savePads();         // 重存（含清掉多餘鍵）
    setCurrentPage (juce::jmin (currentPage, numPages - 1));
}
