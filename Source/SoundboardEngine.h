#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

//==============================================================================
/** 一段「已解碼」的音效（PCM 放在記憶體）。用 ReferenceCountedObject 確保播放中的
    voice 仍持有它，即使使用者把該格換掉也不會崩潰。 */
struct AudioClip : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<AudioClip>;

    juce::String              name;
    juce::AudioBuffer<float>  buffer;
    double                    sourceSampleRate = 44100.0;
};

//==============================================================================
/**
    音效板的音訊引擎。

    主裝置：輸入=真麥克風、輸出=CABLE Input。輸出 = 麥克風直通 + 音效混音。
    監聽裝置（可選，第二個輸出=系統預設耳機）：只播「音效」那一份，
    讓你自己聽得到按的音效、但不會聽到自己的人聲。

    兩個裝置在不同音訊執行緒上跑，靠一個無鎖環形緩衝(StereoFifo)傳遞「只有音效」的訊號。
*/
class SoundboardEngine : public juce::AudioIODeviceCallback
{
public:
    SoundboardEngine();
    ~SoundboardEngine() override;

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    AudioClip::Ptr loadClip (const juce::File& file, juce::String& errorMessage);
    void trigger (AudioClip::Ptr clip, float gain = 1.0f, bool loop = false);
    void stopAll();                          // 停掉所有正在播的音效
    void stopClip (AudioClip* clip);         // 停掉某一段（給迴圈再按一下停用）

    void  setMasterGain (float g) { masterGain.store (juce::jlimit (0.0f, 1.0f, g)); }
    float getMasterGain() const   { return masterGain.load(); }

    // 麥克風直通：把真麥克風疊進主輸出（送 Discord 時要開）
    void setMicPassthrough (bool on) { micPassthrough.store (on); }
    bool getMicPassthrough() const   { return micPassthrough.load(); }

    // 監聽：把「只有音效」播到系統預設耳機，自己聽得到音效但不會聽到自己人聲
    void setMonitorEnabled (bool on);
    bool getMonitorEnabled() const { return monitorEnabled.load(); }

    // 麥克風降噪：高通濾波(去低頻嗡聲) + 雜音門檻(沒講話時靜音)。只作用在麥克風，不影響音效。
    void  setNoiseReduction (bool on) { noiseReduction.store (on); }
    bool  getNoiseReduction() const   { return noiseReduction.load(); }
    void  setGateThreshold (float t)  { gateThreshold.store (juce::jlimit (0.0f, 0.2f, t)); }
    float getGateThreshold() const    { return gateThreshold.load(); }

    // 用儲存的狀態(XML)開啟主音訊裝置；空字串 = 系統預設
    void         initialiseAudio (const juce::String& savedState);
    juce::String getAudioStateXml() const;

    // juce::AudioIODeviceCallback（主裝置）
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    //==========================================================================
    struct Voice   { AudioClip::Ptr clip; double position = 0.0; float gain = 1.0f; bool loop = false; bool active = false; };
    struct Trigger { AudioClip::Ptr clip; float gain = 1.0f; bool loop = false; };

    // 只放「音效」的立體聲環形緩衝（主執行緒寫、監聽執行緒讀；無鎖 SPSC）
    struct StereoFifo
    {
        explicit StereoFifo (int numFrames);
        void write (const float* L, const float* R, int n);
        void read  (float* L, float* R, int n);
        void reset();

        juce::AbstractFifo       fifo;
        juce::AudioBuffer<float> buffer;
    };

    // 監聽裝置的回呼：從 fifo 讀「音效」播到耳機
    struct MonitorCallback : public juce::AudioIODeviceCallback
    {
        MonitorCallback (StereoFifo& f, std::atomic<bool>& e) : fifo (f), enabled (e) {}
        void audioDeviceIOCallbackWithContext (const float* const*, int,
                                               float* const* outputChannelData, int numOutputChannels,
                                               int numSamples,
                                               const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
        void audioDeviceStopped() override {}

        StereoFifo&        fifo;
        std::atomic<bool>& enabled;
        std::vector<float> tmpR;
    };

    void         startVoice (AudioClip::Ptr clip, float gain, bool loop);
    juce::String resolveFFmpeg() const;
    juce::File   transcodeWithFFmpeg (const juce::File& src, juce::String& errorMessage);

    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;

    double deviceSampleRate = 48000.0;

    static constexpr int maxVoices = 24;
    std::array<Voice, maxVoices> voices;

    static constexpr int triggerCapacity = 64;
    juce::AbstractFifo                   triggerFifo { triggerCapacity };
    std::array<Trigger, triggerCapacity> triggerSlots;

    std::atomic<float> masterGain     { 0.8f };
    std::atomic<bool>  micPassthrough { true };
    std::atomic<bool>        stopAllFlag { false };   // 要求清掉所有 voice
    std::atomic<AudioClip*>  stopClipPtr { nullptr };  // 要求停掉某段

    // 麥克風降噪狀態（只在音訊執行緒讀寫 gate 狀態）
    std::atomic<bool>            noiseReduction { true };
    std::atomic<float>           gateThreshold  { 0.02f };
    juce::dsp::IIR::Filter<float> micHP[2];
    float gateEnv = 0.0f, gateGain = 0.0f;
    float envAttack = 0.0f, envRelease = 0.0f, gateOpen = 0.0f, gateClose = 0.0f;
    juce::AudioBuffer<float>     micScratch;

    // 監聽（第二輸出裝置）
    std::atomic<bool>        monitorEnabled { false };
    StereoFifo               monitorFifo { 48000 };                  // 約 1 秒緩衝
    MonitorCallback          monitorCallback { monitorFifo, monitorEnabled };
    juce::AudioDeviceManager monitorDeviceManager;
    juce::AudioBuffer<float> sfxScratch;                            // 主回呼用：算「只有音效」那份

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundboardEngine)
};
