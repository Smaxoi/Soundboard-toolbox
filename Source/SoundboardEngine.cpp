#include "SoundboardEngine.h"

namespace
{
    // JUCE 的 String(const char*) 會把位元組當 Latin-1；中文要用 UTF-8 解才正確。
    inline juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
}

//==============================================================================
//  StereoFifo —— 只放「音效」的無鎖立體聲環形緩衝
//==============================================================================
SoundboardEngine::StereoFifo::StereoFifo (int numFrames) : fifo (numFrames)
{
    buffer.setSize (2, numFrames);
    buffer.clear();
}

void SoundboardEngine::StereoFifo::write (const float* L, const float* R, int n)
{
    int s1, sz1, s2, sz2;
    fifo.prepareToWrite (n, s1, sz1, s2, sz2);
    if (sz1 > 0) { buffer.copyFrom (0, s1, L,        sz1); buffer.copyFrom (1, s1, R,        sz1); }
    if (sz2 > 0) { buffer.copyFrom (0, s2, L + sz1,  sz2); buffer.copyFrom (1, s2, R + sz1,  sz2); }
    fifo.finishedWrite (sz1 + sz2);   // 空間不足時自動只寫得下的部分（溢位就丟棄）
}

void SoundboardEngine::StereoFifo::read (float* L, float* R, int n)
{
    int s1, sz1, s2, sz2;
    fifo.prepareToRead (n, s1, sz1, s2, sz2);
    if (sz1 > 0) { juce::FloatVectorOperations::copy (L,       buffer.getReadPointer (0, s1), sz1);
                   juce::FloatVectorOperations::copy (R,       buffer.getReadPointer (1, s1), sz1); }
    if (sz2 > 0) { juce::FloatVectorOperations::copy (L + sz1, buffer.getReadPointer (0, s2), sz2);
                   juce::FloatVectorOperations::copy (R + sz1, buffer.getReadPointer (1, s2), sz2); }

    const int got = sz1 + sz2;
    for (int i = got; i < n; ++i) { L[i] = 0.0f; R[i] = 0.0f; }   // 不夠就補靜音（underrun）
    fifo.finishedRead (got);
}

void SoundboardEngine::StereoFifo::reset() { fifo.reset(); }

//==============================================================================
//  MonitorCallback —— 監聽裝置：從 fifo 讀「音效」播到耳機
//==============================================================================
void SoundboardEngine::MonitorCallback::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    tmpR.assign ((size_t) juce::jmax (8192, device->getCurrentBufferSizeSamples()), 0.0f);
}

void SoundboardEngine::MonitorCallback::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                                          float* const* out, int numOut,
                                                                          int numSamples,
                                                                          const juce::AudioIODeviceCallbackContext&)
{
    if (numOut <= 0)
        return;

    if (! enabled.load())
    {
        for (int ch = 0; ch < numOut; ++ch)
            if (out[ch] != nullptr)
                juce::FloatVectorOperations::clear (out[ch], numSamples);
        return;
    }

    float* L = out[0];
    if (numOut > 1 && out[1] != nullptr)
    {
        fifo.read (L, out[1], numSamples);
        for (int ch = 2; ch < numOut; ++ch)          // 多餘聲道清空
            if (out[ch] != nullptr)
                juce::FloatVectorOperations::clear (out[ch], numSamples);
    }
    else
    {
        // 單聲道輸出：右聲道讀到暫存丟掉
        if ((int) tmpR.size() < numSamples) tmpR.assign ((size_t) numSamples, 0.0f);
        fifo.read (L, tmpR.data(), numSamples);
    }
}

//==============================================================================
//  SoundboardEngine
//==============================================================================
SoundboardEngine::SoundboardEngine()
{
    formatManager.registerBasicFormats();
    deviceManager.addAudioCallback (this);
    // 實際開啟主裝置由 initialiseAudio() 處理（才能套用儲存的設定）
}

SoundboardEngine::~SoundboardEngine()
{
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();

    monitorDeviceManager.removeAudioCallback (&monitorCallback);
    monitorDeviceManager.closeAudioDevice();
}

void SoundboardEngine::initialiseAudio (const juce::String& savedState)
{
    std::unique_ptr<juce::XmlElement> xml;
    if (savedState.isNotEmpty())
        xml = juce::parseXML (savedState);

    deviceManager.initialise (2, 2, xml.get(), true);   // 2 路輸入(麥克風) + 2 路輸出(CABLE)
}

juce::String SoundboardEngine::getAudioStateXml() const
{
    if (auto xml = deviceManager.createStateXml())
        return xml->toString();

    return {};
}

void SoundboardEngine::setMonitorEnabled (bool on)
{
    if (on == monitorEnabled.load())
        return;

    if (on)
    {
        monitorFifo.reset();
        monitorEnabled.store (true);
        monitorDeviceManager.initialiseWithDefaultDevices (0, 2);   // 預設輸出(耳機)，不要輸入
        monitorDeviceManager.addAudioCallback (&monitorCallback);
    }
    else
    {
        monitorEnabled.store (false);
        monitorDeviceManager.removeAudioCallback (&monitorCallback);
        monitorDeviceManager.closeAudioDevice();
    }
}

//==============================================================================
void SoundboardEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    deviceSampleRate = device->getCurrentSampleRate();
    const int blockSize = juce::jmax (8192, device->getCurrentBufferSizeSamples());

    sfxScratch.setSize (2, blockSize, false, false, true);
    micScratch.setSize (2, blockSize, false, false, true);

    // 麥克風高通濾波 ~85Hz（去除低頻嗡嗡聲/電流聲）
    juce::dsp::ProcessSpec spec { deviceSampleRate, (juce::uint32) blockSize, 1 };
    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (deviceSampleRate, 85.0f);
    for (auto& f : micHP) { f.prepare (spec); f.coefficients = hp; f.reset(); }

    // 雜音門檻的平滑係數（時間 -> 衰減係數）
    auto coeff = [this] (float ms) { return std::exp (-1.0f / (juce::jmax (0.001f, ms * 0.001f) * (float) deviceSampleRate)); };
    envAttack  = coeff (1.0f);
    envRelease = coeff (80.0f);
    gateOpen   = coeff (3.0f);
    gateClose  = coeff (120.0f);
    gateEnv = 0.0f;
    gateGain = 0.0f;

    for (auto& v : voices) { v.active = false; v.clip = nullptr; }
}

void SoundboardEngine::audioDeviceStopped() {}

//==============================================================================
void SoundboardEngine::trigger (AudioClip::Ptr clip, float gain)
{
    if (clip == nullptr)
        return;

    int start1, size1, start2, size2;
    triggerFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 + size2 == 0)
        return;

    const int i = (size1 > 0) ? start1 : start2;
    triggerSlots[i].clip = std::move (clip);
    triggerSlots[i].gain = gain;
    triggerFifo.finishedWrite (1);
}

void SoundboardEngine::startVoice (AudioClip::Ptr clip, float gain)
{
    for (auto& v : voices)
    {
        if (! v.active)
        {
            v.clip     = std::move (clip);
            v.position = 0.0;
            v.gain     = gain;
            v.active   = true;
            return;
        }
    }
}

//==============================================================================
void SoundboardEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                         int numInputChannels,
                                                         float* const* outputChannelData,
                                                         int numOutputChannels,
                                                         int numSamples,
                                                         const juce::AudioIODeviceCallbackContext&)
{
    // 1) 收進被觸發的音效
    if (const int numReady = triggerFifo.getNumReady(); numReady > 0)
    {
        int start1, size1, start2, size2;
        triggerFifo.prepareToRead (numReady, start1, size1, start2, size2);
        for (int n = 0; n < size1; ++n) startVoice (std::move (triggerSlots[start1 + n].clip), triggerSlots[start1 + n].gain);
        for (int n = 0; n < size2; ++n) startVoice (std::move (triggerSlots[start2 + n].clip), triggerSlots[start2 + n].gain);
        triggerFifo.finishedRead (size1 + size2);
    }

    const int n = juce::jmin (numSamples, sfxScratch.getNumSamples());

    // 2) 把「只有音效」算進 sfxScratch（立體聲）
    sfxScratch.clear (0, 0, n);
    sfxScratch.clear (1, 0, n);
    float* sfxL = sfxScratch.getWritePointer (0);
    float* sfxR = sfxScratch.getWritePointer (1);

    const float master = masterGain.load();

    for (auto& v : voices)
    {
        if (! v.active || v.clip == nullptr)
            continue;

        const auto& src    = v.clip->buffer;
        const int   srcLen = src.getNumSamples();
        const int   srcCh  = src.getNumChannels();
        if (srcLen <= 1 || srcCh <= 0) { v.active = false; v.clip = nullptr; continue; }

        const double  ratio = v.clip->sourceSampleRate / deviceSampleRate;
        const float*  s0    = src.getReadPointer (0);
        const float*  s1    = (srcCh >= 2) ? src.getReadPointer (1) : s0;

        for (int i = 0; i < n; ++i)
        {
            const int idx = (int) v.position;
            if (idx >= srcLen - 1) { v.active = false; v.clip = nullptr; break; }

            const float frac = (float) (v.position - idx);
            sfxL[i] += (s0[idx] + frac * (s0[idx + 1] - s0[idx])) * v.gain * master;
            sfxR[i] += (s1[idx] + frac * (s1[idx + 1] - s1[idx])) * v.gain * master;

            v.position += ratio;
        }
    }

    // 3) 組主輸出：清空 -> (麥克風直通) -> 疊上音效
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    if (micPassthrough.load() && numInputChannels > 0)
    {
        const int micCh = juce::jmin (numInputChannels, 2);

        // 複製麥克風輸入到 micScratch
        for (int ch = 0; ch < micCh; ++ch)
            if (inputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::copy (micScratch.getWritePointer (ch), inputChannelData[ch], n);
            else
                micScratch.clear (ch, 0, n);

        // 降噪：高通 + 雜音門檻
        if (noiseReduction.load())
        {
            for (int ch = 0; ch < micCh; ++ch)
            {
                float* d = micScratch.getWritePointer (ch);
                for (int i = 0; i < n; ++i)
                    d[i] = micHP[ch].processSample (d[i]);
            }

            const float thr = gateThreshold.load();
            for (int i = 0; i < n; ++i)
            {
                float lvl = 0.0f;
                for (int ch = 0; ch < micCh; ++ch)
                    lvl = juce::jmax (lvl, std::abs (micScratch.getReadPointer (ch)[i]));

                gateEnv = lvl + (gateEnv - lvl) * ((lvl > gateEnv) ? envAttack : envRelease);
                const float target = (gateEnv > thr) ? 1.0f : 0.0f;
                gateGain = target + (gateGain - target) * ((target > gateGain) ? gateOpen : gateClose);

                for (int ch = 0; ch < micCh; ++ch)
                    micScratch.getWritePointer (ch)[i] *= gateGain;
            }
        }

        // 把處理後的麥克風疊到輸出
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
            {
                const int mc = juce::jmin (ch, micCh - 1);
                juce::FloatVectorOperations::add (outputChannelData[ch], micScratch.getReadPointer (mc), n);
            }
    }

    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::add (outputChannelData[ch], (ch % 2 == 0) ? sfxL : sfxR, n);

    // 4) 把「只有音效」餵給監聽裝置
    if (monitorEnabled.load())
        monitorFifo.write (sfxL, sfxR, n);
}

//==============================================================================
AudioClip::Ptr SoundboardEngine::loadClip (const juce::File& file, juce::String& errorMessage)
{
    if (! file.existsAsFile())
    {
        errorMessage = utf8 ("檔案不存在：") + file.getFullPathName();
        return nullptr;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    juce::File tempWav;

    if (reader == nullptr)   // JUCE 讀不了(很可能 mp4/m4a/aac) -> 用 ffmpeg 轉暫存 wav
    {
        tempWav = transcodeWithFFmpeg (file, errorMessage);
        if (tempWav == juce::File())
            return nullptr;
        reader.reset (formatManager.createReaderFor (tempWav));
    }

    if (reader == nullptr)
    {
        errorMessage = utf8 ("無法解碼此檔案格式。");
        if (tempWav != juce::File()) tempWav.deleteFile();
        return nullptr;
    }

    auto* clip = new AudioClip();
    clip->name             = file.getFileNameWithoutExtension();
    clip->sourceSampleRate = reader->sampleRate;

    const int len = (int) reader->lengthInSamples;
    clip->buffer.setSize ((int) reader->numChannels, juce::jmax (1, len));
    reader->read (&clip->buffer, 0, len, 0, true, true);

    if (tempWav != juce::File())
        tempWav.deleteFile();

    return AudioClip::Ptr (clip);
}

//==============================================================================
juce::String SoundboardEngine::resolveFFmpeg() const
{
    auto beside = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                      .getParentDirectory().getChildFile ("ffmpeg.exe");
    if (beside.existsAsFile())
        return beside.getFullPathName();

    return "ffmpeg";
}

juce::File SoundboardEngine::transcodeWithFFmpeg (const juce::File& src, juce::String& errorMessage)
{
    const juce::String ff = resolveFFmpeg();

    auto out = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("sb_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()) + ".wav");

    juce::StringArray cmd;
    cmd.add (ff);
    cmd.add ("-y");
    cmd.add ("-i"); cmd.add (src.getFullPathName());
    cmd.add ("-vn");
    cmd.add ("-ac"); cmd.add ("2");
    cmd.add ("-ar"); cmd.add ("48000");
    cmd.add (out.getFullPathName());

    juce::ChildProcess proc;
    if (! proc.start (cmd))
    {
        errorMessage = utf8 ("找不到 ffmpeg。MP4 需要它來解碼：請把 ffmpeg.exe 放到程式旁邊，或加進系統 PATH。");
        return {};
    }

    proc.waitForProcessToFinish (30000);

    if (! out.existsAsFile() || out.getSize() == 0)
    {
        errorMessage = utf8 ("ffmpeg 轉檔失敗（檔案可能損毀，或裡面沒有音軌）。");
        out.deleteFile();
        return {};
    }

    return out;
}
