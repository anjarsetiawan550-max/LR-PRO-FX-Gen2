#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DspUtils.h"
#include "ReverbEngine.h"
#include "SpxPlateEngine.h"
#include "DelayEngine.h"
#include "HarmonyEngine.h"
#include "PresetManager.h"

class ReverbFXProcessor : public juce::AudioProcessor
{
public:
    ReverbFXProcessor();
    ~ReverbFXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LR FX Gen 2"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- dipakai oleh editor ----
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    int getFxType() const { return juce::jlimit (0, 4, (int) std::lround (typeParam->load())); }
    float getParamReal (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    /** Tipe Delay: tap tempo. Tipe Harmony: ganti Major/Minor (Sharp). Mengembalikan false untuk tipe lain. */
    bool tapTempo();

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    std::atomic<int> currentPreset { 0 };
    std::atomic<double> beatOriginMs { 0.0 };

    // Audio I/O channel routing (Utility panel): 0 = None, 1 = channel 1 (L), 2 = channel 2 (R)
    std::atomic<int> input1Source { 1 }, input2Source { 2 };
    std::atomic<int> output1Source { 1 }, output2Source { 2 };

private:
    std::atomic<float>* typeParam = nullptr;
    std::atomic<float>* pInput = nullptr; std::atomic<float>* pMix = nullptr; std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pEqLow = nullptr; std::atomic<float>* pEqHigh = nullptr;
    std::atomic<float>* pDecay = nullptr; std::atomic<float>* pPre = nullptr; std::atomic<float>* pRoom = nullptr;
    std::atomic<float>* pDiff = nullptr;  std::atomic<float>* pDamp = nullptr;
    std::atomic<float>* pLowCut = nullptr; std::atomic<float>* pHiCut = nullptr;
    std::atomic<float>* pDTime = nullptr; std::atomic<float>* pDFb = nullptr;
    std::atomic<float>* pDHpf = nullptr;  std::atomic<float>* pDLpf = nullptr;
    std::atomic<float>* pHKey = nullptr;  std::atomic<float>* pHMode = nullptr; std::atomic<float>* pHScale = nullptr;
    std::atomic<float>* pHTone = nullptr; std::atomic<float>* pHOut = nullptr;

    ReverbEngine reverb;
    SpxPlateEngine spx;
    DelayEngine delay;
    HarmonyEngine harmony;
    Biquad lowShelf[2], highShelf[2];
    float lastLowDb = 1000.f, lastHighDb = 1000.f;
    int lastType = -1;
    double sampleRateHz = 44100.0;

    juce::SmoothedValue<float> inSm, mixSm, outSm;

    std::vector<double> tapIntervals;
    double lastTapMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbFXProcessor)
};
