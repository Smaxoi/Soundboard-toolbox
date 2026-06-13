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
                public juce::FileDragAndDropTarget,
                private juce::Timer
    {
    public:
        explicit Pad (SoundboardComponent& owner);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;   // 設定熱鍵時擷取按鍵

        // 拖放檔案到格子上載入
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int, int) override;
        void fileDragEnter (const juce::StringArray& files, int, int) override;
        void fileDragExit  (const juce::StringArray& files) override;

        // 給 SoundboardComponent 呼叫的介面（持久化 / 熱鍵 / 觸發）
        void assign (AudioClip::Ptr clipToUse, const juce::File& sourceFile);
        void clearSound();                          // 只清音效（保留熱鍵）
        void fullClear();                           // 音效 + 熱鍵全清
        void play();                                // 若有音效就觸發
        void onStopped();                           // 被「停止全部」清掉時重設迴圈狀態

        void setHotkey (int vk, int mods);          // 設定/顯示熱鍵（不負責註冊）
        int  getHotkeyVk()   const { return hotkeyVk; }
        int  getHotkeyMods() const { return hotkeyMods; }
        juce::File getFile() const { return file; }

        void         setCustomName (const juce::String& n);   // 自訂顯示名稱（空=用檔名）
        juce::String getCustomName() const { return customName; }

        void  setPadGain (float g);
        float getPadGain() const { return padGain; }
        void  setLoopMode (bool b);
        bool  getLoopMode() const { return loopMode; }

    private:
        void clicked();
        void showMenu();
        void loadFile();
        void loadFromFile (const juce::File& f);   // 載入（右鍵與拖放共用）
        void showVolumePopup();                    // 音量小滑桿
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
        float                              padGain  = 1.0f;          // 這格的音量倍率
        bool                               loopMode = false;         // 是否設為迴圈
        bool                               isLooping = false;        // 目前正在迴圈中
        bool                               dragOver = false;         // 拖放游標在上方
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pad)
    };

    //==========================================================================
    /** 一個可綁「全域熱鍵」的按鈕（給「停止全部」用）。左鍵=動作；右鍵=設定/清除熱鍵。 */
    class HotkeyButton : public juce::TextButton
    {
    public:
        explicit HotkeyButton (const juce::String& text);

        std::function<void()>        onTriggered;      // 左鍵或熱鍵觸發
        std::function<void(int,int)> onHotkeyChanged;  // 熱鍵變更(0,0=清除)

        void setHotkey (int vk, int mods) { hotkeyVk = vk; hotkeyMods = mods; repaint(); }
        int  getHotkeyVk()   const { return hotkeyVk; }
        int  getHotkeyMods() const { return hotkeyMods; }

        void clicked() override;
        void mouseDown (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;
        void paintButton (juce::Graphics&, bool, bool) override;

    private:
        void showMenu();
        int  hotkeyVk = 0, hotkeyMods = 0;
        bool awaitingHotkey = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HotkeyButton)
    };

    //==========================================================================
    /** 頁碼小圓點：每頁一個點、目前頁是藍色；左右拖動時藍點拉長成膠囊，放開吸附到該頁。 */
    class PageDragger : public juce::Component
    {
    public:
        PageDragger() { setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); }

        std::function<void(int)> onStep;                 // 換頁：參數為相對位移(+下一頁/-上一頁)

        void setPageInfo (int pages, int current)
        {
            numPages    = juce::jmax (1, pages);
            currentPage = juce::jlimit (0, numPages - 1, current);
            dragOffset  = 0.0f;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const float r       = 4.0f;
            const float spacing = 16.0f;
            const float totalW  = (numPages - 1) * spacing;
            const float startX  = getWidth()  * 0.5f - totalW * 0.5f;
            const float cy      = getHeight() * 0.5f;

            // 每頁一個灰色小點
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            for (int i = 0; i < numPages; ++i)
                g.fillEllipse (startX + i * spacing - r, cy - r, 2 * r, 2 * r);

            // 目前頁：藍色（拖動時拉長成膠囊）
            const float a  = (float) currentPage;
            const float b  = juce::jlimit (0.0f, (float) (numPages - 1), currentPage + dragOffset);
            const float x0 = startX + juce::jmin (a, b) * spacing;
            const float x1 = startX + juce::jmax (a, b) * spacing;
            g.setColour (juce::Colour (0xff4a90e2));
            g.fillRoundedRectangle (x0 - r, cy - r, (x1 - x0) + 2 * r, 2 * r, r);
        }

        void mouseDown (const juce::MouseEvent&) override { dragOffset = 0.0f; }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float pxPerPage = 50.0f;                 // 每拖 50px = 一頁
            dragOffset = juce::jlimit ((float) -currentPage,
                                       (float) (numPages - 1 - currentPage),
                                       -e.getDistanceFromDragStartX() / pxPerPage);
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            const int delta = (int) std::lround (dragOffset);
            dragOffset = 0.0f;
            if (delta != 0 && onStep) onStep (delta);      // 換頁（setPageInfo 會重畫）
            else repaint();                                // 沒過半 -> 彈回
        }

    private:
        int   numPages    = 1;
        int   currentPage = 0;
        float dragOffset  = 0.0f;
    };

    void openDeviceSettings();
    void stopAllAction();                            // 停掉全部 + 重設各格迴圈狀態
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
    HotkeyButton      stopButton     { juce::CharPointer_UTF8 ("停止") };
    int stopHotkeyVk = 0, stopHotkeyMods = 0;
    static constexpr int kStopHotkeyId = 100000;   // 停止鈕專用的熱鍵 id（避開格子索引）
    juce::ToggleButton micToggle     { juce::CharPointer_UTF8 ("麥克風直通") };
    juce::ToggleButton monitorToggle { juce::CharPointer_UTF8 ("監聽(自己聽音效)") };
    juce::ToggleButton nrToggle      { juce::CharPointer_UTF8 ("麥克風降噪") };
    juce::Label        gateLabel;
    juce::Slider       gateSlider;
    juce::OwnedArray<Pad> pads;

    // 頁面列
    juce::TextButton  prevPageBtn, nextPageBtn, addPageBtn, delPageBtn;
    juce::Label       pageNameLabel;   // 可雙擊改名
    PageDragger       pageIndicator;   // 「1 / 3」可左右拖動換頁
    juce::StringArray pageNames;
    int               numPages    = 1;
    int               currentPage = 0;

    static constexpr int cols = 4;
    static constexpr int rows = 4;
    static constexpr int padsPerPage = cols * rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundboardComponent)
};
