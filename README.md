# VoiceMod 音效板 (Soundboard)

用 **C++ / JUCE** 寫的音效板：一格一格按鈕，每顆綁一段音效，按下去就播放，
並可把輸出送進「虛擬麥克風」，讓 Discord／遊戲的對方也聽得到。

- 原生支援：`wav` / `mp3` / `flac` / `ogg` / `aiff`
- **MP4 / m4a / aac / mov**：會自動呼叫 `ffmpeg` 轉檔後載入（見下方說明）
- 同時可疊放多顆音效（內建混音）

> 註：先前的「即時變聲效果」引擎 `Source/VoiceEngine.*` 暫時停在這、還沒納入這個 app，
> 之後做「對自己的聲音套效果」時會用到。

## 一、需要先裝的東西

1. **Visual Studio 2022**，安裝時勾選「**使用 C++ 的桌面開發**」工作負載
   （這個工作負載已內含 CMake，不必另外裝）。
2. **（選用）ffmpeg** — 只有要載入 **MP4** 這類檔案才需要。
   到 https://www.gyan.dev/ffmpeg/builds/ 下載，把 `ffmpeg.exe` 放到程式旁邊
   （`build\Soundboard_artefacts\Release\` 裡），或加進系統 PATH。

## 二、編譯

在專案資料夾打開「**x64 Native Tools Command Prompt for VS 2022**」或一般 PowerShell：

```powershell
# 1) 產生專案（第一次會從 GitHub 下載 JUCE，需幾分鐘）
cmake -B build

# 2) 編譯
cmake --build build --config Release
```

執行檔在：

```
build\Soundboard_artefacts\Release\Soundboard.exe
```

## 三、使用

1. 開啟程式，會看到 4×4 的格子。
2. 點任一個「**+**」格 → 選一個音效檔 → 該格就綁好了。
3. 之後**點該格就會播放**；要同時疊放就連點多格。
4. **右鍵**（或觸控長按）格子 → 可「替換音效／清除」。
5. 上方「**總音量**」調整整體音量。

## 四、送進 Discord

1. 安裝 **VB-Audio Virtual Cable**（免費）：https://vb-audio.com/Cable/ ，裝好後重開機。
2. 程式裡點「**音訊裝置設定…**」→ 把**輸出裝置選成 `CABLE Input`**。
3. 在 **Discord** → 設定 → 語音與視訊 → 輸入裝置選 **`CABLE Output`**。
4. 之後你按音效板，Discord 對方就聽得到了。

> 想自己也聽到：Windows 音效設定 → 錄製 → `CABLE Output` → 內容 → 「**接聽**」勾「接聽此裝置」，
> 或測試階段先把輸出維持在你的喇叭/耳機。

## 五、程式結構

```
CMakeLists.txt              建置設定（自動抓 JUCE）
Source/
  Main.cpp                  程式進入點：建立視窗
  SoundboardComponent.*     UI：按鈕格、音量、裝置設定
  SoundboardEngine.*        音訊引擎：解碼匯入、混音、輸出
  VoiceEngine.*             （停用中）之後的即時變聲效果引擎
```

## 六、下一步

- 全域熱鍵：在遊戲/Discord 視窗裡也能觸發
- 雙輸出：同時送 Discord + 自己監聽，不必靠 Windows「接聽」
- 把停用中的 `VoiceEngine` 接上麥克風，做即時變聲
- 音效拖放匯入、預設(preset) 存讀
