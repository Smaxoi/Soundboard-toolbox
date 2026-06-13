#pragma once

#include <JuceHeader.h>
#include "SoundboardEngine.h"
#include "GlobalHotkeys.h"

//==============================================================================
/**
    音效板主畫面：上方工具列（標題、總音量、清除全部、音訊裝置設定），
    下方 4x4 按鈕格。每一格 (Pad) 綁一段音效，可再綁一個「全域熱鍵」。
    每格的「檔案路徑 + 熱鍵」都會存進設定檔，下次開啟自動載回來。

    熱鍵邏輯一律以 pads 集合與其索引驅動，不寫死格數/排列，方便日後改版面。
*/
class SoundboardComponent : public juce::Component,
                            public juce::ChangeListener
{
public:
    SoundboardComponent();
    ~SoundboardComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    //==========================================================================
    /** 單一音效格。空的顯示「+」，點一下載入；載入後點一下播放；右鍵選單可設熱鍵/清除。 */
    class Pad : public juce::Component,
                private juce::Timer
    {
    public:
        explicit Pad (SoundboardComponent& owner);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;   // 設定熱鍵時擷取按鍵

        // 給 SoundboardComponent 呼叫的介面（持久化 / 熱鍵 / 觸發）
        void assign (AudioClip::Ptr clipToUse, const juce::File& sourceFile);
        void clearSound();                          // 只清音效（保留熱鍵）
        void fullClear();                           // 音效 + 熱鍵全清
        void play();                                // 若有音效就觸發

        void setHotkey (int vk, int mods);          // 設定/顯示熱鍵（不負責註冊）
        int  getHotkeyVk()   const { return hotkeyVk; }
        int  getHotkeyMods() const { return hotkeyMods; }
        juce::File getFile() const { return file; }

        void         setCustomName (const juce::String& n);   // 自訂顯示名稱（空=用檔名）
        juce::String getCustomName() const { return customName; }

    private:
        void clicked();
        void showMenu();
        void loadFile();
        void captureHotkey();
        void renamePad();                          // 跳出輸入框改名
        void timerCallback() override;             // 按下高亮的淡出動畫

        SoundboardComponent&               owner;
        AudioClip::Ptr                     clip;
        juce::String                       customName;  // 自訂顯示名稱（空=用檔名）
        juce::File                         file;       // 來源檔（存檔/重載用）
        int                                hotkeyVk   = 0;
        int                                hotkeyMods = 0;
        bool                               awaitingHotkey = false;   // 等待使用者按鍵中
        float                              flash = 0.0f;             // 0=無, 1=剛按下
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pad)
    };

    void openDeviceSettings();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;  // 音訊裝置變動 -> 存設定
    void savePads();                                // 寫入每格的檔案路徑 + 熱鍵
    void loadPads();                                // 開啟時載回音效 + 熱鍵
    void clearAllPads();                            // 全部清空（音效+熱鍵）
    bool refreshHotkeys();                          // 依各格設定重新註冊全域熱鍵；回傳是否全部成功
    void assignHotkey (Pad& pad, int vk, int mods); // 設定某格熱鍵 -> 重新註冊 -> 存檔

    // 多頁
    void         setCurrentPage (int page);         // 切到某頁（顯示該頁格子）
    void         addPage();                          // 新增一頁
    void         deletePage();                       // 刪除目前頁（含確認）
    void         doDeleteCurrentPage();              // 實際刪除
    juce::String defaultPageName (int page) const;   // 預設頁名「頁面 N」

    SoundboardEngine                       engine;
    GlobalHotkeys                          hotkeys;
    std::unique_ptr<juce::PropertiesFile>  settings;

    juce::Label       title;
    juce::Label       masterLabel;
    juce::Slider      masterVolume;
    juce::TextButton  clearAllButton { juce::CharPointer_UTF8 ("清除全部") };
    juce::TextButton  deviceButton   { juce::CharPointer_UTF8 ("音訊裝置設定…") };
    juce::ToggleButton micToggle     { juce::CharPointer_UTF8 ("麥克風直通") };
    juce::ToggleButton monitorToggle { juce::CharPointer_UTF8 ("監聽(自己聽音效)") };
    juce::ToggleButton nrToggle      { juce::CharPointer_UTF8 ("麥克風降噪") };
    juce::Label        gateLabel;
    juce::Slider       gateSlider;
    juce::OwnedArray<Pad> pads;

    // 頁面列
    juce::TextButton  prevPageBtn, nextPageBtn, addPageBtn, delPageBtn;
    juce::Label       pageNameLabel;   // 可雙擊改名
    juce::Label       pageIndicator;   // 「1 / 3」
    juce::StringArray pageNames;
    int               numPages    = 1;
    int               currentPage = 0;

    static constexpr int cols = 4;
    static constexpr int rows = 4;
    static constexpr int padsPerPage = cols * rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundboardComponent)
};
