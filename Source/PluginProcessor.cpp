#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using PID = juce::ParameterID;

static juce::AudioParameterFloatAttributes fmt (std::function<juce::String (float, int)> f)
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f));
}

juce::AudioProcessorValueTreeState::ParameterLayout ReverbFXProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto dB  = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto pct = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; };
    auto pct01 = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.f)) + " %"; };
    auto ms  = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; };
    auto sec = [] (float v, int) { return juce::String (v, 2) + " s"; };
    auto hz  = [] (float v, int)
    {
        return v >= 1000.f ? juce::String (v / 1000.f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    };

    l.add (std::make_unique<juce::AudioParameterChoice> (PID { "type", 1 }, "Type",
                                                         juce::StringArray { "Plate", "Hall", "Delay", "Harmony", "SPX Plate" }, 0));

    l.add (std::make_unique<APF> (PID { "input", 1 },  "Input",  juce::NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, fmt (dB)));
    l.add (std::make_unique<APF> (PID { "mix", 1 },    "Mix",    juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.3f, fmt (pct01)));
    l.add (std::make_unique<APF> (PID { "output", 1 }, "Output", juce::NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, fmt (dB)));
    l.add (std::make_unique<APF> (PID { "eqlow", 1 },  "Rev EQ Low",  juce::NormalisableRange<float> (-18.f, 18.f, 0.1f), 0.f, fmt (dB)));
    l.add (std::make_unique<APF> (PID { "eqhigh", 1 }, "Rev EQ High", juce::NormalisableRange<float> (-18.f, 18.f, 0.1f), 0.f, fmt (dB)));

    // Plate & Hall
    l.add (std::make_unique<APF> (PID { "decay", 1 },     "Decay",     juce::NormalisableRange<float> (0.3f, 20.f, 0.01f, 0.349f), 2.5f, fmt (sec)));
    l.add (std::make_unique<APF> (PID { "predelay", 1 },  "PreDelay",  juce::NormalisableRange<float> (0.f, 200.f, 1.f), 15.f, fmt (ms)));
    l.add (std::make_unique<APF> (PID { "room", 1 },      "Room",      juce::NormalisableRange<float> (0.f, 100.f, 1.f), 50.f, fmt (pct)));
    l.add (std::make_unique<APF> (PID { "diffusion", 1 }, "Diffusion", juce::NormalisableRange<float> (0.f, 100.f, 1.f), 70.f, fmt (pct)));
    l.add (std::make_unique<APF> (PID { "damping", 1 },   "Damping",   juce::NormalisableRange<float> (0.f, 100.f, 1.f), 40.f, fmt (pct)));
    l.add (std::make_unique<APF> (PID { "lowcut", 1 },    "Low Cut",   juce::NormalisableRange<float> (20.f, 20000.f, 1.f, 0.25f), 80.f, fmt (hz)));
    l.add (std::make_unique<APF> (PID { "hicut", 1 },     "Hi Cut",    juce::NormalisableRange<float> (20.f, 20000.f, 1.f, 0.25f), 16000.f, fmt (hz)));

    // Delay
    l.add (std::make_unique<APF> (PID { "dlytime", 1 }, "Delay Time",     juce::NormalisableRange<float> (30.f, 2000.f, 1.f), 500.f, fmt (ms)));
    l.add (std::make_unique<APF> (PID { "dlyfb", 1 },   "Delay Feedback", juce::NormalisableRange<float> (0.f, 95.f, 1.f), 35.f, fmt (pct)));
    l.add (std::make_unique<APF> (PID { "dlyhpf", 1 },  "Delay Hpf",      juce::NormalisableRange<float> (20.f, 2000.f, 1.f, 0.289f), 20.f, fmt (hz)));
    l.add (std::make_unique<APF> (PID { "dlylpf", 1 },  "Delay Lpf",      juce::NormalisableRange<float> (1000.f, 20000.f, 1.f, 0.445f), 8000.f, fmt (hz)));

    // Harmony
    l.add (std::make_unique<juce::AudioParameterChoice> (PID { "hkey", 1 }, "Harmony Key",
                                                         juce::StringArray { "C", "D", "E", "F", "G", "A", "B" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (PID { "hmode", 1 }, "Harmony Mode",
                                                         juce::StringArray { "OCT-", "5th L", "3rd L", "3rd H", "5th H", "OCT+",
                                                                             "OCT- & OCT+", "5th L & 3rd H", "5th L & 5th H",
                                                                             "3rd L & 3rd H", "3rd H & 5th H" }, 3));
    l.add (std::make_unique<juce::AudioParameterChoice> (PID { "hscale", 1 }, "Harmony Sharp",
                                                         juce::StringArray { "Major", "Minor" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (PID { "htone", 1 }, "Harmony Tone",
                                                         juce::StringArray { "Normal", "Warm", "Bright" }, 0));
    l.add (std::make_unique<APF> (PID { "hout", 1 }, "Harmony Out", juce::NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, fmt (dB)));

    return l;
}

ReverbFXProcessor::ReverbFXProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()),
      presets (apvts)
{
    typeParam = apvts.getRawParameterValue ("type");
    pInput  = apvts.getRawParameterValue ("input");   pMix    = apvts.getRawParameterValue ("mix");
    pOutput = apvts.getRawParameterValue ("output");  pEqLow  = apvts.getRawParameterValue ("eqlow");
    pEqHigh = apvts.getRawParameterValue ("eqhigh");  pDecay  = apvts.getRawParameterValue ("decay");
    pPre    = apvts.getRawParameterValue ("predelay"); pRoom  = apvts.getRawParameterValue ("room");
    pDiff   = apvts.getRawParameterValue ("diffusion"); pDamp = apvts.getRawParameterValue ("damping");
    pLowCut = apvts.getRawParameterValue ("lowcut");  pHiCut  = apvts.getRawParameterValue ("hicut");
    pDTime  = apvts.getRawParameterValue ("dlytime"); pDFb    = apvts.getRawParameterValue ("dlyfb");
    pDHpf   = apvts.getRawParameterValue ("dlyhpf");  pDLpf   = apvts.getRawParameterValue ("dlylpf");
    pHKey   = apvts.getRawParameterValue ("hkey");    pHMode  = apvts.getRawParameterValue ("hmode");
    pHScale = apvts.getRawParameterValue ("hscale");  pHTone  = apvts.getRawParameterValue ("htone");
    pHOut   = apvts.getRawParameterValue ("hout");

    beatOriginMs.store (juce::Time::getMillisecondCounterHiRes());
}

bool ReverbFXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void ReverbFXProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    reverb.prepare (sampleRate);
    spx.prepare (sampleRate);
    delay.prepare (sampleRate);
    harmony.prepare (sampleRate);
    for (auto& b : lowShelf)  b.reset();
    for (auto& b : highShelf) b.reset();
    lastLowDb = lastHighDb = 1000.f;
    lastType = -1;

    inSm.reset (sampleRate, 0.03);  inSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInput->load()));
    mixSm.reset (sampleRate, 0.03); mixSm.setCurrentAndTargetValue (pMix->load());
    outSm.reset (sampleRate, 0.03); outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
}

void ReverbFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);
    if (numOut < 2) return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    const bool monoIn = numIn < 2;

    const int type = getFxType();
    if (type != lastType)
    {
        if (lastType >= 0) { reverb.clear(); spx.clear(); delay.clear(); harmony.clear(); }
        lastType = type;
    }

    if (type < 2)
        reverb.setParams (type, pDecay->load(), pPre->load(), pRoom->load() * 0.01f, pDiff->load() * 0.01f,
                          pDamp->load() * 0.01f, pLowCut->load(), pHiCut->load());
    else if (type == 2)
        delay.setParams (pDTime->load(), pDFb->load() * 0.01f, pDHpf->load(), pDLpf->load());
    else if (type == 4)
        spx.setParams (pDecay->load(), pPre->load(), pRoom->load() * 0.01f, pDiff->load() * 0.01f,
                       pDamp->load() * 0.01f, pLowCut->load(), pHiCut->load());
    else
        harmony.setParams ((int) std::lround (pHKey->load()), (int) std::lround (pHMode->load()),
                           (int) std::lround (pHScale->load()), (int) std::lround (pHTone->load()), pHOut->load());

    const float lowDb = pEqLow->load(), highDb = pEqHigh->load();
    if (lowDb != lastLowDb)
    {
        for (auto& b : lowShelf) b.lowShelf (sampleRateHz, 800.0, lowDb);
        lastLowDb = lowDb;
    }
    if (highDb != lastHighDb)
    {
        for (auto& b : highShelf) b.highShelf (sampleRateHz, 8000.0, highDb);
        lastHighDb = highDb;
    }

    inSm.setTargetValue (juce::Decibels::decibelsToGain (pInput->load()));
    mixSm.setTargetValue (pMix->load());
    outSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    const int in1 = input1Source.load(),   in2 = input2Source.load();
    const int out1 = output1Source.load(), out2 = output2Source.load();

    for (int i = 0; i < n; ++i)
    {
        const float gIn = inSm.getNextValue(), mix = mixSm.getNextValue(), gOut = outSm.getNextValue();
        // input routing: 0 = None, 1 = channel 1 (L), 2 = channel 2 (R)
        const float rawL = L[i], rawR = monoIn ? L[i] : R[i];
        const float srcL = in1 == 0 ? 0.f : (in1 == 1 ? rawL : rawR);
        const float srcR = in2 == 0 ? 0.f : (in2 == 1 ? rawL : rawR);
        const float dl = srcL * gIn;
        const float dr = (monoIn ? srcL : srcR) * gIn;

        float wl, wr;
        if (type < 2)       reverb.process (dl, dr, wl, wr);
        else if (type == 2) delay.process (dl, dr, wl, wr);
        else if (type == 4) spx.process (dl, dr, wl, wr);
        else                harmony.process (dl, dr, wl, wr);

        wl = highShelf[0].process (lowShelf[0].process (wl));
        wr = highShelf[1].process (lowShelf[1].process (wr));

        const float mL = (dl * (1.f - mix) + wl * mix) * gOut;
        const float mR = (dr * (1.f - mix) + wr * mix) * gOut;
        L[i] = out1 == 0 ? 0.f : (out1 == 1 ? mL : mR);
        R[i] = out2 == 0 ? 0.f : (out2 == 1 ? mL : mR);
    }
}

bool ReverbFXProcessor::tapTempo()
{
    const int type = getFxType();

    if (type == 3)      // Harmony: TAP / Spasi = ganti Sharp (Major <-> Minor)
    {
        if (auto* p = apvts.getParameter ("hscale"))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.f : 0.f);
            p->endChangeGesture();
        }
        return true;
    }

    if (type != 2) return false;

    const double now = juce::Time::getMillisecondCounterHiRes();
    beatOriginMs.store (now);                       // reset LED tempo

    if (lastTapMs > 0.0 && now - lastTapMs < 2000.0)
    {
        tapIntervals.push_back (now - lastTapMs);
        if (tapIntervals.size() > 4) tapIntervals.erase (tapIntervals.begin());

        double sum = 0.0;
        for (auto v : tapIntervals) sum += v;
        const double avg = sum / (double) tapIntervals.size();

        if (auto* p = apvts.getParameter ("dlytime"))
        {
            const float ms = p->getNormalisableRange().snapToLegalValue ((float) avg);
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (ms));
            p->endChangeGesture();
        }
    }
    else
    {
        tapIntervals.clear();
    }
    lastTapMs = now;
    return true;
}

void ReverbFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ReverbFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset.store (juce::jlimit (0, PresetManager::numSlots - 1,
                                               (int) apvts.state.getProperty ("preset", 0)));
        }
}

juce::AudioProcessorEditor* ReverbFXProcessor::createEditor()
{
    return new ReverbFXEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbFXProcessor();
}
