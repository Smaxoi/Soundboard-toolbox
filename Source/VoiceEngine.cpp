#include "VoiceEngine.h"

//==============================================================================
void VoiceEngine::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) maximumBlockSize;
    spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);

    delayLine.prepare (spec);
    delayLine.reset();

    ringOsc.initialise ([] (float x) { return std::sin (x); }); // 正弦載波
    ringOsc.prepare (spec);

    reverb.prepare (spec);
    reverb.reset();
}

void VoiceEngine::reset()
{
    delayLine.reset();
    reverb.reset();
}

//==============================================================================
void VoiceEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // ---- 1. 失真：out = tanh(in * drive)。tanh 把過大的值壓向 ±1，形成柔性破音 ----
    if (params.distortionOn)
    {
        const float drive = juce::jmax (1.0f, params.distortionDrive);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = std::tanh (data[i] * drive);
        }
    }

    // ---- 2. 環形調變：out = in * 正弦載波。聲音乘上一個固定頻率 -> 機器人/金屬聲 ----
    if (params.ringModOn)
    {
        ringOsc.setFrequency (params.ringModFreqHz);
        const float mix = juce::jlimit (0.0f, 1.0f, params.ringModMix);

        // 外層跑 sample、內層跑聲道：讓載波每個 sample 前進一次，所有聲道共用同一相位
        for (int i = 0; i < numSamples; ++i)
        {
            const float carrier = ringOsc.processSample (0.0f); // -1..1 的正弦
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer (ch);
                const float in = data[i];
                data[i] = in * (1.0f - mix) + (in * carrier) * mix;
            }
        }
    }

    // ---- 3. 回音 / 延遲：DelayLine 配合回授，做出一次次衰減的回音 ----
    if (params.delayOn)
    {
        const float delaySamples = (float) (params.delayTimeMs * 0.001 * currentSampleRate);
        delayLine.setDelay (delaySamples);
        const float fb  = juce::jlimit (0.0f, 0.95f, params.delayFeedback);
        const float mix = juce::jlimit (0.0f, 1.0f, params.delayMix);

        // DelayLine 的讀/寫位置是「每個聲道各自獨立」，所以可以一個聲道一個聲道處理
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float in      = data[i];
                const float delayed = delayLine.popSample (ch);   // 讀出延遲訊號
                delayLine.pushSample (ch, in + delayed * fb);     // 存入（含回授 -> 重複回音）
                data[i] = in + delayed * mix;                     // 原音 + 回音
            }
        }
    }

    // ---- 4. 殘響：放在效果鏈最後。用 JUCE 內建 Freeverb ----
    if (params.reverbOn)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 1.0f, params.reverbRoomSize);
        rp.wetLevel = juce::jlimit (0.0f, 1.0f, params.reverbWet);
        rp.dryLevel = 1.0f;     // 保留完整原音，殘響疊加上去
        rp.damping  = juce::jlimit (0.0f, 1.0f, params.reverbDamping);
        rp.width    = 1.0f;
        reverb.setParameters (rp);

        // juce::dsp 模組吃的是 AudioBlock + ProcessContext，這裡把 buffer 包一層給它
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }
}
