# 環境建置與編譯指南

這份說明如何從零把 **VoiceMod 工具箱**（C++ / JUCE，Windows）建置起來並執行。

---

## 一、需要先裝的軟體

| 軟體 | 用途 | 說明 |
|------|------|------|
| **Visual Studio 2022**（Community 免費即可） | C++ 編譯器 + CMake | 安裝時**務必勾選「使用 C++ 的桌面開發 (Desktop development with C++)」**工作負載。它已內含 **MSVC 編譯器、CMake、Ninja**，不必另外裝 CMake。 |
| **Git** | 下載 JUCE | 編譯時會用 CMake 的 FetchContent 自動從 GitHub 抓 JUCE，需要 git。 |
| （選用）**ffmpeg** | 載入 MP4/m4a/aac | 只有要把這類影音檔當音效時才需要。見第五節。 |
| （選用）**VB-Audio Virtual Cable** | 送進 Discord/遊戲 | 虛擬麥克風。見第四節。 |

> 預期：Visual Studio 安裝約數 GB；第一次編譯會下載 JUCE 並編譯，約 **10 分鐘**（之後增量編譯很快）。

---

## 二、取得原始碼

把整個專案資料夾放到本機，例如 `D:\Sam\Desktop\voicemod`。
若是用 git：

```powershell
git clone <你的 repo 網址> voicemod
cd voicemod
```

> `build/` 與 `sounds/*.wav` 已被 `.gitignore` 排除，不會進版本控制。

---

## 三、編譯

### 方法 A：用 Visual Studio 開資料夾（最簡單）

1. 開啟 **Visual Studio 2022** → **檔案 → 開啟 → 資料夾…** → 選 `voicemod` 資料夾。
2. VS 會自動偵測 `CMakeLists.txt` 並開始設定（第一次會下載 JUCE，請等它跑完，下方狀態列會顯示進度）。
3. 上方啟動項目選 **`Soundboard.exe`**，組態選 **Release**。
4. 按 **▶（執行）** 或 `Ctrl+F5`。

### 方法 B：命令列

從開始功能表打開 **「Developer PowerShell for VS 2022」**（這個殼裡才有 `cmake`／編譯器的 PATH），然後：

```powershell
cd D:\Sam\Desktop\voicemod

# 1) 設定（第一次會從 GitHub 下載 JUCE，需幾分鐘）
cmake -B build -G "Visual Studio 17 2022" -A x64

# 2) 編譯（Release）
cmake --build build --config Release
```

### 產物位置

```
build\Soundboard_artefacts\Release\Soundboard.exe
```

> 說明：CMake 的目標名稱仍叫 `Soundboard`，所以執行檔是 `Soundboard.exe`，但它其實是整個工具箱（含主選單、音效板、文字轉語音）。

---

## 四、執行與「送進 Discord」的音訊設定

直接雙擊 `Soundboard.exe` 會看到**主選單**，點功能會各自開獨立視窗（可同時開多個）。

要讓音效／語音進到 Discord，需要 **VB-Audio Virtual Cable**（免費，[vb-audio.com/Cable](https://vb-audio.com/Cable/)，裝完重開機）。完整路由：

| 在哪裡 | 設成 |
|--------|------|
| **音效板 →「音訊裝置設定…」** | 輸入＝你的真麥克風；輸出＝`CABLE Input` |
| **音效板的開關** | 「麥克風直通」開、「監聽(自己聽音效)」開 |
| **Discord → 語音與視訊** | 輸入裝置＝`CABLE Output`；輸出裝置＝你的耳機 |
| **Discord 同頁** | 關掉「自動增益控制 (AGC)」與「噪音抑制 (Krisp)」 |
| **Windows 舊版聲音控制台（`mmsys.cpl`）** | 把 `CABLE Output` 的「Listen 接聽」**關掉**（由 app 的監聽取代）|

> 戴耳機，避免喇叭回授。詳細原理見對話記錄或之後補的使用手冊。

---

## 五、選用：MP4 / ffmpeg

音效板原生可讀 `wav / mp3 / flac / ogg / aiff`。要載入 **MP4 / m4a / aac / mov** 時，程式會自動呼叫 `ffmpeg` 轉檔：

1. 到 https://www.gyan.dev/ffmpeg/builds/ 下載「ffmpeg-release-essentials.zip」。
2. 解壓，把裡面 `bin\ffmpeg.exe` 複製到**執行檔旁邊**（`build\Soundboard_artefacts\Release\`），或加進系統 PATH。

---

## 六、疑難排解

| 症狀 | 解法 |
|------|------|
| 第一次設定卡很久 | 正在從 GitHub 下載 JUCE，需網路＋耐心（數分鐘）。 |
| 命令列說找不到 `cmake` | 用「Developer PowerShell for VS 2022」，或改用方法 A（VS 開資料夾）。 |
| 編譯出現 C4819／中文亂碼 | `CMakeLists.txt` 已加 `/utf-8`，正常不會發生；若自行改動請保留那行。 |
| 防毒跳出提示 | 全域熱鍵用「鍵盤掛鉤」偵測按鍵，部分防毒會提示，允許即可（程式不記錄輸入）。 |
| 載入 MP4 失敗 | 需要 `ffmpeg.exe`（見第五節）。 |
| Discord 聽不到／斷斷續續 | 確認 Discord 輸入＝`CABLE Output`，並關掉 AGC / Krisp。 |
| 文字轉語音念不出中文 | 系統需安裝中文語音：Windows 設定 → 時間與語言 → 語音 → 新增語音。 |

---

## 七、專案結構

```
CMakeLists.txt            建置設定（FetchContent 自動抓 JUCE、/utf-8、連結 JUCE 模組）
Source/
  Main.cpp               app 進入點：建立主視窗
  MainComponent.*        主選單外殼 + 把各功能開成獨立視窗
  SoundboardComponent.*  音效板 UI（多頁、熱鍵、音量、循環…）
  SoundboardEngine.*     共用音訊引擎：裝置管理、混音、麥克風直通/降噪、雙輸出
  GlobalHotkeys.*        全域熱鍵（Windows 低階鍵盤掛鉤）
  TTSComponent.*         文字轉語音（System.Speech 合成 → 共用引擎播放）
  VoiceEngine.*          （停用中）離線效果引擎，保留待之後做變聲器
.gitignore
```

---

## 八、相依與授權

- **JUCE**：編譯時自動下載（CMakeLists 內 pin 在某個版本 tag）。JUCE 採 GPL／商用雙授權，個人/開源用途走 GPL。
- **Windows API**：全域熱鍵用 Win32（`SetWindowsHookEx`）、文字轉語音透過 PowerShell 呼叫 .NET `System.Speech`，皆為系統內建。
