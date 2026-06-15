// ═══════════════════════════════════════════════════════════════════
//  FanTuneLookAndFeel.h  —  Skeuomorphic JUCE LookAndFeel for FanTune
//  Target: JUCE 7 / VST3 / AU
//  Design Language: Brushed Dark Steel + Glowing Cyan + Wire Mesh Guard
// ═══════════════════════════════════════════════════════════════════
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace FanTune {

// ─── Brand colours ─────────────────────────────────────────────────
namespace Colour {
  static const juce::Colour Chassis        { 0xFF111118 };  // brushed steel base
  static const juce::Colour ChassisLight   { 0xFF1C1C28 };  // highlight on steel
  static const juce::Colour ChassisDark    { 0xFF0A0A12 };  // deep shadow
  static const juce::Colour CyanPrimary    { 0xFF00DCFF };  // main cyan
  static const juce::Colour CyanDim        { 0x5500DCFF };  // 33% cyan
  static const juce::Colour CyanFaint      { 0x1A00DCFF };  // 10% cyan
  static const juce::Colour KnobBody       { 0xFF1A1C28 };  // dark knob face
  static const juce::Colour KnobHighlight  { 0xFF3A3E52 };  // top-left highlight
  static const juce::Colour TextPrimary    { 0xFFE0F8FF };  // label text
  static const juce::Colour TextDim        { 0x88C0F0FF };  // muted text
  static const juce::Colour GreenOK        { 0xFF00FFAA };  // meter green
  static const juce::Colour RedClip        { 0xFFFF3366 };  // meter clip
}

// ═══════════════════════════════════════════════════════════════════
class FanTuneLookAndFeel  :  public juce::LookAndFeel_V4
{
public:
    FanTuneLookAndFeel()
    {
        // Global colour overrides fed into JUCE's colour-ID system
        setColour (juce::ResizableWindow::backgroundColourId,  Colour::Chassis);
        setColour (juce::Slider::thumbColourId,                Colour::CyanPrimary);
        setColour (juce::Slider::trackColourId,                Colour::CyanDim);
        setColour (juce::Slider::backgroundColourId,           Colour::ChassisDark);
        setColour (juce::Label::textColourId,                  Colour::TextPrimary);
        setColour (juce::ComboBox::backgroundColourId,         Colour::ChassisDark);
        setColour (juce::ComboBox::outlineColourId,            Colour::CyanDim);
        setColour (juce::ComboBox::textColourId,               Colour::TextPrimary);
        setColour (juce::TextButton::buttonColourId,           Colour::ChassisDark);
        setColour (juce::TextButton::textColourOffId,          Colour::TextDim);
        setColour (juce::TextButton::textColourOnId,           Colour::CyanPrimary);
        setColour (juce::PopupMenu::backgroundColourId,        Colour::ChassisLight);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour::CyanFaint);
        setColour (juce::PopupMenu::textColourId,              Colour::TextPrimary);
    }

    // ─────────────────────────────────────────────────────────────
    //  ROTARY KNOB (the hero control)
    // ─────────────────────────────────────────────────────────────
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const float cx = x + width  * 0.5f;
        const float cy = y + height * 0.5f;
        const float outerR = juce::jmin(width, height) * 0.5f - 3.0f;
        const float innerR = outerR - 6.0f;   // body radius

        const float currentAngle = rotaryStartAngle
                                 + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // ── 1. Drop shadow behind entire knob
        {
            juce::ColourGradient shadow (juce::Colours::transparentBlack, cx, cy,
                                         juce::Colour(0xAA000000),        cx, cy + outerR + 2.0f, true);
            g.setGradientFill (shadow);
            g.fillEllipse (cx - outerR - 2, cy - outerR - 2,
                           (outerR + 2)*2.f, (outerR + 2)*2.f);
        }

        // ── 2. Track groove (full sweep, dark)
        {
            juce::Path track;
            track.addCentredArc (cx, cy, outerR - 2, outerR - 2,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour (juce::Colour(0xFF050508));
            g.strokePath (track, juce::PathStrokeType (5.0f,
                          juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded));
        }

        // ── 3. Cyan value arc (filled portion + glow)
        {
            juce::Path arc;
            arc.addCentredArc (cx, cy, outerR - 2, outerR - 2,
                               0.0f, rotaryStartAngle, currentAngle, true);

            // base arc
            juce::ColourGradient arcGrad (juce::Colour(0xCC0096C0), cx - outerR, cy,
                                          Colour::CyanPrimary,       cx + outerR, cy, false);
            g.setGradientFill (arcGrad);
            g.strokePath (arc, juce::PathStrokeType (4.0f,
                          juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded));

            // glow pass — render shorter arc near tip
            float glowStart = juce::jmax(rotaryStartAngle, currentAngle - 0.35f);
            juce::Path glowArc;
            glowArc.addCentredArc (cx, cy, outerR - 2, outerR - 2,
                                   0.0f, glowStart, currentAngle, true);
            g.saveState();
            // JUCE doesn't have native blur; approximate with a second pass at wider stroke + alpha
            g.setColour (Colour::CyanPrimary.withAlpha(0.45f));
            g.strokePath (glowArc, juce::PathStrokeType (8.0f,
                          juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded));
            g.restoreState();
        }

        // ── 4. Knob body — radial gradient (brushed-steel sphere illusion)
        {
            juce::ColourGradient body (Colour::KnobHighlight,
                                       cx - innerR * 0.22f, cy - innerR * 0.28f,
                                       Colour::ChassisDark,
                                       cx, cy, true);
            body.addColour (0.40, Colour::KnobBody);
            body.addColour (0.80, juce::Colour(0xFF0C0E16));
            g.setGradientFill (body);
            g.fillEllipse (cx - innerR, cy - innerR, innerR*2.f, innerR*2.f);
        }

        // ── 5. Brushed-steel radial lines (simulated with thin strokes)
        {
            g.saveState();
            juce::Rectangle<float> clipRect (cx-innerR, cy-innerR, innerR*2.f, innerR*2.f);
            g.reduceClipRegion (clipRect.toNearestInt());

            for (int i = 0; i < 36; ++i)
            {
                const float a = (i / 36.0f) * juce::MathConstants<float>::twoPi;
                const float ex = cx + std::cos(a) * innerR;
                const float ey = cy + std::sin(a) * innerR;
                g.setColour (i % 2 == 0
                    ? juce::Colours::white.withAlpha(0.025f)
                    : juce::Colours::black.withAlpha(0.030f));
                g.drawLine (cx, cy, ex, ey, 0.8f);
            }
            g.restoreState();
        }

        // ── 6. Top-left specular highlight arc
        {
            juce::Path hl;
            hl.addCentredArc (cx, cy, innerR - 1.5f, innerR - 1.5f,
                              0.0f,
                              juce::MathConstants<float>::pi * 1.05f,
                              juce::MathConstants<float>::pi * 1.95f, true);
            juce::ColourGradient hlGrad (juce::Colours::white.withAlpha(0.2f),
                                          cx - innerR * 0.6f, cy - innerR * 0.8f,
                                          juce::Colours::transparentWhite,
                                          cx + innerR * 0.3f, cy - innerR * 0.1f, false);
            g.setGradientFill (hlGrad);
            g.strokePath (hl, juce::PathStrokeType (3.0f));
        }

        // ── 7. Knob rim border
        {
            g.setColour (Colour::CyanDim);
            g.drawEllipse (cx - innerR, cy - innerR, innerR*2.f, innerR*2.f, 1.5f);
        }

        // ── 8. Pointer dot at tip
        {
            const float dotR   = juce::jmax (2.5f, innerR * 0.12f);
            const float dotDist = innerR - dotR * 2.2f;
            const float dotX   = cx + dotDist * std::cos (currentAngle);
            const float dotY   = cy + dotDist * std::sin (currentAngle);
            const bool hot = slider.isMouseOverOrDragging();

            g.setColour (Colour::CyanPrimary.withAlpha (hot ? 0.55f : 0.4f));
            g.fillEllipse (dotX - dotR * 2.5f, dotY - dotR * 2.5f, dotR * 5.0f, dotR * 5.0f);

            g.setColour (hot ? juce::Colours::white : Colour::CyanPrimary);
            g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }

    // ─────────────────────────────────────────────────────────────
    //  LINEAR SLIDER (used for I/O faders in footer)
    // ─────────────────────────────────────────────────────────────
    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                           juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            const float trackW  = 4.0f;
            const float trackX  = x + (width - trackW) * 0.5f;
            // track background
            g.setColour (Colour::ChassisDark);
            g.fillRoundedRectangle (trackX, (float)y, trackW, (float)height, 2.0f);

            // filled portion
            juce::ColourGradient fill (Colour::CyanPrimary, trackX, sliderPos,
                                       juce::Colour(0xFF0060A0), trackX, (float)(y+height), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (trackX, sliderPos, trackW, (float)(y+height)-sliderPos, 2.0f);

            // thumb
            const float thumbH = 14.0f, thumbW = 26.0f;
            g.setColour (Colour::KnobBody);
            g.fillRoundedRectangle (x + (width-thumbW)*0.5f, sliderPos - thumbH*0.5f,
                                    thumbW, thumbH, 3.0f);
            g.setColour (Colour::CyanDim);
            g.drawRoundedRectangle (x + (width-thumbW)*0.5f, sliderPos - thumbH*0.5f,
                                    thumbW, thumbH, 3.0f, 1.0f);
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                              sliderPos, 0, 0, style, slider);
        }
    }

    // ─────────────────────────────────────────────────────────────
    //  TEXT BUTTON (preset prev/next, header buttons)
    // ─────────────────────────────────────────────────────────────
    void drawButtonBackground (juce::Graphics& g,
                                juce::Button& button,
                                const juce::Colour& /*backgroundColour*/,
                                bool isHighlighted,
                                bool isDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = 4.0f;

        juce::Colour bg = isDown        ? Colour::CyanDim
                        : isHighlighted ? Colour::CyanFaint
                                        : juce::Colour(0xFF0A0A16);
        g.setColour (bg);
        g.fillRoundedRectangle (bounds, corner);

        juce::Colour border = (isHighlighted || isDown)
                            ? Colour::CyanPrimary
                            : Colour::CyanDim;
        g.setColour (border);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool isHighlighted,
                         bool /*isDown*/) override
    {
        g.setColour (button.getToggleState() ? Colour::CyanPrimary
                    : isHighlighted ? Colour::TextPrimary.withAlpha (0.85f)
                                    : Colour::TextDim);
        g.setFont (juce::Font (juce::FontOptions().withName ("Segoe UI")
                                              .withHeight (9.0f)
                                              .withStyle ("Regular")));
        g.drawFittedText (button.getButtonText().toUpperCase(),
                          button.getLocalBounds(), juce::Justification::centred, 1);
    }

    // ─────────────────────────────────────────────────────────────
    //  COMBO BOX (preset selector)
    // ─────────────────────────────────────────────────────────────
    void drawComboBox (juce::Graphics& g,
                       int width, int height,
                       bool isButtonDown,
                       int, int, int, int,
                       juce::ComboBox& box) override
    {
        const juce::Rectangle<float> bounds (0, 0, (float) width, (float) height);
        const bool hot = box.isMouseOver() || isButtonDown;

        juce::ColourGradient bg (juce::Colour (0xFF141420), bounds.getX(), bounds.getY(),
                                 juce::Colour (0xFF0A0A12), bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (hot ? Colour::CyanPrimary.withAlpha (0.35f) : Colour::CyanDim);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

        const float arrowX = (float) width - 14.0f;
        juce::Path arrow;
        arrow.startNewSubPath (arrowX,      height * 0.42f);
        arrow.lineTo          (arrowX + 4.0f, height * 0.58f);
        arrow.lineTo          (arrowX + 8.0f, height * 0.42f);
        g.setColour (hot ? Colour::CyanPrimary : Colour::CyanDim);
        g.strokePath (arrow, juce::PathStrokeType (1.5f,
                      juce::PathStrokeType::mitered,
                      juce::PathStrokeType::square));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions().withName ("Segoe UI")
                                          .withHeight (10.0f)
                                          .withStyle ("Bold"));
    }

    // ─────────────────────────────────────────────────────────────
    //  POPUP MENU
    // ─────────────────────────────────────────────────────────────
    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.setColour (Colour::ChassisLight);
        g.fillRoundedRectangle (0,0,(float)width,(float)height, 6.0f);
        g.setColour (Colour::CyanFaint);
        g.drawRoundedRectangle (0.5f,0.5f,(float)width-1,(float)height-1, 6.0f, 1.0f);
    }

    void drawPopupMenuItem (juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked,
                            bool /*hasSubMenu*/,
                            const juce::String& text,
                            const juce::String& /*shortcutKeyText*/,
                            const juce::Drawable* /*icon*/,
                            const juce::Colour* /*textColour*/) override
    {
        if (isSeparator) {
            g.setColour (Colour::CyanFaint);
            g.drawHorizontalLine (area.getCentreY(), (float)area.getX()+6,
                                  (float)area.getRight()-6);
            return;
        }
        if (isHighlighted) {
            g.setColour (Colour::CyanFaint);
            g.fillRoundedRectangle (area.reduced(2).toFloat(), 3.0f);
        }
        g.setColour (isActive ? Colour::TextPrimary : Colour::TextDim);
        g.setFont (juce::Font (juce::FontOptions().withName ("Segoe UI")
                                              .withHeight (11.0f)
                                              .withStyle ("Regular")));
        g.drawFittedText (text, area.withLeft(area.getX()+10),
                          juce::Justification::centredLeft, 1);
        if (isTicked) {
            g.setColour (Colour::CyanPrimary);
            g.fillEllipse ((float)area.getX()+3, area.getCentreY()-3.f, 6.f, 6.f);
        }
    }

    // ─────────────────────────────────────────────────────────────
    //  LABEL
    // ─────────────────────────────────────────────────────────────
    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions().withName ("Segoe UI")
                                          .withHeight (9.0f)
                                          .withStyle ("Bold"));
    }

    // ─────────────────────────────────────────────────────────────
    //  SCROLLBAR
    // ─────────────────────────────────────────────────────────────
    void drawScrollbar (juce::Graphics& g,
                        juce::ScrollBar&,
                        int x, int y, int width, int height,
                        bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override
    {
        g.setColour (Colour::ChassisDark);
        g.fillRect (x, y, width, height);
        g.setColour ((isMouseOver||isMouseDown) ? Colour::CyanPrimary : Colour::CyanDim);
        if (isScrollbarVertical)
            g.fillRoundedRectangle ((float)x+1,(float)y+thumbStartPosition,
                                    (float)width-2,(float)thumbSize, 2.f);
        else
            g.fillRoundedRectangle ((float)x+thumbStartPosition,(float)y+1,
                                    (float)thumbSize,(float)height-2, 2.f);
    }
};

} // namespace FanTune
