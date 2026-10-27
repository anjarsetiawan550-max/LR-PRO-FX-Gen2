#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#include <array>

// 10 preset slots, disimpan di file XML (dipakai bersama oleh standalone & VST3).
class PresetManager
{
public:
    static constexpr int numSlots = 10;

    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
    {
        if (! load())
        {
            makeFactoryPresets();
            save();
        }
    }

    bool isUsed (int i) const        { return slots[(size_t) i].used; }
    juce::String getName (int i) const { return slots[(size_t) i].name; }

    void store (int i, const juce::String& name)
    {
        auto& s = slots[(size_t) i];
        s.used = true;
        s.name = name;
        s.values.clear();
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                s.values[rp->paramID] = rp->getValue();
        save();
    }

    bool recall (int i)
    {
        auto& s = slots[(size_t) i];
        if (! s.used) return false;

        for (auto* p : apvts.processor.getParameters())
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                auto it = s.values.find (rp->paramID);
                if (it == s.values.end()) continue;
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (it->second);
                rp->endChangeGesture();
            }
        }
        return true;
    }

private:
    struct Slot
    {
        bool used = false;
        juce::String name;
        std::map<juce::String, float> values;
    };

    juce::AudioProcessorValueTreeState& apvts;
    std::array<Slot, numSlots> slots;

    static juce::File getFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("ReverbFX").getChildFile ("presets.xml");
    }

    bool load()
    {
        auto f = getFile();
        if (! f.existsAsFile()) return false;
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("ReverbFXPresets")) return false;

        const int ver = xml->getIntAttribute ("ver", 1);

        for (auto* e : xml->getChildWithTagNameIterator ("Preset"))
        {
            const int idx = e->getIntAttribute ("slot", -1);
            if (idx < 0 || idx >= numSlots) continue;
            auto& s = slots[(size_t) idx];
            s.used = true;
            s.name = e->getStringAttribute ("name");
            s.values.clear();
            for (auto* p : e->getChildWithTagNameIterator ("P"))
                s.values[p->getStringAttribute ("id")] = (float) p->getDoubleAttribute ("v");
        }

        // v1 -> v2: Low Cut / Hi Cut range changed (LR style 20..20000 Hz). Convert stored values.
        if (ver < 2)
        {
            const juce::NormalisableRange<float> oldLow (20.f, 500.f, 1.f, 0.387f);
            const juce::NormalisableRange<float> oldHi  (1000.f, 20000.f, 1.f, 0.445f);
            auto* pLow = apvts.getParameter ("lowcut");
            auto* pHi  = apvts.getParameter ("hicut");
            for (auto& s : slots)
            {
                if (! s.used) continue;
                auto itL = s.values.find ("lowcut");
                if (itL != s.values.end() && pLow != nullptr)
                    itL->second = pLow->convertTo0to1 (oldLow.convertFrom0to1 (itL->second));
                auto itH = s.values.find ("hicut");
                if (itH != s.values.end() && pHi != nullptr)
                    itH->second = pHi->convertTo0to1 (oldHi.convertFrom0to1 (itH->second));
            }
            save();
        }

        // v2 -> v3 -> v4: parameter Type bertambah pilihan (Harmony, lalu SPX Plate), jadi nilai
        // ternormalisasinya berubah. Ubah dulu ke nomor pilihan lama (v1/v2: 3 pilihan, v3: 4 pilihan),
        // lalu ke normalisasi baru (5 pilihan -> nomor / 4). Setelah itu isi slot contoh yang masih kosong.
        if (ver < 4)
        {
            const float oldDiv = ver < 3 ? 2.f : 3.f;
            for (auto& s : slots)
            {
                if (! s.used) continue;
                auto it = s.values.find ("type");
                if (it != s.values.end())
                    it->second = (float) std::lround (it->second * oldDiv) / 4.f;
            }
            if (ver < 3 && ! slots[3].used) makeHarmonyFactory();
            if (! slots[4].used) makeSpxFactory();
            save();
        }
        return true;
    }

    void save() const
    {
        juce::XmlElement root ("ReverbFXPresets");
        root.setAttribute ("ver", 4);
        for (int i = 0; i < numSlots; ++i)
        {
            const auto& s = slots[(size_t) i];
            if (! s.used) continue;
            auto* e = root.createNewChildElement ("Preset");
            e->setAttribute ("slot", i);
            e->setAttribute ("name", s.name);
            for (auto& kv : s.values)
            {
                auto* p = e->createNewChildElement ("P");
                p->setAttribute ("id", kv.first);
                p->setAttribute ("v", (double) kv.second);
            }
        }
        auto f = getFile();
        f.getParentDirectory().createDirectory();
        root.writeTo (f);
    }

    void setFactory (int slot, const juce::String& name,
                     std::initializer_list<std::pair<const char*, float>> vals)
    {
        auto& s = slots[(size_t) slot];
        s.used = true;
        s.name = name;
        s.values.clear();
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                s.values[rp->paramID] = rp->getDefaultValue();
        for (auto& v : vals)
            if (auto* rp = apvts.getParameter (v.first))
                s.values[rp->paramID] = rp->convertTo0to1 (v.second);
    }

    void makeHarmonyFactory()
    {
        setFactory (3, "Harmony 3rd H", { { "type", 3 }, { "hkey", 0 }, { "hmode", 3 }, { "hscale", 0 },
                                          { "htone", 0 }, { "hout", 0 }, { "mix", 0.5f } });
    }

    void makeSpxFactory()
    {
        setFactory (4, "SPX Plate", { { "type", 4 }, { "decay", 1.8f }, { "predelay", 15 }, { "room", 50 },
                                      { "diffusion", 80 }, { "damping", 65 }, { "lowcut", 100 }, { "hicut", 12000 },
                                      { "mix", 0.3f } });
    }

    void makeFactoryPresets()
    {
        setFactory (0, "Plate Bright", { { "type", 0 }, { "decay", 2.0f }, { "predelay", 10 }, { "room", 45 },
                                         { "diffusion", 85 }, { "damping", 25 }, { "lowcut", 80 }, { "hicut", 14000 }, { "mix", 0.3f } });
        setFactory (1, "Hall Large",   { { "type", 1 }, { "decay", 4.5f }, { "predelay", 25 }, { "room", 70 },
                                         { "diffusion", 70 }, { "damping", 45 }, { "lowcut", 100 }, { "hicut", 9000 }, { "mix", 0.35f } });
        setFactory (2, "Slap Delay",   { { "type", 2 }, { "dlytime", 120 }, { "dlyfb", 20 }, { "dlyhpf", 200 },
                                         { "dlylpf", 7000 }, { "mix", 0.25f } });
        makeHarmonyFactory();
        makeSpxFactory();
    }
};
