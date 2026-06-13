#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

//==============================================================================
/**
    系統全域熱鍵：即使視窗沒有焦點（在遊戲 / Discord 裡）也能觸發。

    這個類別「完全不認識」音效板或格子——它只負責「id ↔ 按鍵」的對應與觸發回呼，
    呼叫端自己決定 id 代表什麼。所以之後版面格式怎麼變動都不影響這個模組。
*/
class GlobalHotkeys
{
public:
    GlobalHotkeys();
    ~GlobalHotkeys();

    // 與平台無關的修飾鍵旗標（對外介面，內部再轉成 Win32 的 MOD_*）
    enum Modifier { ModNone = 0, ModAlt = 1, ModCtrl = 2, ModShift = 4 };

    /** 註冊一個全域熱鍵。id 由呼叫端自訂（音效板用格子索引）。
        vkCode = Windows Virtual-Key；modifiers = 上面 Modifier 的組合。
        回傳是否成功（失敗多半是該組合已被別的程式佔用）。 */
    bool registerHotkey (int id, int vkCode, int modifiers);
    void unregister (int id);
    void unregisterAll();

    /** 熱鍵被按下時觸發（在訊息執行緒上）。參數是註冊時給的 id。 */
    std::function<void (int id)> onHotkey;

    /** 工具：把 JUCE 的 KeyPress 轉成 (vkCode, modifiers)。不支援的鍵回傳 false。 */
    static bool keyPressToVk (const juce::KeyPress& kp, int& vkCodeOut, int& modifiersOut);

    /** 工具：把 (vkCode, modifiers) 轉成可顯示字串，例如 "Ctrl+Alt+F1"。 */
    static juce::String describe (int vkCode, int modifiers);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE (GlobalHotkeys)
};
