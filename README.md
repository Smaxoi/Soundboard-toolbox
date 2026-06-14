# VoiceMod 工具箱

一個用 **C++ / JUCE** 寫的 Windows 語音工具箱。開啟是一個**主選單**，每個功能會開成**獨立視窗**，可同時使用：

- **音效板 (Soundboard)** — 多頁按鈕格，每格綁一段音效，可設音量、循環、**全域熱鍵**（遊戲中也能觸發）。原生讀 `wav/mp3/flac/ogg/aiff`；`mp4/m4a/aac` 會自動用 `ffmpeg` 轉檔。支援拖放載入、重新命名。
- **文字轉語音 (TTS)** — 選系統語音與語速，輸入文字即用系統語音念出來。
- **變聲器** — 規劃中（之後走 RVC 角色變聲）。

所有功能共用同一個**音訊引擎**：可把輸出送進虛擬麥克風（VB-Audio Cable）→ **Discord / 遊戲**對方就聽得到；麥克風會直通並可**降噪**；還有**雙輸出**讓你自己也聽得到音效但不會聽到自己。

## 快速開始

**我只想使用** 👉 到 **[Releases 下載安裝檔](https://github.com/Smaxoi/Soundboard-toolbox/releases/latest)**，雙擊安裝即可（免系統管理員、不需額外執行階段）。
安裝步驟與進階設定見 **[INSTALL.md（安裝教學）](INSTALL.md)**。

**我要自己編譯（開發者）** 👉 看 **[BUILD.md（環境建置）](BUILD.md)**

```powershell
# 在「Developer PowerShell for VS 2022」裡：
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
# 產物：build\Soundboard_artefacts\Release\Soundboard.exe
```

**製作安裝檔**：先編譯，再用 Inno Setup 編譯 `installer/voicemod.iss` → `dist/VoiceMod-Toolbox-Setup.exe`。

## 程式結構

```
Source/
  Main.cpp               進入點
  MainComponent.*        主選單 + 各功能獨立視窗
  SoundboardComponent.*  音效板 UI
  SoundboardEngine.*     共用音訊引擎（裝置/混音/麥克風直通/降噪/雙輸出）
  GlobalHotkeys.*        全域熱鍵（低階鍵盤掛鉤）
  TTSComponent.*         文字轉語音
  VoiceEngine.*          （停用中）離線效果引擎，待之後做變聲器
```

## 開發狀態

JUCE/C++、Windows。本機 git repo（尚無遠端）。詳細功能與後續規劃見對話與 `BUILD.md`。
