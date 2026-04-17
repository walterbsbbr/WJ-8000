#include "PluginEditor.h"
#include "MIDI/DW8000SysEx.h"
#include "DW8000ImageData.h"

// ─────────────────────────────────────────────────────────────────────────
// Layout constants (all in pixels)
// ─────────────────────────────────────────────────────────────────────────
//   CW  = cell width  (each parameter slot)
//   CH  = cell height (label + fader + value box)
//   HDR = header height (section title above box)
//   GAP = vertical gap between rows
//
//   Row 1 cells start at y = R1Y
//   Row 2 cells start at y = R2Y
//   Row 3 cells start at y = R3Y
//   Bottom bar at y = BARY

static constexpr int CW   = 54;
static constexpr int CH   = 148;
static constexpr int HDR  = 18;
static constexpr int GAP  = 14;

static constexpr int R1Y  = 28 + HDR;               // 46
static constexpr int R2Y  = R1Y + CH + GAP + HDR;   // 226
static constexpr int R3Y  = R2Y + CH + GAP + HDR;   // 406
static constexpr int KBY  = R3Y + CH + GAP;         // 568  keyboard top
static constexpr int KBH  = 120;                     // keyboard height (photo needs more room)
static constexpr int BARY = KBY + KBH + 8;          // 664  title/button bar

// ─────────────────────────────────────────────────────────────────────────
// MiniKeyboard
// ─────────────────────────────────────────────────────────────────────────
bool MiniKeyboard::isBlackKey(int note) noexcept
{
    int s = note % 12;
    return s==1 || s==3 || s==6 || s==8 || s==10;
}

// ── Calibration constants (fractions of component size, tuned to KEYS.png) ──
//   KBX0  / KBX1  = left / right edge of the key array inside the frame
//   KBY0  / KBY1  = top  / bottom edge of white keys
//   BKY1           = bottom edge of black keys
// Adjust these if the blue overlay doesn't align perfectly.
static constexpr float KBX0  = 0.013f;   // left  frame fraction
static constexpr float KBX1  = 0.987f;   // right frame fraction
static constexpr float KBY0  = 0.095f;   // top of keys
static constexpr float KBY1  = 0.910f;   // bottom of white keys
static constexpr float BKY1  = 0.095f + (0.910f - 0.095f) * 0.615f; // black key bottom

void MiniKeyboard::paint(juce::Graphics& g)
{
    // ── Load image (cached after first call) ─────────────────────────────
    static const juce::Image keysImg = juce::ImageCache::getFromMemory(
        DW8000ImageData::KEYS_png, DW8000ImageData::KEYS_pngSize);

    auto b = getLocalBounds().toFloat();

    // ── Draw the keyboard photo scaled to fill the component ─────────────
    g.drawImage(keysImg, b, juce::RectanglePlacement::stretchToFit);

    // ── Compute key geometry in component coordinates ─────────────────────
    float kx0 = b.getX() + KBX0 * b.getWidth();
    float kx1 = b.getX() + KBX1 * b.getWidth();
    float ky0 = b.getY() + KBY0 * b.getHeight();
    float ky1 = b.getY() + KBY1 * b.getHeight();
    float bky1= b.getY() + BKY1 * b.getHeight();

    float kw  = kx1 - kx0;
    float wkw = kw / (float)NUM_WHITE;
    float wkh = ky1 - ky0;
    float bkw = wkw * 0.56f;
    float bkh = bky1 - ky0;

    // Black key x-centres within an octave, in white-key units from C
    static const float kBkX[5]  = { 0.63f, 1.65f, 3.63f, 4.65f, 5.63f };
    static const int   kS2B[12] = { -1,0,-1,1,-1,-1,2,-1,3,-1,4,-1 };

    // ── Blue overlays on active white keys ───────────────────────────────
    int wi = 0;
    for (int note = FIRST_NOTE; note <= LAST_NOTE; ++note)
    {
        if (isBlackKey(note)) { continue; }

        if (proc_.isNoteActive(note))
        {
            float x = kx0 + wi * wkw;
            juce::Rectangle<float> kr(x + 1.0f, ky0, wkw - 2.0f, wkh);

            // Translucent blue wash preserving the photo texture underneath
            g.setColour(juce::Colour(0xFF3A7FFF).withAlpha(0.52f));
            g.fillRect(kr);

            // Brighter top band (key-press sheen)
            g.setColour(juce::Colour(0xFF88BBFF).withAlpha(0.38f));
            g.fillRect(kr.withHeight(wkh * 0.28f));
        }
        ++wi;
    }

    // ── Blue overlays on active black keys ───────────────────────────────
    for (int note = FIRST_NOTE; note <= LAST_NOTE; ++note)
    {
        if (!isBlackKey(note)) continue;

        if (proc_.isNoteActive(note))
        {
            int   rel  = note - FIRST_NOTE;
            int   oct  = rel / 12;
            int   semi = rel % 12;
            float cx   = kx0 + (oct * 7.0f + kBkX[kS2B[semi]]) * wkw;
            float bx   = cx - bkw * 0.5f;

            juce::Rectangle<float> kr(bx, ky0, bkw, bkh);

            g.setColour(juce::Colour(0xFF1A5FFF).withAlpha(0.75f));
            g.fillRect(kr);

            // Top highlight
            g.setColour(juce::Colour(0xFF66AAFF).withAlpha(0.45f));
            g.fillRect(kr.withHeight(bkh * 0.30f));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────
// PresetDisplay  — 2-digit red 7-segment LCD
// ─────────────────────────────────────────────────────────────────────────

// Segment bitmask for digits 1–8.
// Bit order:  a=0(top)  b=1(top-right)  c=2(bot-right)  d=3(bottom)
//             e=4(bot-left)  f=5(top-left)  g=6(middle)
static const uint8_t kSeg7[9] = {
    0,                                    // [0] unused
    0b0000110,  // 1 : b c
    0b1011011,  // 2 : a b d e g
    0b1001111,  // 3 : a b c d g
    0b1100110,  // 4 : b c f g
    0b1101101,  // 5 : a c d f g
    0b1111101,  // 6 : a c d e f g
    0b0000111,  // 7 : a b c
    0b1111111,  // 8 : all
};

void PresetDisplay::drawDigit(juce::Graphics& g, int d,
                              float x, float y, float w, float h,
                              juce::Colour lit, juce::Colour dim)
{
    if (d < 1 || d > 8) return;
    uint8_t segs = kSeg7[d];

    float t   = h * 0.11f;   // segment thickness
    float gap = 1.5f;         // gap between segments
    float hw  = w - 2.0f * (t + gap);
    float hh  = h * 0.5f - t - gap;

    // Segment rectangles: (left-x, top-y, width, height)
    juce::Rectangle<float> seg[7] = {
        { x + t + gap,   y,                   hw, t  },   // a  top
        { x + w - t,     y + t + gap,          t,  hh },   // b  top-right
        { x + w - t,     y + h*0.5f + gap,     t,  hh },   // c  bot-right
        { x + t + gap,   y + h - t,            hw, t  },   // d  bottom
        { x,             y + h*0.5f + gap,     t,  hh },   // e  bot-left
        { x,             y + t + gap,          t,  hh },   // f  top-left
        { x + t + gap,   y + h*0.5f - t*0.5f, hw, t  },   // g  middle
    };

    for (int i = 0; i < 7; ++i)
    {
        g.setColour((segs >> i) & 1 ? lit : dim);
        g.fillRoundedRectangle(seg[i], 1.5f);
    }
}

void PresetDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // ── LCD bezel ────────────────────────────────────────────────────────
    g.setColour(juce::Colour(0xFF080404));
    g.fillRoundedRectangle(b, 4.0f);
    g.setColour(juce::Colour(0xFF2A0808));
    g.drawRoundedRectangle(b.reduced(1.0f), 3.5f, 1.2f);

    // ── Screen area ──────────────────────────────────────────────────────
    auto screen = b.reduced(6.0f, 5.0f);
    g.setColour(juce::Colour(0xFF0D0202));
    g.fillRoundedRectangle(screen, 2.0f);

    // ── Compute current preset bank / program ────────────────────────────
    int idx  = proc_.getCurrentProgram();          // 0–63
    int bank = idx / 8 + 1;                        // 1–8
    int prog = idx % 8 + 1;                        // 1–8

    // ── Draw two digits side by side ─────────────────────────────────────
    juce::Colour litCol = juce::Colour(0xFFFF3300);   // bright red-orange
    juce::Colour dimCol = juce::Colour(0xFF380A0A);   // barely visible

    float dh  = screen.getHeight() * 0.72f;
    float dw  = dh * 0.55f;
    float sep = screen.getWidth() * 0.06f;
    float totalW = 2.0f * dw + sep;
    float dx0 = screen.getCentreX() - totalW * 0.5f;
    float dy0 = screen.getCentreY() - dh * 0.5f;

    drawDigit(g, bank, dx0,        dy0, dw, dh, litCol, dimCol);
    drawDigit(g, prog, dx0+dw+sep, dy0, dw, dh, litCol, dimCol);

    // ── Red LED glow ─────────────────────────────────────────────────────
    g.setColour(juce::Colour(0xFFFF2200).withAlpha(0.06f));
    g.fillRoundedRectangle(screen, 2.0f);
}

// ─────────────────────────────────────────────────────────────────────────
// DW8000LookAndFeel
// ─────────────────────────────────────────────────────────────────────────
DW8000LookAndFeel::DW8000LookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,          juce::Colour(0xFFCCCCCC));
    setColour(juce::Slider::textBoxOutlineColourId,       juce::Colour(0xFF333333));
    setColour(juce::Slider::textBoxBackgroundColourId,    juce::Colour(0xFF222222));
    setColour(juce::Label::textColourId,                  juce::Colour(0xFFDDAA66));
    setColour(juce::ResizableWindow::backgroundColourId,  juce::Colour(0xFF1A1A1A));
    setColour(juce::TextButton::buttonColourId,   juce::Colour(0xFF2A2A2A));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFE87D1E)); // lit = orange
    setColour(juce::TextButton::textColourOffId,  juce::Colour(0xFFAAAAAA));
    setColour(juce::TextButton::textColourOnId,   juce::Colour(0xFF111111)); // dark text on lit
}

void DW8000LookAndFeel::drawLinearSlider(juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float /*minPos*/, float /*maxPos*/,
    juce::Slider::SliderStyle /*style*/, juce::Slider& /*s*/)
{
    float cx = x + w * 0.5f;

    // ── Track ─────────────────────────────────────────────────────────
    float trackW = 4.0f;
    float trackX = cx - trackW * 0.5f;
    g.setColour(juce::Colour(0xFF3A3A3A));
    g.fillRoundedRectangle(trackX, (float)y, trackW, (float)h, 2.0f);

    // ── Fill (bottom of track up to thumb) ────────────────────────────
    float bottom = (float)(y + h);
    if (sliderPos < bottom)
    {
        g.setColour(juce::Colour(0xFFE87D1E).withAlpha(0.45f));
        g.fillRoundedRectangle(trackX, sliderPos, trackW, bottom - sliderPos, 2.0f);
    }

    // ── Thumb (fader cap) ─────────────────────────────────────────────
    const float thumbW = 30.0f;
    const float thumbH = 14.0f;
    float tx = cx - thumbW * 0.5f;
    float ty = sliderPos - thumbH * 0.5f;

    g.setColour(juce::Colour(0xFF2C2C2C));
    g.fillRoundedRectangle(tx, ty, thumbW, thumbH, 3.0f);
    g.setColour(juce::Colour(0xFFE87D1E));
    g.drawRoundedRectangle(tx, ty, thumbW, thumbH, 3.0f, 1.2f);
    // Centre grip line
    g.drawLine(tx + 5.0f, sliderPos, tx + thumbW - 5.0f, sliderPos, 1.5f);
}

void DW8000LookAndFeel::drawLabel(juce::Graphics& g, juce::Label& lbl)
{
    g.setColour(findColour(juce::Label::textColourId));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawFittedText(lbl.getText(), lbl.getLocalBounds(),
                     juce::Justification::centred, 1);
}

// ─────────────────────────────────────────────────────────────────────────
// Knob (vertical fader)
// ─────────────────────────────────────────────────────────────────────────
Knob::Knob(const juce::String& labelText,
           juce::AudioProcessorValueTreeState& apvts,
           const juce::String& paramID)
{
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 46, 16);
    addAndMakeVisible(slider);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    attachment = std::make_unique<SliderAttachment>(apvts, paramID, slider);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(14));
    slider.setBounds(area);
}

// ─────────────────────────────────────────────────────────────────────────
// Layout helpers (static)
// ─────────────────────────────────────────────────────────────────────────

// Place knobs in a horizontal row inside rect r, equal-width cells
void DW8000Editor::lay(juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
{
    int n = (int)knobs.size();
    if (n == 0) return;
    int cw = r.getWidth() / n;
    int x  = r.getX();
    for (auto* k : knobs)
    {
        k->setBounds(x, r.getY(), cw, r.getHeight());
        x += cw;
    }
}

// Draw a labelled section box (called from paint)
void DW8000Editor::sect(juce::Graphics& g, const char* title, juce::Rectangle<int> r)
{
    // Background fill
    g.setColour(juce::Colour(0xFF242424));
    g.fillRoundedRectangle(r.toFloat(), 4.0f);

    // Border
    g.setColour(juce::Colour(0xFF484848));
    g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.0f);

    // Title tab above box
    g.setColour(juce::Colour(0xFFE87D1E));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(title,
               r.getX(), r.getY() - HDR,
               r.getWidth(), HDR - 2,
               juce::Justification::centredLeft);
}

// ─────────────────────────────────────────────────────────────────────────
// DW8000Editor – constructor
// ─────────────────────────────────────────────────────────────────────────
DW8000Editor::DW8000Editor(DW8000Processor& p)
    : AudioProcessorEditor(&p), proc_(p),
      keyboard_(p),
      presetDisplay_(p),
      // ── OSC 1 ──────────────────────────────────────────────────────
      kOsc1Oct   ("OCT",       p.apvts, "p_00"),
      kOsc1Wave  ("WAVE",      p.apvts, "p_01"),
      kOsc1Lvl   ("LEVEL",     p.apvts, "p_02"),
      // ── Auto Bend ──────────────────────────────────────────────────
      kAutoBendSel ("SELECT",  p.apvts, "p_03"),
      kAutoBendMode("MODE",    p.apvts, "p_04"),
      kAutoBendTime("TIME",    p.apvts, "p_05"),
      kAutoBendInt ("INT",     p.apvts, "p_06"),
      // ── OSC 2 ──────────────────────────────────────────────────────
      kOsc2Oct   ("OCT",       p.apvts, "p_07"),
      kOsc2Wave  ("WAVE",      p.apvts, "p_08"),
      kOsc2Lvl   ("LEVEL",     p.apvts, "p_09"),
      kOsc2Int   ("INTV",      p.apvts, "p_0A"),
      kOsc2Det   ("DETUNE",    p.apvts, "p_0B"),
      // ── Noise ──────────────────────────────────────────────────────
      kNoise     ("LEVEL",     p.apvts, "p_0C"),
      // ── MG / LFO ───────────────────────────────────────────────────
      kMgWav     ("WAVE",      p.apvts, "p_22"),
      kMgFreq    ("FREQ",      p.apvts, "p_23"),
      kMgDly     ("DELAY",     p.apvts, "p_24"),
      kMgOsc     ("OSC",       p.apvts, "p_25"),
      kMgVcf     ("VCF",       p.apvts, "p_26"),
      // ── VCF ────────────────────────────────────────────────────────
      kVcfCut    ("CUTOFF",    p.apvts, "p_0F"),
      kVcfRes    ("RESO",      p.apvts, "p_10"),
      kVcfKbd    ("KBD TRK",   p.apvts, "p_11"),
      kVcfEGPol  ("EG POL",    p.apvts, "p_12"),
      kVcfEGInt  ("EG INT",    p.apvts, "p_13"),
      kVcfAtk    ("ATK",       p.apvts, "p_14"),
      kVcfDec    ("DEC",       p.apvts, "p_15"),
      kVcfBrk    ("BRK",       p.apvts, "p_16"),
      kVcfSlp    ("SLP",       p.apvts, "p_17"),
      kVcfSus    ("SUS",       p.apvts, "p_18"),
      kVcfRel    ("REL",       p.apvts, "p_19"),
      kVcfVel    ("VEL SNS",   p.apvts, "p_1A"),
      // ── VCA ────────────────────────────────────────────────────────
      kVcaAtk    ("ATK",       p.apvts, "p_1B"),
      kVcaDec    ("DEC",       p.apvts, "p_1C"),
      kVcaBrk    ("BRK",       p.apvts, "p_1D"),
      kVcaSlp    ("SLP",       p.apvts, "p_1E"),
      kVcaSus    ("SUS",       p.apvts, "p_1F"),
      kVcaRel    ("REL",       p.apvts, "p_20"),
      kVcaVel    ("VEL SNS",   p.apvts, "p_21"),
      // ── Voice / Misc ───────────────────────────────────────────────
      kPortoTime ("PORTO",     p.apvts, "p_2F"),
      // ── Bend ───────────────────────────────────────────────────────
      kBendRng   ("OSC RNG",   p.apvts, "p_27"),
      kBendVcf   ("VCF",       p.apvts, "p_28"),
      // ── Aftertouch ─────────────────────────────────────────────────
      kAtOsc     ("OSC MG",    p.apvts, "p_30"),
      kAtVcf     ("VCF",       p.apvts, "p_31"),
      kAtVca     ("VCA",       p.apvts, "p_32"),
      // ── Delay ──────────────────────────────────────────────────────
      kDlyTim    ("TIME",      p.apvts, "p_29"),
      kDlyFac    ("FACTOR",    p.apvts, "p_2A"),
      kDlyFbk    ("FDBK",      p.apvts, "p_2B"),
      kDlyMFq    ("MOD FREQ",  p.apvts, "p_2C"),
      kDlyMIn    ("MOD INT",   p.apvts, "p_2D"),
      kDlyEfx    ("LEVEL",     p.apvts, "p_2E"),
      // ── Arpeggiator ────────────────────────────────────────────────
      kArpSpeed  ("SPEED",     p.apvts, "arp_speed"),
      kArpClock  ("CLOCK",     p.apvts, "arp_clock")
{
    setLookAndFeel(&laf_);

    for (auto* k : {
            &kOsc1Oct,  &kOsc1Wave, &kOsc1Lvl,
            &kAutoBendSel, &kAutoBendMode, &kAutoBendTime, &kAutoBendInt,
            &kOsc2Oct,  &kOsc2Wave, &kOsc2Lvl, &kOsc2Int, &kOsc2Det,
            &kNoise,
            &kMgWav,    &kMgFreq,  &kMgDly,  &kMgOsc,  &kMgVcf,
            &kVcfCut,  &kVcfRes,  &kVcfKbd, &kVcfEGPol, &kVcfEGInt,
            &kVcfAtk,  &kVcfDec,  &kVcfBrk, &kVcfSlp,   &kVcfSus, &kVcfRel, &kVcfVel,
            &kVcaAtk,  &kVcaDec,  &kVcaBrk, &kVcaSlp,   &kVcaSus, &kVcaRel, &kVcaVel,
            &kPortoTime,
            &kBendRng, &kBendVcf,
            &kAtOsc,   &kAtVcf,   &kAtVca,
            &kDlyTim,  &kDlyFac,  &kDlyFbk, &kDlyMFq,   &kDlyMIn, &kDlyEfx,
            &kArpSpeed, &kArpClock })
        addAndMakeVisible(k);

    // ── Auto Bend SELECT: 0=OFF 1=OSC1 2=OSC2 3=BOTH ────────────────────
    static const char* kBendSelLabels[] = { "OFF", "OSC1", "OSC2", "BOTH" };
    kAutoBendSel.slider.textFromValueFunction = [](double v) -> juce::String {
        int i = juce::roundToInt(v);
        if (i < 0 || i > 3) i = 0;
        return kBendSelLabels[i];
    };
    kAutoBendSel.slider.valueFromTextFunction = [](const juce::String& t) -> double {
        auto s = t.trim();
        if (s == "OSC1") return 1.0;
        if (s == "OSC2") return 2.0;
        if (s == "BOTH") return 3.0;
        return 0.0;
    };

    // ── Auto Bend MODE: internal 0/1 → display 1(UP)/2(DN) ──────────────
    kAutoBendMode.slider.textFromValueFunction = [](double v) -> juce::String {
        return juce::roundToInt(v) == 0 ? "1 UP" : "2 DN";
    };
    kAutoBendMode.slider.valueFromTextFunction = [](const juce::String& t) -> double {
        return t.trim().startsWith("2") ? 1.0 : 0.0;
    };

    // ── EG Polarity display: internal 0/1 → label 1/2 ───────────────────
    kVcfEGPol.slider.textFromValueFunction = [](double v) -> juce::String {
        return juce::roundToInt(v) == 0 ? "1" : "2";
    };
    kVcfEGPol.slider.valueFromTextFunction = [](const juce::String& t) -> double {
        return t.trim() == "1" ? 0.0 : 1.0;
    };

    // ── OSC1 Octave: internal 0/1/2 → 16' / 8' / 4' ─────────────────────
    auto octText = [](double v) -> juce::String {
        switch (juce::roundToInt(v)) {
            case 0:  return "16'";
            case 2:  return "4'";
            default: return "8'";
        }
    };
    auto octVal = [](const juce::String& t) -> double {
        auto s = t.trim();
        if (s == "16'") return 0.0;
        if (s == "4'")  return 2.0;
        return 1.0;
    };
    kOsc1Oct.slider.textFromValueFunction = octText;
    kOsc1Oct.slider.valueFromTextFunction = octVal;

    // OSC2 octave: 0=16'  1=8'  2=4'
    kOsc2Oct.slider.textFromValueFunction = [](double v) -> juce::String {
        switch (juce::roundToInt(v)) {
            case 0:  return "16'";
            case 2:  return "4'";
            default: return "8'";
        }
    };
    kOsc2Oct.slider.valueFromTextFunction = [](const juce::String& t) -> double {
        auto s = t.trim();
        if (s == "16'") return 0.0;
        if (s == "4'")  return 2.0;
        return 1.0;
    };

    // ── Waveform display: internal 0–15 → label 1–16 ─────────────────
    auto waveText = [](double v) -> juce::String {
        return juce::String(juce::roundToInt(v) + 1);
    };
    auto waveVal = [](const juce::String& t) -> double {
        return (double)(t.getIntValue() - 1);
    };
    kOsc1Wave.slider.textFromValueFunction = waveText;
    kOsc1Wave.slider.valueFromTextFunction = waveVal;
    kOsc2Wave.slider.textFromValueFunction = waveText;
    kOsc2Wave.slider.valueFromTextFunction = waveVal;

    // OSC2 Interval: internal 0-4 → panel labels 1 / -3 / 3 / 4 / 5
    static const char* kIntervalLabels[] = { "1", "-3", "3", "4", "5" };
    kOsc2Int.slider.textFromValueFunction = [](double v) -> juce::String {
        int i = juce::roundToInt(v);
        if (i < 0 || i > 4) return "1";
        return kIntervalLabels[i];
    };
    kOsc2Int.slider.valueFromTextFunction = [](const juce::String& t) -> double {
        auto s = t.trim();
        if (s == "-3") return 1.0;
        if (s == "3")  return 2.0;
        if (s == "4")  return 3.0;
        if (s == "5")  return 4.0;
        return 0.0;  // "1" = Unison
    };

    // ── KEY ASSIGN radio buttons ──────────────────────────────────────
    for (auto* b : { &btnPoly1, &btnPoly2, &btnUnison1, &btnUnison2 })
    {
        b->setClickingTogglesState(false);
        addAndMakeVisible(b);
    }
    btnPoly1.onClick   = [this] { setKeyAssignMode(0); };
    btnPoly2.onClick   = [this] { setKeyAssignMode(1); };
    btnUnison1.onClick = [this] { setKeyAssignMode(2); };
    btnUnison2.onClick = [this] { setKeyAssignMode(3); };
    updateKeyAssignButtons();

    // ── MG waveform labels ────────────────────────────────────────────
    static const char* kMgWaveLabels[] = { "TRI", "SAW\xe2\x86\x91", "SAW\xe2\x86\x93", "SQ" };
    kMgWav.slider.textFromValueFunction = [](double v) -> juce::String {
        int i = juce::roundToInt(v);
        if (i < 0 || i > 3) i = 0;
        return kMgWaveLabels[i];
    };
    kMgWav.slider.valueFromTextFunction = [](const juce::String& t) -> double {
        auto s = t.trim();
        if (s.startsWith("SAW") && s.contains("\xe2\x86\x91")) return 1.0;
        if (s.startsWith("SAW"))                                 return 2.0;
        if (s == "SQ")                                           return 3.0;
        return 0.0; // TRI
    };

    // ── Arp clock display: 1→INT/32, 2→INT/16, 3→INT/8, 4→EXT/32 … ──────
    static const char* kClockLabels[] = { "INT/32","INT/16","INT/8","EXT/32","EXT/16","EXT/8" };
    kArpClock.slider.textFromValueFunction = [](double v) -> juce::String {
        int i = juce::roundToInt(v) - 1;
        if (i < 0 || i > 5) i = 2;
        return kClockLabels[i];
    };
    kArpClock.slider.valueFromTextFunction = [](const juce::String& t) -> double {
        auto s = t.trim();
        for (int i = 0; i < 6; ++i)
            if (s == kClockLabels[i]) return (double)(i + 1);
        return 3.0;
    };

    // ── Arp buttons setup ─────────────────────────────────────────────
    for (auto* b : { &btnArpOn, &btnArpMode, &btnArpLatch, &btnArpOct })
    {
        b->setClickingTogglesState(false);
        addAndMakeVisible(b);
    }

    btnArpOn.onClick = [this]
    {
        auto* p = dynamic_cast<juce::AudioParameterBool*>(proc_.apvts.getParameter("arp_on"));
        if (p) { p->beginChangeGesture(); *p = !p->get(); p->endChangeGesture(); }
        updateArpButtons();
    };
    btnArpMode.onClick = [this]
    {
        auto* p = dynamic_cast<juce::AudioParameterInt*>(proc_.apvts.getParameter("arp_mode"));
        if (p) { p->beginChangeGesture(); *p = 1 - p->get(); p->endChangeGesture(); }
        updateArpButtons();
    };
    btnArpLatch.onClick = [this]
    {
        auto* p = dynamic_cast<juce::AudioParameterBool*>(proc_.apvts.getParameter("arp_latch"));
        if (p) { p->beginChangeGesture(); *p = !p->get(); p->endChangeGesture(); }
        updateArpButtons();
    };
    btnArpOct.onClick = [this]
    {
        auto* p = dynamic_cast<juce::AudioParameterInt*>(proc_.apvts.getParameter("arp_octave"));
        if (p)
        {
            p->beginChangeGesture();
            *p = (p->get() + 1) % 3;
            p->endChangeGesture();
        }
        updateArpButtons();
    };
    updateArpButtons();

    btnExport.onClick = [this] { exportSyx(); };
    btnImport.onClick = [this] { importSyx(); };
    addAndMakeVisible(btnExport);
    addAndMakeVisible(btnImport);

    // ── Panic button ──────────────────────────────────────────────────
    btnPanic.setClickingTogglesState(false);
    btnPanic.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF5A1A1A));
    btnPanic.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFF6666));
    btnPanic.onClick = [this] { proc_.panicAllNotesOff(); };
    addAndMakeVisible(btnPanic);

    // ── Delay bypass button ───────────────────────────────────────────
    btnDelayBypass.setClickingTogglesState(false);
    btnDelayBypass.onClick = [this]
    {
        auto* p = dynamic_cast<juce::AudioParameterBool*>(
            proc_.apvts.getParameter("delay_bypass"));
        if (p) { p->beginChangeGesture(); *p = !p->get(); p->endChangeGesture(); }
        btnDelayBypass.setToggleState(
            [&]{ auto* q = dynamic_cast<juce::AudioParameterBool*>(
                    proc_.apvts.getParameter("delay_bypass"));
                 return q && q->get(); }(),
            juce::dontSendNotification);
    };
    addAndMakeVisible(btnDelayBypass);

    // ── Preset selector ───────────────────────────────────────────────
    addAndMakeVisible(presetDisplay_);

    for (auto* b : { &btnBank, &btnProg, &btnPresetUp, &btnPresetDown })
    {
        b->setClickingTogglesState(false);
        addAndMakeVisible(b);
    }
    btnBank.onClick = [this]
    {
        int idx  = proc_.getCurrentProgram();
        int bank = idx / 8;
        int prog = idx % 8;
        proc_.setCurrentProgram(((bank + 1) % 8) * 8 + prog);
    };
    btnProg.onClick = [this]
    {
        int idx  = proc_.getCurrentProgram();
        int bank = idx / 8;
        int prog = idx % 8;
        proc_.setCurrentProgram(bank * 8 + (prog + 1) % 8);
    };
    btnPresetUp.onClick = [this]
    {
        int next = (proc_.getCurrentProgram() + 1) % 64;
        proc_.setCurrentProgram(next);
    };
    btnPresetDown.onClick = [this]
    {
        int prev = (proc_.getCurrentProgram() + 63) % 64;   // +63 mod 64 = -1 with wrap
        proc_.setCurrentProgram(prev);
    };

    addAndMakeVisible(keyboard_);

    startTimerHz(20);   // poll key assign param 20×/sec to stay in sync

    setSize(1210, BARY + 44);
}

// ─────────────────────────────────────────────────────────────────────────
// paint – background + section boxes
// ─────────────────────────────────────────────────────────────────────────
void DW8000Editor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1A1A1A));

    if (draggingOver_)
    {
        g.setColour(juce::Colours::orange.withAlpha(0.12f));
        g.fillAll();
    }

    // ── Row 1 section boxes ──────────────────────────────────────────
    sect(g, "OSC 1",      { 8,            R1Y,  3*CW,   CH });
    sect(g, "AUTO BEND",  { 8 + 3*CW+8,   R1Y,  4*CW,   CH });
    sect(g, "OSC 2",      { 8 + 7*CW+16,  R1Y,  5*CW,   CH });
    sect(g, "NOISE",      { 8 + 12*CW+24, R1Y,  1*CW,   CH });
    sect(g, "MG / LFO",   { 8 + 13*CW+32, R1Y,  5*CW,   CH });

    // ── WJ-8000 logo badge (right of MG/LFO) ────────────────────────
    {
        int lx = 8 + 13*CW + 32 + 5*CW + 10;   // right of MG/LFO + gap
        int ly = 28;                              // top of row-1 header area
        int lw = getWidth() - lx - 8;
        int lh = R1Y + CH - ly;                  // full height of row 1

        auto lb = juce::Rectangle<int>(lx, ly, lw, lh).toFloat();
        float cr = 5.0f;

        // ── Outer plate (dark metal base) ────────────────────────────
        juce::ColourGradient bgGrad(
            juce::Colour(0xFF1C1C1C), lb.getX(),    lb.getY(),
            juce::Colour(0xFF141414), lb.getX(),    lb.getBottom(), false);
        bgGrad.addColour(0.35, juce::Colour(0xFF252525));
        bgGrad.addColour(0.65, juce::Colour(0xFF1C1C1C));
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(lb, cr);

        // ── Chamfered border (2-tone bevel) ──────────────────────────
        // Dark shadow bottom-right
        g.setColour(juce::Colour(0xFF080808));
        g.drawRoundedRectangle(lb, cr, 2.0f);
        // Lighter top-left highlight
        auto inner = lb.reduced(2.5f);
        g.setColour(juce::Colour(0xFF3A3A3A));
        g.drawRoundedRectangle(inner, cr - 1.5f, 1.0f);
        // Inner inner dark border
        auto inner2 = inner.reduced(2.0f);
        g.setColour(juce::Colour(0xFF111111));
        g.drawRoundedRectangle(inner2, cr - 2.5f, 0.8f);

        // ── Subtle horizontal brush lines ─────────────────────────────
        g.setColour(juce::Colour(0xFF222222));
        for (float hy = lb.getY() + 6.0f; hy < lb.getBottom() - 4.0f; hy += 3.0f)
            g.drawLine(lb.getX() + 6.0f, hy, lb.getRight() - 6.0f, hy, 0.6f);

        // ── Brass/bronze text glow (shadow pass) ─────────────────────
        g.setColour(juce::Colour(0xFF5A3800).withAlpha(0.55f));
        g.setFont(juce::FontOptions(34.0f, juce::Font::bold));
        g.drawText("WJ-8000",
                   lb.translated(0.0f, 2.0f).toNearestInt(),
                   juce::Justification::centred);

        // ── Main brass text ───────────────────────────────────────────
        juce::ColourGradient txtGrad(
            juce::Colour(0xFFD4AA3A), lb.getX(), lb.getCentreY() - 10.0f,
            juce::Colour(0xFF7A5A10), lb.getX(), lb.getCentreY() + 18.0f, false);
        txtGrad.addColour(0.45, juce::Colour(0xFFE8C84A));
        g.setGradientFill(txtGrad);
        g.setFont(juce::FontOptions(34.0f, juce::Font::bold));
        g.drawText("WJ-8000", lb.toNearestInt(), juce::Justification::centred);

        // ── Top specular highlight ────────────────────────────────────
        juce::ColourGradient sheen(
            juce::Colour(0xFFFFFFFF).withAlpha(0.04f), lb.getX(), lb.getY(),
            juce::Colour(0x00000000),                   lb.getX(), lb.getCentreY(), false);
        g.setGradientFill(sheen);
        g.fillRoundedRectangle(lb, cr);
    }

    // ── Row 2 section boxes ──────────────────────────────────────────
    sect(g, "VCF",        { 8,             R2Y, 12*CW,   CH });
    sect(g, "VCA",        { 8 + 12*CW+8,   R2Y,  7*CW,   CH });

    // ── Row 3 section boxes ──────────────────────────────────────────
    sect(g, "KEY ASSIGN",  { 8,              R3Y,  2*CW,   CH });
    sect(g, "VOICE",       { 8 + 2*CW+8,    R3Y,  1*CW,   CH });
    sect(g, "BEND",        { 8 + 3*CW+16,   R3Y,  2*CW,   CH });
    sect(g, "AFTERTOUCH",  { 8 + 5*CW+24,   R3Y,  3*CW,   CH });
    sect(g, "DELAY",       { 8 + 8*CW+32,   R3Y,  6*CW,   CH });
    sect(g, "ARPEGGIATOR", { 8 + 14*CW+40,  R3Y,  4*CW,   CH });
    sect(g, "PRESET",      { 8 + 18*CW+48,  R3Y,  3*CW,   CH });

}

// ─────────────────────────────────────────────────────────────────────────
// resized – place every slider inside its section box
// ─────────────────────────────────────────────────────────────────────────
void DW8000Editor::resized()
{
    // ── Row 1 ────────────────────────────────────────────────────────
    lay({ 8,            R1Y, 3*CW, CH }, { &kOsc1Oct,  &kOsc1Wave,  &kOsc1Lvl });
    lay({ 8 + 3*CW+8,   R1Y, 4*CW, CH }, { &kAutoBendSel, &kAutoBendMode,
                                             &kAutoBendTime, &kAutoBendInt });
    lay({ 8 + 7*CW+16,  R1Y, 5*CW, CH }, { &kOsc2Oct,  &kOsc2Wave, &kOsc2Lvl,
                                             &kOsc2Int,  &kOsc2Det });
    lay({ 8 + 12*CW+24, R1Y, 1*CW, CH }, { &kNoise });
    lay({ 8 + 13*CW+32, R1Y, 5*CW, CH }, { &kMgWav, &kMgFreq, &kMgDly,
                                             &kMgOsc, &kMgVcf });

    // ── Row 2 ────────────────────────────────────────────────────────
    lay({ 8,             R2Y, 12*CW, CH }, { &kVcfCut, &kVcfRes,  &kVcfKbd,
                                              &kVcfEGPol, &kVcfEGInt,
                                              &kVcfAtk, &kVcfDec,  &kVcfBrk,
                                              &kVcfSlp, &kVcfSus,  &kVcfRel, &kVcfVel });
    lay({ 8 + 12*CW+8,   R2Y,  7*CW, CH }, { &kVcaAtk, &kVcaDec, &kVcaBrk,
                                               &kVcaSlp, &kVcaSus, &kVcaRel, &kVcaVel });

    // ── Row 3 ────────────────────────────────────────────────────────
    // KEY ASSIGN: 4 radio buttons stacked vertically inside their section
    {
        constexpr int btnH  = 28;
        constexpr int secW  = 2 * CW;
        constexpr int pad   = 6;
        int btnW = secW - 2 * pad;
        int spacing = (CH - 4 * btnH) / 5;
        int x = 8 + pad;
        btnPoly1.setBounds  (x, R3Y + spacing,               btnW, btnH);
        btnPoly2.setBounds  (x, R3Y + spacing*2 +   btnH,    btnW, btnH);
        btnUnison1.setBounds(x, R3Y + spacing*3 + 2*btnH,    btnW, btnH);
        btnUnison2.setBounds(x, R3Y + spacing*4 + 3*btnH,    btnW, btnH);
    }
    lay({ 8 + 2*CW+8,   R3Y, 1*CW, CH }, { &kPortoTime });
    lay({ 8 + 3*CW+16,  R3Y, 2*CW, CH }, { &kBendRng, &kBendVcf });
    lay({ 8 + 5*CW+24,  R3Y, 3*CW, CH }, { &kAtOsc, &kAtVcf, &kAtVca });
    lay({ 8 + 8*CW+32,  R3Y, 6*CW, CH }, { &kDlyTim, &kDlyFac, &kDlyFbk,
                                             &kDlyMFq, &kDlyMIn, &kDlyEfx });
    // BYPASS button sits in the section-label tab row (above the DELAY box)
    {
        constexpr int dx = 8 + 8*CW + 32;
        constexpr int dw = 6*CW;
        constexpr int bw = 52;
        btnDelayBypass.setBounds(dx + dw - bw, R3Y - HDR, bw, HDR - 2);
    }

    // ARP section: left 2*CW = 4 buttons, right 2*CW = 2 sliders
    {
        constexpr int btnH  = 28;
        constexpr int secX  = 8 + 14*CW + 40;
        constexpr int secW  = 2 * CW;
        constexpr int pad   = 6;
        int btnW   = secW - 2 * pad;
        int spacing = (CH - 4 * btnH) / 5;
        int bx = secX + pad;
        btnArpOn.setBounds   (bx, R3Y + spacing,               btnW, btnH);
        btnArpMode.setBounds (bx, R3Y + spacing*2 +   btnH,    btnW, btnH);
        btnArpLatch.setBounds(bx, R3Y + spacing*3 + 2*btnH,    btnW, btnH);
        btnArpOct.setBounds  (bx, R3Y + spacing*4 + 3*btnH,    btnW, btnH);
    }
    lay({ 8 + 16*CW+40, R3Y, 2*CW, CH }, { &kArpSpeed, &kArpClock });

    // PRESET section: LCD on top, then two rows of buttons
    {
        constexpr int px  = 8 + 18*CW + 48;
        constexpr int pw  = 3*CW;
        constexpr int pad = 8;
        constexpr int gap = 4;

        // LCD display — top 52% of section height
        int lcdW = pw - 2*pad;
        int lcdH = (int)(CH * 0.52f);
        presetDisplay_.setBounds(px + pad, R3Y + pad, lcdW, lcdH);

        // Remaining vertical space split into two button rows
        int remaining = CH - lcdH - pad * 2 - gap;
        int btnH      = juce::jmax(20, remaining / 2 - gap);
        int halfW     = (lcdW - gap) / 2;

        // Row 1: BANK | PROG
        int row1Y = R3Y + lcdH + pad;
        btnBank.setBounds(px + pad,            row1Y, halfW, btnH);
        btnProg.setBounds(px + pad + halfW + gap, row1Y, halfW, btnH);

        // Row 2: – | +   (step one preset at a time through 11-88)
        int row2Y = row1Y + btnH + gap;
        btnPresetDown.setBounds(px + pad,            row2Y, halfW, btnH);
        btnPresetUp.setBounds  (px + pad + halfW + gap, row2Y, halfW, btnH);
    }

    // ── Keyboard ─────────────────────────────────────────────────────
    keyboard_.setBounds(8, KBY, getWidth() - 16, KBH);

    // ── Bottom bar buttons ────────────────────────────────────────────
    btnPanic.setBounds (8,                 BARY + 6, 80, 28);
    btnImport.setBounds(getWidth() - 104,  BARY + 6, 96, 28);
    btnExport.setBounds(getWidth() - 208,  BARY + 6, 96, 28);
}

// ─────────────────────────────────────────────────────────────────────────
// Destructor
// ─────────────────────────────────────────────────────────────────────────
DW8000Editor::~DW8000Editor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ─────────────────────────────────────────────────────────────────────────
// KEY ASSIGN helpers
// ─────────────────────────────────────────────────────────────────────────
void DW8000Editor::setKeyAssignMode(int mode)
{
    auto* param = dynamic_cast<juce::AudioParameterInt*>(
        proc_.apvts.getParameter("p_0D"));
    if (param)
    {
        param->beginChangeGesture();
        *param = mode;
        param->endChangeGesture();
    }
    updateKeyAssignButtons();
}

void DW8000Editor::updateKeyAssignButtons()
{
    auto* param = dynamic_cast<juce::AudioParameterInt*>(
        proc_.apvts.getParameter("p_0D"));
    int mode = param ? param->get() : 0;

    btnPoly1.setToggleState  (mode == 0, juce::dontSendNotification);
    btnPoly2.setToggleState  (mode == 1, juce::dontSendNotification);
    btnUnison1.setToggleState(mode == 2, juce::dontSendNotification);
    btnUnison2.setToggleState(mode == 3, juce::dontSendNotification);
}

void DW8000Editor::timerCallback()
{
    updateKeyAssignButtons();
    updateArpButtons();
    presetDisplay_.repaint();
    keyboard_.repaint();

    // Keep delay bypass button lit state in sync with parameter
    {
        auto* p = dynamic_cast<juce::AudioParameterBool*>(
            proc_.apvts.getParameter("delay_bypass"));
        bool bypassed = p && p->get();
        btnDelayBypass.setToggleState(bypassed, juce::dontSendNotification);
    }

}

void DW8000Editor::updateArpButtons()
{
    auto* pOn    = dynamic_cast<juce::AudioParameterBool*>(proc_.apvts.getParameter("arp_on"));
    auto* pMode  = dynamic_cast<juce::AudioParameterInt*> (proc_.apvts.getParameter("arp_mode"));
    auto* pLatch = dynamic_cast<juce::AudioParameterBool*>(proc_.apvts.getParameter("arp_latch"));
    auto* pOct   = dynamic_cast<juce::AudioParameterInt*> (proc_.apvts.getParameter("arp_octave"));

    btnArpOn.setToggleState   (pOn    && pOn->get(),    juce::dontSendNotification);
    btnArpMode.setToggleState (pMode  && pMode->get() == 1, juce::dontSendNotification);
    btnArpLatch.setToggleState(pLatch && pLatch->get(), juce::dontSendNotification);

    static const char* kOctLabels[] = { "OCT 1", "OCT 2", "FULL" };
    int oct = pOct ? pOct->get() : 0;
    btnArpOct.setButtonText(kOctLabels[juce::jlimit(0, 2, oct)]);
    // OCT button lights up when range > 1 octave
    btnArpOct.setToggleState(oct > 0, juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────────────────
// FileDragAndDropTarget
// ─────────────────────────────────────────────────────────────────────────
bool DW8000Editor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase(".syx")) return true;
    return false;
}

void DW8000Editor::fileDragEnter(const juce::StringArray&, int, int)
{
    draggingOver_ = true;
    repaint();
}

void DW8000Editor::fileDragExit(const juce::StringArray&)
{
    draggingOver_ = false;
    repaint();
}

void DW8000Editor::filesDropped(const juce::StringArray& files, int, int)
{
    draggingOver_ = false;
    repaint();

    for (auto& path : files)
    {
        juce::File f(path);
        if (!f.hasFileExtension(".syx")) continue;

        juce::MemoryBlock mb;
        if (!f.loadFileAsData(mb)) continue;

        const auto* data = static_cast<const uint8_t*>(mb.getData());
        int         size = (int)mb.getSize();

        if (size == 64 * 57 || size == 64 * 66) { proc_.loadBankFromFile(f); return; }
        if (size == 57 && DW8000SysEx::isValidDW8000Dump(data, 57))
                                    { proc_.loadPatch(DW8000Patch::fromSysExBytes(data + 5)); return; }
    }
}

// ─────────────────────────────────────────────────────────────────────────
// SysEx file I/O
// ─────────────────────────────────────────────────────────────────────────
void DW8000Editor::exportSyx()
{
    auto fc = std::make_shared<juce::FileChooser>(
        "Save DW-8000 SysEx", juce::File{}, "*.syx");
    fc->launchAsync(juce::FileBrowserComponent::saveMode
                    | juce::FileBrowserComponent::canSelectFiles,
        [this, fc](const juce::FileChooser& chooser)
        {
            auto result = chooser.getResult();
            if (result == juce::File{}) return;
            auto msg = DW8000SysEx::makeDataDump(proc_.getCurrentPatch());
            result.replaceWithData(msg.getRawData(), (size_t)msg.getRawDataSize());
        });
}

void DW8000Editor::importSyx()
{
    auto fc = std::make_shared<juce::FileChooser>(
        "Load DW-8000 SysEx", juce::File{}, "*.syx");
    fc->launchAsync(juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles,
        [this, fc](const juce::FileChooser& chooser)
        {
            auto result = chooser.getResult();
            if (result == juce::File{}) return;

            juce::MemoryBlock mb;
            result.loadFileAsData(mb);
            const auto* data = static_cast<const uint8_t*>(mb.getData());
            int         size = (int)mb.getSize();

            if (size == 64 * 57 || size == 64 * 66)
                proc_.loadBankFromFile(result);
            else if (size == 57 && DW8000SysEx::isValidDW8000Dump(data, 57))
                proc_.loadPatch(DW8000Patch::fromSysExBytes(data + 5));
        });
}
