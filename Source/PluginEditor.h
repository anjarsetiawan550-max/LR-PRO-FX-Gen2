#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// Tombol bergambar (up / down) dengan overlay glow opsional
// ---------------------------------------------------------------------------
class ImgButton : public juce::Button
{
public:
    ImgButton (const juce::Image& up, const juce::Image& down, const juce::Image& glowImg = {})
        : juce::Button ("btn"), upImg (up), downImg (down), glow (glowImg)
    {
        setWantsKeyboardFocus (false);
    }

    void setGlow (bool on)        { if (on != glowOn)  { glowOn = on;  repaint(); } }
    void setForcedDown (bool on)  { if (on != forced)  { forced = on;  repaint(); } }

    void paintButton (juce::Graphics& g, bool, bool isDown) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage ((isDown || forced) ? downImg : upImg, getLocalBounds().toFloat());
        if (glowOn && glow.isValid())
            g.drawImage (glow, getLocalBounds().toFloat());
    }

private:
    juce::Image upImg, downImg, glow;
    bool glowOn = false, forced = false;
};

// ---------------------------------------------------------------------------
// Knop filmstrip (64 frame vertikal) -> juce::Slider
// ---------------------------------------------------------------------------
class FilmstripKnob : public juce::Slider
{
public:
    explicit FilmstripKnob (const juce::Image& s) : strip (s)
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity (220);
        setWantsKeyboardFocus (false);
    }

    void paint (juce::Graphics& g) override
    {
        const int frames = 64;
        const double norm = valueToProportionOfLength (getValue());
        const int frame = juce::jlimit (0, frames - 1, (int) std::lround (norm * (frames - 1)));
        const int fw = strip.getWidth();
        const int fh = strip.getHeight() / frames;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, frame * fh, fw, fh);
    }

private:
    juce::Image strip;
};

// ---------------------------------------------------------------------------
// Knop TYPE / PROGRAM: roda mouse / drag = ubah nilai, klik kiri = konfirmasi
// ---------------------------------------------------------------------------
class TypeKnob : public juce::Component
{
public:
    explicit TypeKnob (const juce::Image& s) : strip (s) { setWantsKeyboardFocus (false); }

    std::function<void (int)> onStep;     // +n / -n langkah
    std::function<void()> onClick;        // klik kiri

    void setNormalised (float v)
    {
        if (std::abs (v - norm) > 1.0e-4f) { norm = v; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const int frames = 64;
        const int frame = juce::jlimit (0, frames - 1, (int) std::lround (norm * (frames - 1)));
        const int fw = strip.getWidth();
        const int fh = strip.getHeight() / frames;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, frame * fh, fw, fh);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        leftClick = e.mods.isLeftButtonDown();
        lastY = e.y;
        dragAccum = 0.f;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        dragAccum += (float) (lastY - e.y);
        lastY = e.y;
        const float stepPx = 10.f;
        while (dragAccum >= stepPx)  { if (onStep) onStep (+1); dragAccum -= stepPx; }
        while (dragAccum <= -stepPx) { if (onStep) onStep (-1); dragAccum += stepPx; }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (leftClick && ! e.mouseWasDraggedSinceMouseDown() && onClick)
            onClick();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        int steps = 0;
        if (! w.isSmooth)
        {
            steps = w.deltaY > 0.f ? 1 : (w.deltaY < 0.f ? -1 : 0);
        }
        else
        {
            wheelAccum += w.deltaY;
            steps = (int) (wheelAccum / 0.15f);
            wheelAccum -= (float) steps * 0.15f;
        }
        if (steps != 0 && onStep) onStep (steps);
    }

private:
    juce::Image strip;
    float norm = 0.f, dragAccum = 0.f, wheelAccum = 0.f;
    int lastY = 0;
    bool leftClick = false;
};

// ---------------------------------------------------------------------------
// Model tampilan LCD
// ---------------------------------------------------------------------------
struct LcdModel
{
    enum Mode { Home, Menu, Store };
    Mode mode = Home;

    // Home
    juce::String typeName, presetLine, hint, bpmText;
    int led = -1;                       // 0..3, -1 = mati

    // Menu
    juce::String title;
    juce::StringArray labels, values;
    int selected = 0, first = 0;

    // Store
    juce::String storeTitle;

    bool operator== (const LcdModel& o) const
    {
        return mode == o.mode && typeName == o.typeName && presetLine == o.presetLine && hint == o.hint
            && bpmText == o.bpmText && led == o.led && title == o.title && labels == o.labels
            && values == o.values && selected == o.selected && first == o.first && storeTitle == o.storeTitle;
    }
    bool operator!= (const LcdModel& o) const { return ! (*this == o); }
};

class LcdComponent : public juce::Component
{
public:
    LcdComponent();

    void setModel (const LcdModel& m);
    void paint (juce::Graphics&) override;
    void resized() override;

    // popup nama preset
    void showStore (const juce::String& initialName);
    void hideStore();
    juce::String getStoreName() const { return nameEditor.getText(); }

    std::function<void()> onSubmit, onCancel;

private:
    LcdModel model;
    juce::Image screenBg, glass, bezel, redOn, redOff, greenOn, greenOff;
    juce::TextEditor nameEditor;
};

// ---------------------------------------------------------------------------
// Utility panel: Audio I/O channel routing (same as LR PRO FX)
// ---------------------------------------------------------------------------
class UtilityPanel : public juce::Component
{
public:
    explicit UtilityPanel (ReverbFXProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                       // load current routing into the combo boxes

    std::function<void()> onBack;

private:
    ReverbFXProcessor& proc;
    juce::Label title, labels[4];
    juce::ComboBox choices[4];
    juce::TextButton back;
};

// ---------------------------------------------------------------------------
// Editor utama
// ---------------------------------------------------------------------------
class ReverbFXEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ReverbFXEditor (ReverbFXProcessor&);
    ~ReverbFXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    enum class View { Home, Menu, Store };

    struct MenuItem { const char* id; const char* label; double step; bool mult; };

    void timerCallback() override;

    const MenuItem* menuItems (int& count) const;
    void navigate (int dir);                 // +1 = Up, -1 = Down
    void onKnobStep (int steps);
    void onKnobClick();
    void stepParam (const MenuItem& item, int steps);
    void setType (int idx);
    void doTap();
    void toggleUtility();
    void startStore();
    void commitStore();
    void cancelStore();
    void flash (const juce::String& text);

    ReverbFXProcessor& proc;

    juce::Image background;
    LcdComponent lcd;
    ImgButton btnDown, btnUp, btnUtility, btnMenu, btnExit, btnStore, btnTap;
    TypeKnob typeKnob;
    FilmstripKnob knobInput, knobMix, knobOutput, knobLow, knobHigh;
    UtilityPanel utilityPanel;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAtt> attInput, attMix, attOutput, attLow, attHigh;


    View view = View::Home;
    int browseIdx = 0, storeSlot = 0;
    int menuSel = 0, menuFirst = 0;

    juce::String flashText;
    double flashUntil = 0.0, lastStepMs = 0.0, tapFlashUntil = 0.0;
    bool spaceHeld = false;
    LcdModel lastModel;
    bool modelValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbFXEditor)
};
