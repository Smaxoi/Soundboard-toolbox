#pragma once

#include <juce_dsp/juce_dsp.h>

//==============================================================================
/**
    VoiceEngine —— 純 DSP 引擎（整個專案的核心）。

    它「不知道」聲音從哪來：可能是即時麥克風，也可能是讀檔。
    它只負責一件事：拿到一塊聲音 (AudioBuffer) -> 套用音效 -> 改寫回去。

    因為這樣解耦，之後即時模式和離線模式可以共用同一顆引擎。
    現在的離線版只是用一個 for 迴圈把整個 wav 切成一塊一塊餵給它。
*/
class VoiceEngine
{
public:
    VoiceEngine() = default;

    //==========================================================================
    // 所有可調參數集中放這裡。
    // （之後要接 UI 即時調整時，這些會改成 std::atomic 以便跨執行緒安全傳遞。）
    struct Parameters
    {
        // --- 失真 Distortion：把波形壓扁，產生粗糙/破音感 ---
        bool  distortionOn    = false;
        float distortionDrive = 8.0f;     // 越大越破

        // --- 環形調變 Ring Modulation：機器人 / 金屬聲 ---
        bool  ringModOn     = false;
        float ringModFreqHz = 50.0f;      // 載波頻率，30~120Hz 都可玩
        float ringModMix    = 1.0f;       // 0 = 原音, 1 = 全濕

        // --- 回音 / 延遲 Delay / Echo ---
        bool  delayOn       = false;
        float delayTimeMs   = 250.0f;     // 每次回音的間隔
        float delayFeedback = 0.45f;      // 回授量（<1 回音才會逐漸消失）
        float delayMix      = 0.5f;       // 回音音量

        // --- 殘響 Reverb：空間感 ---
        bool  reverbOn       = true;
        float reverbRoomSize = 0.6f;      // 0~1，空間大小
        float reverbWet      = 0.33f;     // 濕度
        float reverbDamping  = 0.5f;      // 高頻吸收
    };

    Parameters params;

    //==========================================================================
    /** 開始處理前呼叫一次，讓內部模組依取樣率/塊大小/聲道數配置好記憶體。 */
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);

    /** 核心：就地處理一塊聲音（直接改寫 buffer 內容）。 */
    void process (juce::AudioBuffer<float>& buffer);

    /** 清空內部狀態（延遲線、殘響殘留）。 */
    void reset();

private:
    double currentSampleRate = 44100.0;

    // 延遲線：最多存 192000 個 sample（@48kHz 約 4 秒），夠做回音
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 192000 };

    // 環形調變用的正弦載波振盪器
    juce::dsp::Oscillator<float> ringOsc;

    // JUCE 內建殘響（Freeverb 演算法）
    juce::dsp::Reverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceEngine)
};
