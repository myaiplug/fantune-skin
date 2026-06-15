#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FanTuneLookAndFeel.h"

namespace FanTune::UI
{

constexpr int kChassisW = 960;
constexpr int kChassisH = 480;
constexpr int kBoxFanBlades = 3;

inline juce::Font labelFont (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions().withName ("Segoe UI")
                                       .withHeight (size)
                                       .withStyle (bold ? "Bold" : "Regular"));
}

inline void paintChassisBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto bf = bounds.toFloat();

    for (int i = 4; i >= 1; --i)
    {
        const float expand = (float) i * 2.5f;
        g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.018f * (float) i));
        g.fillRoundedRectangle (bf.expanded (expand), 12.0f + expand * 0.3f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.92f));
    g.fillRoundedRectangle (bf.expanded (3.0f), 14.0f);

    juce::ColourGradient face (juce::Colour (0xFF1C1C28), bf.getX(), bf.getY(),
                               juce::Colour (0xFF0E0E18), bf.getX(), bf.getBottom(), false);
    face.addColour (0.30, juce::Colour (0xFF141420));
    face.addColour (0.60, juce::Colour (0xFF111118));
    g.setGradientFill (face);
    g.fillRoundedRectangle (bf, 12.0f);

    g.setColour (juce::Colour (0x1FFFFFFF));
    for (int x = bounds.getX(); x < bounds.getRight(); x += 3)
        g.drawVerticalLine (x, bf.getY(), bf.getBottom());

    g.setColour (juce::Colour (0x14FFFFFF));
    for (int y = bounds.getY(); y < bounds.getBottom(); y += 5)
        g.drawHorizontalLine (y, bf.getX(), bf.getRight());

    juce::ColourGradient vignette (juce::Colours::transparentBlack, bf.getCentreX(), bf.getCentreY(),
                                   juce::Colour (0x99000000), bf.getCentreX(), bf.getBottom(), true);
    g.setGradientFill (vignette);
    g.fillRoundedRectangle (bf, 12.0f);

    g.setColour (FanTune::Colour::CyanFaint);
    g.drawRoundedRectangle (bf.reduced (0.5f), 12.0f, 1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (bf.expanded (1.0f), 13.0f, 2.0f);
}

inline void paintHeader (juce::Graphics& g, juce::Rectangle<int> header)
{
    juce::ColourGradient grad (juce::Colour (0xFF1A1A2C), 0.0f, (float) header.getY(),
                               juce::Colour (0xFF12121E), 0.0f, (float) header.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (header);

    g.setColour (FanTune::Colour::CyanDim.withAlpha (0.35f));
    juce::ColourGradient line (juce::Colours::transparentBlack, (float) header.getX(), (float) header.getBottom(),
                               FanTune::Colour::CyanDim, (float) header.getCentreX(), (float) header.getBottom(), false);
    line.addColour (1.0, juce::Colours::transparentBlack);
    g.setGradientFill (line);
    g.drawHorizontalLine (header.getBottom() - 1, (float) header.getX(), (float) header.getRight());
}

inline void paintBrandTitle (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto text = "FANTUNE";
    g.setFont (labelFont (18.0f));

    for (int pass = 3; pass >= 1; --pass)
    {
        const float alpha = 0.12f * (float) pass;
        const float expand = (float) pass * 0.8f;
        g.setColour (FanTune::Colour::CyanPrimary.withAlpha (alpha));
        g.drawText (text, area.expanded ((int) expand), juce::Justification::centredLeft);
    }

    g.setColour (FanTune::Colour::TextPrimary);
    g.drawText (text, area, juce::Justification::centredLeft);
}

inline void paintLogoMark (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto cx = area.getCentreX();
    const auto cy = area.getCentreY();
    const float s = area.getWidth() / 30.0f;

    g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.25f));
    g.fillEllipse (area.expanded (2.0f));

    juce::Path b1, b2, b3;
    b1.startNewSubPath (cx, cy);
    b1.quadraticTo (cx - 5*s, cy - 9*s, cx + 3*s, cy - 12*s);
    b1.quadraticTo (cx + 9*s, cy - 13*s, cx + 7*s, cy - 5*s);
    b1.closeSubPath();

    b2.startNewSubPath (cx, cy);
    b2.quadraticTo (cx + 9*s, cy - 3*s, cx + 12*s, cy + 5*s);
    b2.quadraticTo (cx + 13*s, cy + 11*s, cx + 5*s, cy + 9*s);
    b2.closeSubPath();

    b3.startNewSubPath (cx, cy);
    b3.quadraticTo (cx - 1*s, cy + 9*s, cx - 9*s, cy + 10*s);
    b3.quadraticTo (cx - 15*s, cy + 9*s, cx - 11*s, cy + 2*s);
    b3.closeSubPath();

    g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.7f)); g.fillPath (b1);
    g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.5f)); g.fillPath (b2);
    g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.6f)); g.fillPath (b3);

    g.setColour (FanTune::Colour::CyanPrimary);
    g.fillEllipse (cx - 3.5f*s, cy - 3.5f*s, 7.0f*s, 7.0f*s);
    g.setColour (juce::Colours::white);
    g.fillEllipse (cx - 1.5f*s, cy - 1.5f*s, 3.0f*s, 3.0f*s);
}

inline void paintTrackLines (juce::Graphics& g, int width)
{
    auto drawTrack = [&] (int y)
    {
        juce::ColourGradient t (juce::Colours::transparentBlack, 0.0f, (float) y,
                              FanTune::Colour::CyanDim.withAlpha (0.55f), (float) width * 0.5f, (float) y, false);
        t.addColour (1.0, juce::Colours::transparentBlack);
        g.setGradientFill (t);
        g.drawHorizontalLine (y, 0.0f, (float) width);
    };
    drawTrack (52);
    drawTrack (kChassisH - 64);
}

inline void paintPanelDividers (juce::Graphics& g)
{
    auto drawDivider = [&] (int x)
    {
        juce::ColourGradient d (juce::Colours::transparentBlack, (float) x, 68.0f,
                               FanTune::Colour::CyanDim.withAlpha (0.28f), (float) x, 240.0f, false);
        d.addColour (1.0, juce::Colours::transparentBlack);
        g.setGradientFill (d);
        g.drawVerticalLine (x, 68.0f, (float) (kChassisH - 68));
    };
    drawDivider (168);
    drawDivider (kChassisW - 168);
    drawDivider (kChassisW - 140);
}

inline void paintFooter (juce::Graphics& g, juce::Rectangle<int> footer)
{
    juce::ColourGradient grad (juce::Colour (0xFF13131F), 0.0f, (float) footer.getY(),
                               juce::Colour (0xFF0F0F1A), 0.0f, (float) footer.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (footer);
    g.setColour (FanTune::Colour::CyanFaint);
    g.drawHorizontalLine (footer.getY(), (float) footer.getX(), (float) footer.getRight());
}

class HeaderActionButton final : public juce::TextButton
{
public:
    explicit HeaderActionButton (const juce::String& text) : juce::TextButton (text)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
};

class KeySelector final : public juce::Component
{
public:
    KeySelector()
    {
        addAndMakeVisible (combo);
        combo.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
        combo.setJustificationType (juce::Justification::centred);
        combo.setTextWhenNothingSelected ("C");
        combo.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::ComboBox& getComboBox() { return combo; }

    void paint (juce::Graphics& g) override
    {
        g.setColour (FanTune::Colour::TextDim.withAlpha (0.35f));
        g.setFont (labelFont (8.0f, false));
        g.drawText ("KEY", getLocalBounds().removeFromTop (10), juce::Justification::centredLeft);
    }

    void resized() override
    {
        combo.setBounds (0, 12, getWidth(), getHeight() - 12);
    }

private:
    juce::ComboBox combo;
};

class ScaleSelector final : public juce::Component
{
public:
    ScaleSelector()
    {
        addAndMakeVisible (combo);
        combo.addItemList ({ "Chromatic", "Major", "Minor", "Pentatonic", "Pent Minor", "Dorian", "Blues" }, 1);
        combo.setJustificationType (juce::Justification::centred);
        combo.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::ComboBox& getComboBox() { return combo; }

    void paint (juce::Graphics& g) override
    {
        g.setColour (FanTune::Colour::TextDim.withAlpha (0.35f));
        g.setFont (labelFont (8.0f, false));
        g.drawText ("SCALE", getLocalBounds().removeFromTop (10), juce::Justification::centredLeft);
    }

    void resized() override
    {
        combo.setBounds (0, 12, getWidth(), getHeight() - 12);
    }

private:
    juce::ComboBox combo;
};

class BypassLedButton final : public juce::Button
{
public:
    BypassLedButton() : juce::Button ("bypass")
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        const float active = getToggleState() ? 1.0f : 0.0f;

        if (active > 0.5f || highlighted)
        {
            g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.12f + active * 0.2f));
            g.fillEllipse (b.expanded (4.0f));
        }

        juce::ColourGradient body (juce::Colour (0xFF1E2A2E), b.getCentreX() - 4, b.getCentreY() - 6,
                                  juce::Colour (0xFF0A1012), b.getCentreX(), b.getCentreY(), true);
        g.setGradientFill (body);
        g.fillEllipse (b);

        const auto border = juce::Colour::fromFloatRGBA (0.0f, 0.86f + active * 0.14f, 1.0f, 0.3f + active * 0.7f);
        g.setColour (border);
        g.drawEllipse (b, 2.0f);

        if (active > 0.5f || highlighted || down)
        {
            g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.15f + active * 0.25f));
            g.fillEllipse (b.reduced (3.0f));
        }

        const float dotR = 4.0f;
        if (active > 0.5f)
        {
            g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.35f));
            g.fillEllipse (b.getCentreX() - dotR * 2.0f, b.getCentreY() - dotR * 2.0f, dotR * 4.0f, dotR * 4.0f);
        }
        g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.4f + active * 0.6f));
        g.fillEllipse (b.getCentreX() - dotR, b.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);
    }
};

class PresetBar final : public juce::Component
{
public:
    std::function<void(int)> onPresetChange;
    std::function<void()> onLoadFromFile;
    std::function<void()> onRefreshPresets;

    PresetBar()
    {
        setNames ({ "Bedroom Box Fan", "T-Pain Breeze", "Subtle Draft", "Box Fan High", "Hurricane Box Fan" });
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setNames (const juce::StringArray& n)
    {
        names = n;
        index = juce::jlimit (0, juce::jmax (0, names.size() - 1), index);
        repaint();
    }

    void setIndex (int i) { index = juce::jlimit (0, juce::jmax (0, names.size() - 1), i); repaint(); }
    int getIndex() const { return index; }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        auto pill = area.withTrimmedLeft (26.0f).withTrimmedRight (26.0f);

        g.setColour (juce::Colour (0x59000000));
        g.fillRoundedRectangle (pill, 4.0f);
        g.setColour (FanTune::Colour::CyanFaint);
        g.drawRoundedRectangle (pill.reduced (0.5f), 4.0f, 1.0f);

        g.setColour (FanTune::Colour::TextPrimary);
        g.setFont (labelFont (11.0f));
        g.drawText (names[index], pill, juce::Justification::centred);

        auto drawArrow = [&] (juce::Rectangle<float> r, bool prev, bool hot)
        {
            const auto bg = hot ? juce::Colour (0x2600DCFF) : juce::Colour (0x0D00DCFF);
            const auto border = hot ? FanTune::Colour::CyanPrimary.withAlpha (0.5f) : FanTune::Colour::CyanDim;
            g.setColour (bg);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (border);
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
            g.setColour (FanTune::Colour::CyanPrimary.withAlpha (hot ? 1.0f : 0.7f));
            g.setFont (labelFont (11.0f));
            g.drawText (prev ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb9"))
                             : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xba")),
                        r, juce::Justification::centred);
        };

        const bool hoverL = hoverZone == 0;
        const bool hoverR = hoverZone == 1;
        drawArrow (area.removeFromLeft (22.0f).reduced (0.0f, 2.0f), true, hoverL);
        drawArrow (area.removeFromRight (22.0f).reduced (0.0f, 2.0f), false, hoverR);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int zone = e.x < 22 ? 0 : (e.x > getWidth() - 22 ? 1 : -1);
        if (zone != hoverZone) { hoverZone = zone; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverZone != -1) { hoverZone = -1; repaint(); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || e.mods.isRightButtonDown())
        {
            showContextMenu();
            return;
        }

        if (e.x < 22) changePreset (-1);
        else if (e.x > getWidth() - 22) changePreset (1);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.x >= 22 && e.x <= getWidth() - 22 && onLoadFromFile)
            onLoadFromFile();
    }

private:
    void showContextMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Load preset from file...");
        menu.addItem (2, "Refresh user presets");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this] (int result)
                            {
                                if (result == 1 && onLoadFromFile)
                                    onLoadFromFile();
                                else if (result == 2 && onRefreshPresets)
                                    onRefreshPresets();
                            });
    }
    void changePreset (int delta)
    {
        if (names.isEmpty()) return;
        index = (index + delta + names.size()) % names.size();
        repaint();
        if (onPresetChange) onPresetChange (index);
    }

    juce::StringArray names;
    int index = 0;
    int hoverZone = -1;
};

class LevelMeter final : public juce::Component
{
public:
    void setLevels (float peak, float rms) noexcept
    {
        level = juce::jlimit (0.0f, 1.0f, peak);
        rmsLevel = juce::jlimit (0.0f, 1.0f, rms * 2.2f);
        peakHold = juce::jmax (peakHold * 0.96f, level);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.3f));
        g.fillRoundedRectangle (b.removeFromTop (2.0f), 1.0f);

        auto bar = b.reduced (0.0f, 1.0f);
        g.setColour (juce::Colour (0x80000000));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (FanTune::Colour::CyanFaint.withAlpha (0.35f));
        g.drawRoundedRectangle (bar.reduced (0.5f), 2.0f, 1.0f);

        auto rmsFill = bar;
        rmsFill.setHeight (bar.getHeight() * rmsLevel);
        rmsFill.setY (bar.getBottom() - rmsFill.getHeight());
        g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.22f));
        g.fillRoundedRectangle (rmsFill, 2.0f);

        auto fill = bar;
        fill.setHeight (bar.getHeight() * level);
        fill.setY (bar.getBottom() - fill.getHeight());

        juce::ColourGradient grad (FanTune::Colour::CyanPrimary, fill.getX(), fill.getBottom(),
                                   FanTune::Colour::GreenOK, fill.getX(), fill.getY() + fill.getHeight() * 0.4f, false);
        grad.addColour (0.6, FanTune::Colour::GreenOK);
        grad.addColour (1.0, level > 0.88f ? FanTune::Colour::RedClip : FanTune::Colour::GreenOK);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);

        if (peakHold > 0.02f)
        {
            const float y = bar.getBottom() - bar.getHeight() * peakHold;
            g.setColour (peakHold > 0.9f ? FanTune::Colour::RedClip : FanTune::Colour::CyanPrimary);
            g.fillRect (bar.getX(), y - 1.0f, bar.getWidth(), 2.0f);
        }
    }

private:
    float level = 0.0f;
    float rmsLevel = 0.0f;
    float peakHold = 0.0f;
};

class MidiLearnButton final : public juce::TextButton
{
public:
    MidiLearnButton()
    {
        setButtonText ("Learn");
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setLearning (bool learning)
    {
        if (learning != isLearning)
        {
            isLearning = learning;
            setToggleState (learning, juce::dontSendNotification);
            repaint();
        }
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        const auto active = isLearning || getToggleState();
        const auto bg = active ? juce::Colour (0x4400DCFF) : juce::Colour (0x1A00DCFF);
        g.setColour (bg);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (active ? FanTune::Colour::CyanPrimary : FanTune::Colour::CyanDim);
        g.drawRoundedRectangle (b, 4.0f, 1.0f);
        g.setColour (active ? FanTune::Colour::TextPrimary : FanTune::Colour::TextDim);
        g.setFont (labelFont (8.0f, false));
        g.drawText (active ? "LEARN" : "Learn", b, juce::Justification::centred);
        juce::ignoreUnused (highlighted, down);
    }

private:
    bool isLearning = false;
};

class PitchDisplay final : public juce::Component
{
public:
    void setState (const juce::String& note, float cents, float confidence, float frequencyHz, bool tracking) noexcept
    {
        noteName = note;
        centsOffset = cents;
        conf = juce::jlimit (0.0f, 1.0f, confidence);
        frequency = frequencyHz;
        isTracking = tracking;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        g.setColour (juce::Colour (0x59000000));
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (FanTune::Colour::CyanFaint.withAlpha (0.45f));
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

        g.setColour (isTracking ? FanTune::Colour::CyanPrimary : FanTune::Colour::TextDim);
        g.setFont (labelFont (16.0f));
        g.drawText (noteName, b.removeFromTop (b.getHeight() * 0.52f), juce::Justification::centred);

        juce::String centsText = "---";
        if (isTracking)
        {
            const int rounded = (int) std::round (centsOffset);
            centsText = (rounded >= 0 ? "+" : "") + juce::String (rounded) + " ct";
        }

        g.setColour (FanTune::Colour::TextPrimary.withAlpha (0.85f));
        g.setFont (labelFont (8.0f, false));
        g.drawText (centsText, b.removeFromTop (b.getHeight() * 0.30f), juce::Justification::centred);

        juce::String hzText = "---";
        if (isTracking && frequency > 20.0f)
            hzText = juce::String (frequency, frequency > 999.0f ? 0 : 1) + " Hz";

        g.setColour (FanTune::Colour::TextDim.withAlpha (0.75f));
        g.setFont (labelFont (7.0f, false));
        g.drawText (hzText, b.removeFromTop (b.getHeight() * 0.45f), juce::Justification::centred);

        auto bar = b.reduced (4.0f, 1.0f);
        g.setColour (juce::Colour (0x66000000));
        g.fillRoundedRectangle (bar, 2.0f);

        auto fill = bar;
        fill.setWidth (bar.getWidth() * conf);
        juce::ColourGradient grad (FanTune::Colour::CyanPrimary.withAlpha (0.35f), fill.getX(), fill.getCentreY(),
                                     FanTune::Colour::CyanPrimary, fill.getRight(), fill.getCentreY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);

        g.setColour (FanTune::Colour::TextDim.withAlpha (0.55f));
        g.setFont (labelFont (7.0f, false));
        g.drawText (juce::String (juce::roundToInt (conf * 100.0f)) + "%",
                    bar.withY (bar.getY() - 1.0f), juce::Justification::centred);
    }

private:
    juce::String noteName { "---" };
    float centsOffset = 0.0f;
    float conf = 0.0f;
    float frequency = 0.0f;
    bool isTracking = false;
};

class CpuMeter final : public juce::Component
{
public:
    void setLoad (float l) noexcept { load = juce::jlimit (0.0f, 1.0f, l); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0x66000000));
        g.fillRoundedRectangle (b.withTrimmedTop (3.0f).withHeight (10.0f), 2.0f);

        auto fill = b.withTrimmedTop (4.0f).withTrimmedLeft (1.0f).withHeight (8.0f);
        fill.setWidth (juce::jmax (2.0f, fill.getWidth() * load));

        juce::ColourGradient grad (FanTune::Colour::CyanPrimary, fill.getX(), fill.getCentreY(),
                                   FanTune::Colour::GreenOK, fill.getRight() * 0.7f, fill.getCentreY(), false);
        grad.addColour (1.0, FanTune::Colour::RedClip);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 1.5f);

        g.setColour (FanTune::Colour::TextDim.withAlpha (0.6f));
        g.setFont (labelFont (7.0f, false));
        g.drawText (juce::String (juce::roundToInt (load * 100.0f)) + "%",
                    getLocalBounds().removeFromRight (22), juce::Justification::centredRight);
    }

private:
    float load = 0.18f;
};

class FanGuardAssembly final : public juce::Component,
                               private juce::Timer
{
public:
    FanGuardAssembly()
    {
        startTimerHz (60);
    }

    void setRotationHz (float hz) noexcept { rotationHz = hz; }
    void setGlowPhase (float p) noexcept { glowPhase = p; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float housingR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
        const float outerR = housingR - 14.0f;

        paintBoxHousing (g, cx, cy, housingR);

        const float glowAlpha = 0.2f + 0.15f * (0.5f + 0.5f * std::sin (glowPhase));
        g.setColour (FanTune::Colour::CyanPrimary.withAlpha (glowAlpha * 0.4f));
        g.drawEllipse (cx - outerR - 10.0f, cy - outerR - 10.0f, (outerR + 10.0f) * 2.0f, (outerR + 10.0f) * 2.0f, 2.0f);

        juce::ColourGradient guardBg (juce::Colours::transparentBlack, cx, cy,
                                      juce::Colour (0xCC000000), cx + outerR, cy, true);
        g.setGradientFill (guardBg);
        g.fillEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);

        paintWireMesh (g, cx, cy, outerR);

        const float ringSizes[] { 0.86f, 0.71f, 0.57f, 0.43f, 0.29f };
        for (float rs : ringSizes)
        {
            const float r = outerR * rs;
            g.setColour (FanTune::Colour::CyanFaint.withAlpha (0.5f));
            g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        }

        g.setColour (juce::Colour (0x4000DCFF));
        g.drawEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (cx - outerR - 4.0f, cy - outerR - 4.0f, (outerR + 4.0f) * 2.0f, (outerR + 4.0f) * 2.0f, 4.0f);

        paintBlades (g, cx, cy, outerR * 0.88f);

        const float hubPulse = 0.4f + 0.3f * (0.5f + 0.5f * std::sin (glowPhase));
        const float hubR = 20.0f;
        juce::ColourGradient hub (juce::Colour (0xFF2A3040), cx - 6.0f, cy - 8.0f,
                                  juce::Colour (0xFF0D0F18), cx, cy, true);
        g.setGradientFill (hub);
        g.fillEllipse (cx - hubR, cy - hubR, hubR * 2.0f, hubR * 2.0f);
        g.setColour (FanTune::Colour::CyanPrimary.withAlpha (hubPulse));
        g.drawEllipse (cx - hubR, cy - hubR, hubR * 2.0f, hubR * 2.0f, 2.0f);
        g.setColour (FanTune::Colour::CyanPrimary);
        g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
    }

private:
    static void paintBoxHousing (juce::Graphics& g, float cx, float cy, float half)
    {
        const auto housing = juce::Rectangle<float> (cx - half, cy - half, half * 2.0f, half * 2.0f);

        juce::ColourGradient plastic (juce::Colour (0xFF2A2E3C), housing.getX(), housing.getY(),
                                      juce::Colour (0xFF12141C), housing.getRight(), housing.getBottom(), false);
        plastic.addColour (0.45, juce::Colour (0xFF1A1E28));
        g.setGradientFill (plastic);
        g.fillRoundedRectangle (housing, 10.0f);

        g.setColour (juce::Colour (0xFF3A4050));
        g.drawRoundedRectangle (housing.reduced (1.0f), 9.0f, 1.5f);
        g.setColour (juce::Colour (0x66000000));
        g.drawRoundedRectangle (housing.reduced (4.0f), 8.0f, 1.0f);

        const float ventW = housing.getWidth() * 0.72f;
        const float ventH = 6.0f;
        const float ventX = cx - ventW * 0.5f;
        for (int row = 0; row < 3; ++row)
        {
            const float yTop = housing.getY() + 10.0f + (float) row * 8.0f;
            const float yBot = housing.getBottom() - 18.0f - (float) row * 8.0f;
            for (float y : { yTop, yBot })
            {
                g.setColour (juce::Colour (0x44000000));
                g.fillRoundedRectangle (ventX, y, ventW, ventH, 2.0f);
                g.setColour (juce::Colour (0x2200DCFF));
                g.drawRoundedRectangle (ventX, y, ventW, ventH, 2.0f, 0.5f);
            }
        }

        const float screwR = 3.5f;
        const float inset = 14.0f;
        const juce::Point<float> corners[] {
            { housing.getX() + inset,  housing.getY() + inset },
            { housing.getRight() - inset, housing.getY() + inset },
            { housing.getX() + inset,  housing.getBottom() - inset },
            { housing.getRight() - inset, housing.getBottom() - inset }
        };
        for (auto p : corners)
        {
            juce::ColourGradient screw (juce::Colour (0xFF4A5060), p.x - 1, p.y - 1,
                                        juce::Colour (0xFF181C24), p.x, p.y, true);
            g.setGradientFill (screw);
            g.fillEllipse (p.x - screwR, p.y - screwR, screwR * 2.0f, screwR * 2.0f);
            g.setColour (juce::Colour (0xFF606878));
            g.drawEllipse (p.x - screwR, p.y - screwR, screwR * 2.0f, screwR * 2.0f, 0.8f);
            g.setColour (juce::Colour (0xFF9098A8));
            g.drawLine (p.x - 1.5f, p.y, p.x + 1.5f, p.y, 0.6f);
        }
    }

    static void paintWireMesh (juce::Graphics& g, float cx, float cy, float outerR)
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (juce::Rectangle<int> ((int) (cx - outerR), (int) (cy - outerR),
                                                (int) (outerR * 2.0f), (int) (outerR * 2.0f)));

        for (int deg = 0; deg < 360; deg += 6)
        {
            const float a0 = juce::degreesToRadians ((float) deg);
            const float a1 = juce::degreesToRadians ((float) deg + 6.0f);
            juce::Path wedge;
            wedge.addPieSegment (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f,
                                 a0, a1, 0.0);
            g.setColour ((deg / 6) % 2 == 0 ? juce::Colour (0x99607090) : juce::Colour (0x66303550));
            g.fillPath (wedge);
        }

        for (int i = - (int) outerR; i <= (int) outerR; i += 7)
        {
            g.setColour (juce::Colour (0x2800DCFF));
            g.drawLine (cx + (float) i, cy - outerR, cx + (float) i, cy + outerR, 0.6f);
            g.drawLine (cx - outerR, cy + (float) i, cx + outerR, cy + (float) i, 0.6f);
        }

        for (int deg = 0; deg < 360; deg += 18)
        {
            const float a = juce::degreesToRadians ((float) deg);
            g.setColour (juce::Colour (0x3500DCFF));
            g.drawLine (cx, cy,
                        cx + std::cos (a) * outerR,
                        cy + std::sin (a) * outerR, 0.5f);
        }
    }

    void paintBlades (juce::Graphics& g, float cx, float cy, float radius)
    {
        juce::Graphics::ScopedSaveState rot (g);
        g.addTransform (juce::AffineTransform::rotation (bladeAngle, cx, cy));

        for (int i = 0; i < kBoxFanBlades; ++i)
        {
            juce::Path blade;
            blade.startNewSubPath (cx, cy);
            blade.cubicTo (cx - 12.0f, cy - 30.0f, cx + 8.0f, cy - radius * 0.92f, cx + 20.0f, cy - radius);
            blade.cubicTo (cx + 32.0f, cy - radius * 0.88f, cx + 28.0f, cy - 40.0f, cx, cy);
            blade.closeSubPath();

            juce::ColourGradient grad (juce::Colour (0xF2121624), cx, cy,
                                       juce::Colour (0x1400B4DC), cx, cy - radius, false);
            grad.addColour (0.5, juce::Colour (0xD91C2234));
            g.setGradientFill (grad);
            g.fillPath (blade);
            g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.22f));
            g.strokePath (blade, juce::PathStrokeType (1.0f));

            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::twoPi / (float) kBoxFanBlades, cx, cy));
        }
    }

    void timerCallback() override
    {
        bladeAngle += juce::MathConstants<float>::twoPi * rotationHz / 60.0f;
        if (bladeAngle > juce::MathConstants<float>::twoPi)
            bladeAngle -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    float bladeAngle = 0.0f;
    float rotationHz = 12.0f;
    float glowPhase = 0.0f;
};

class KnobSection final : public juce::Component
{
public:
    KnobSection (const juce::String& name, int knobSize)
        : labelName (name), size (knobSize)
    {
        addAndMakeVisible (slider);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        addAndMakeVisible (valueLabel);
        valueLabel.setInterceptsMouseClicks (false, false);
        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setColour (juce::Label::textColourId, FanTune::Colour::TextDim.withAlpha (0.75f));
        valueLabel.setFont (labelFont (8.0f, false));
    }

    juce::Slider& getSlider() { return slider; }

    void setValueText (const juce::String& t)
    {
        valueLabel.setText (t, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        if (slider.isMouseOverOrDragging())
        {
            auto knobArea = getLocalBounds().withSizeKeepingCentre (size + 8, size + 8);
            g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.06f));
            g.fillEllipse (knobArea.toFloat());
        }

        g.setColour (FanTune::Colour::CyanPrimary.withAlpha (0.55f));
        g.setFont (labelFont (8.5f));
        g.drawText (labelName, getLocalBounds().removeFromTop (14), juce::Justification::centred);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop (14);
        valueLabel.setBounds (b.removeFromBottom (14));
        slider.setBounds (b.withSizeKeepingCentre (size, size));
    }

private:
    juce::String labelName;
    int size;
    juce::Slider slider;
    juce::Label valueLabel;
};

} // namespace FanTune::UI