#include "GlobalHotkeys.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <map>
 #include <utility>
#endif

//==============================================================================
//  與平台無關的工具（按鍵轉換 / 顯示字串）
//==============================================================================
bool GlobalHotkeys::keyPressToVk (const juce::KeyPress& kp, int& vk, int& mods)
{
    vk = 0;
    const int kc = kp.getKeyCode();

    if (kc >= 'A' && kc <= 'Z')          vk = kc;
    else if (kc >= 'a' && kc <= 'z')     vk = kc - 'a' + 'A';
    else if (kc >= '0' && kc <= '9')     vk = kc;
    else if (kc >= juce::KeyPress::numberPad0 && kc <= juce::KeyPress::numberPad9)
        vk = 0x60 + (kc - juce::KeyPress::numberPad0);                              // VK_NUMPAD0..9
    else
        for (int i = 0; i < 16; ++i)                                               // F1..F16
            if (kc == juce::KeyPress::F1Key + i) { vk = 0x70 + i; break; }          // VK_F1..

   #if JUCE_WINDOWS
    if (vk == 0)
    {
        // 標點/符號等可打印鍵：用目前鍵盤配置把字元轉成正確的 VK
        // （\ ; ' , . / [ ] - = ` 與空白都靠這個）
        juce::juce_wchar c = kp.getTextCharacter();
        if (c <= 0) c = (juce::juce_wchar) kc;
        if (c >= 32 && c < 127)
        {
            const SHORT res = VkKeyScanW ((WCHAR) c);
            if (res != (SHORT) -1)
                vk = res & 0xFF;
        }
    }
   #endif

    if (vk == 0)
        return false;

    const auto m = kp.getModifiers();
    mods = ModNone;
    if (m.isCtrlDown())  mods |= ModCtrl;
    if (m.isAltDown())   mods |= ModAlt;
    if (m.isShiftDown()) mods |= ModShift;
    return true;
}

juce::String GlobalHotkeys::describe (int vk, int mods)
{
    if (vk == 0)
        return {};

    juce::String s;
    if (mods & ModCtrl)  s << "Ctrl+";
    if (mods & ModAlt)   s << "Alt+";
    if (mods & ModShift) s << "Shift+";

    if (vk >= 'A' && vk <= 'Z')        s << (juce::juce_wchar) vk;
    else if (vk >= '0' && vk <= '9')   s << (juce::juce_wchar) vk;
    else if (vk >= 0x60 && vk <= 0x69) s << "Num" << (vk - 0x60);
    else if (vk >= 0x70 && vk <= 0x7F) s << "F" << (vk - 0x70 + 1);
    else if (vk == 0x20)               s << "Space";
    else
    {
       #if JUCE_WINDOWS
        const UINT ch = MapVirtualKeyW ((UINT) vk, MAPVK_VK_TO_CHAR) & 0x7FFF;     // VK -> 顯示字元
        if (ch != 0) s << (juce::juce_wchar) ch;
        else         s << "Key" << vk;
       #else
        s << "Key" << vk;
       #endif
    }
    return s;
}

//==============================================================================
#if JUCE_WINDOWS

struct GlobalHotkeys::Impl
{
    HHOOK          hook = nullptr;
    GlobalHotkeys& owner;
    std::map<int, std::pair<int, int>> idToKey;   // id -> (vk, mods)
    int            lastFiredVk = 0;

    static Impl* instance;                         // 低階掛鉤是全域的，用 static 指回實例

    explicit Impl (GlobalHotkeys& o) : owner (o)
    {
        instance = this;
        // 低階鍵盤掛鉤：Windows 會在「安裝掛鉤的執行緒(訊息執行緒)」上呼叫 hookProc
        hook = SetWindowsHookExW (WH_KEYBOARD_LL, &Impl::hookProc, GetModuleHandleW (nullptr), 0);
    }

    ~Impl()
    {
        if (hook != nullptr) UnhookWindowsHookEx (hook);
        if (instance == this) instance = nullptr;
    }

    static int currentMods()
    {
        int m = 0;
        if (GetAsyncKeyState (VK_CONTROL) & 0x8000) m |= ModCtrl;
        if (GetAsyncKeyState (VK_MENU)    & 0x8000) m |= ModAlt;
        if (GetAsyncKeyState (VK_SHIFT)   & 0x8000) m |= ModShift;
        return m;
    }

    static bool isModifierVk (int vk)
    {
        return vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT
            || vk == VK_LCONTROL || vk == VK_RCONTROL
            || vk == VK_LMENU    || vk == VK_RMENU
            || vk == VK_LSHIFT   || vk == VK_RSHIFT;
    }

    void handleKeyDown (int vk)
    {
        if (isModifierVk (vk) || vk == lastFiredVk)   // 修飾鍵本身不算、且略過自動連發
            return;

        const int mods = currentMods();
        for (auto& kv : idToKey)
            if (kv.second.first == vk && kv.second.second == mods)
            {
                lastFiredVk = vk;
                if (owner.onHotkey) owner.onHotkey (kv.first);
                return;
            }
    }

    void handleKeyUp (int vk)
    {
        if (vk == lastFiredVk) lastFiredVk = 0;       // 放開後才能再次觸發
    }

    static LRESULT CALLBACK hookProc (int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION && instance != nullptr)
        {
            auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*> (lParam);
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                instance->handleKeyDown ((int) kb->vkCode);
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                instance->handleKeyUp ((int) kb->vkCode);
        }
        return CallNextHookEx (nullptr, nCode, wParam, lParam);   // 不攔截，照樣傳給其他程式
    }
};

GlobalHotkeys::Impl* GlobalHotkeys::Impl::instance = nullptr;

GlobalHotkeys::GlobalHotkeys() : impl (std::make_unique<Impl> (*this)) {}
GlobalHotkeys::~GlobalHotkeys() {}

bool GlobalHotkeys::registerHotkey (int id, int vkCode, int modifiers)
{
    if (impl == nullptr || vkCode == 0)
        return false;

    impl->idToKey[id] = { vkCode, modifiers & (ModCtrl | ModAlt | ModShift) };
    return impl->hook != nullptr;
}

void GlobalHotkeys::unregister (int id)
{
    if (impl != nullptr) impl->idToKey.erase (id);
}

void GlobalHotkeys::unregisterAll()
{
    if (impl != nullptr) impl->idToKey.clear();
}

#else  // 非 Windows：先留空殼

struct GlobalHotkeys::Impl { explicit Impl (GlobalHotkeys&) {} };
GlobalHotkeys::GlobalHotkeys() {}
GlobalHotkeys::~GlobalHotkeys() {}
bool GlobalHotkeys::registerHotkey (int, int, int) { return false; }
void GlobalHotkeys::unregister (int) {}
void GlobalHotkeys::unregisterAll() {}

#endif
