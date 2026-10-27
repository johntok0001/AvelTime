#include "PluginEditor.h"

using namespace avel::time;
using namespace avel::time::ui;
using namespace avel::ui;

namespace
{
    juce::RangedAudioParameter& P (AvelTimeProcessor& p, const juce::String& id)
    {
        auto* prm = p.apvts.getParameter (id);
        jassert (prm != nullptr);
        return *prm;
    }
    float V (AvelTimeProcessor& p, const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); }

    juce::Path logoGlyph()
    {
        // three echoes fading to the right
        juce::Path p;
        p.startNewSubPath (6.5f, 7.0f);  p.lineTo (6.5f, 17.0f);
        p.startNewSubPath (11.5f, 8.5f); p.lineTo (11.5f, 15.5f);
        p.startNewSubPath (16.0f, 10.0f); p.lineTo (16.0f, 14.0f);
        p.startNewSubPath (19.5f, 11.2f); p.lineTo (19.5f, 12.8f);
        return p;
    }
}

AvelTimeEditor::AvelTimeEditor (AvelTimeProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p), tapView (p), filterView (p), envPreview (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    auto* um = &proc.undo;

    // ---------------- top bar
    addAndMakeVisible (presetPrev);
    addAndMakeVisible (presetPill);
    addAndMakeVisible (presetNext);
    presetPrev.setTooltip ("Previous preset");
    presetNext.setTooltip ("Next preset");
    presetPrev.onClick = [this] { proc.undo.beginNewTransaction(); proc.presets.step (-1); };
    presetNext.onClick = [this] { proc.undo.beginNewTransaction(); proc.presets.step (1); };
    presetPill.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (undoBtn);
    addAndMakeVisible (redoBtn);
    undoBtn.setTooltip ("Undo");
    redoBtn.setTooltip ("Redo");
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };

    addAndMakeVisible (viewSeg);
    viewSeg.fontSize = 12.5f;
    viewSeg.setTooltip ("Delay: times and character. Filters: the filters inside the feedback loop. Modulation: LFOs, envelope and routing");
    viewSeg.onChange = [this] (int v) { setView (v); };

    freezeBtn.setIcon (icon::snowflake());
    freezeBtn.setTint (product::accentB);
    freezeBtn.setFilledBackground (true);
    freezeBtn.setTooltip ("Freeze: hold the echoes in the loop forever and stop recording new input");
    freezeBtn.onClick = [this]
    {
        proc.undo.beginNewTransaction();
        auto& prm = P (proc, pid::freeze);
        prm.beginChangeGesture();
        prm.setValueNotifyingHost (V (proc, pid::freeze) > 0.5f ? 0.0f : 1.0f);
        prm.endChangeGesture();
    };
    addAndMakeVisible (freezeBtn);
    for (auto* b : { &abA, &abB })
    {
        b->setFilledBackground (true);
        addAndMakeVisible (*b);
    }
    abA.setTooltip ("Settings A");
    abB.setTooltip ("Settings B");
    abA.onClick = [this] { proc.switchAB (0); };
    abB.onClick = [this] { proc.switchAB (1); };
    abCopy.setTooltip ("Copy the current settings to the other slot");
    abCopy.onClick = [this] { proc.copyABToOther(); };
    addAndMakeVisible (abCopy);

    // ---------------- delay view
    addAndMakeVisible (tapView);
    tapView.setTooltip ("Echoes over time: left side above the line, right side below. Drag the first echo of each side to set its time (Shift: whole ms)");
    addAndMakeVisible (delayCard);
    syncSw.bind (P (proc, pid::sync), um);
    syncSw.setTooltip ("Sync the delay times to the host tempo");
    linkSw.bind (P (proc, pid::link), um);
    linkSw.setTooltip ("Use the left time for both sides");
    addAndMakeVisible (syncSw);
    addAndMakeVisible (linkSw);
    tapBtn.setIcon (icon::tap());
    tapBtn.setFilledBackground (true);
    tapBtn.setTooltip ("Tap the delay time (two or more taps)");
    tapBtn.onClick = [this] { tapTempo(); };
    addAndMakeVisible (tapBtn);
    tempoText.just = juce::Justification::centredLeft;
    addAndMakeVisible (tempoText);

    routingSeg.setOptions ({ "Stereo", "Ping-Pong", "Mono" });
    routingSeg.bind (P (proc, pid::routing), um);
    routingSeg.setTooltip ("Stereo: L and R echo separately. Ping-Pong: echoes bounce between the sides. Mono: one input for both lines");
    timeModeSeg.setOptions ({ "Tape", "Fade", "Jump" });
    timeModeSeg.bind (P (proc, pid::timeMode), um);
    timeModeSeg.setTooltip ("How the echoes react to time changes: Tape glides with a pitch bend, Fade crossfades, Jump switches quickly");
    addAndMakeVisible (routingSeg);
    addAndMakeVisible (timeModeSeg);

    feedback.bind (P (proc, pid::feedback), um);
    cross.bind (P (proc, pid::cross), um);
    width.bind (P (proc, pid::width), um);
    pan.bind (P (proc, pid::pan), um);
    pan.setBipolar (true);
    drive.bind (P (proc, pid::drive), um);
    diffusion.bind (P (proc, pid::diffusion), um);
    pitch.bind (P (proc, pid::pitch), um);
    pitch.setBipolar (true);
    duck.bind (P (proc, pid::duck), um);
    timeL.setTooltip ("Left delay time. Drag the handles on the display too");
    timeR.setTooltip ("Right delay time");
    feedback.setTooltip ("How much of each echo returns into the loop. Above 100 % it builds up into self-oscillation (kept in check by a soft limiter)");
    cross.setTooltip ("Cross feedback: how much of each side feeds the other side");
    width.setTooltip ("Stereo width of the echoes");
    pan.setTooltip ("Balance of the echoes");
    drive.setTooltip ("Saturation inside the loop: every repeat gets a little warmer");
    diffusion.setTooltip ("Smears each repeat into a soft cloud");
    pitch.setTooltip ("Pitch shift per repeat, in semitones. +12 gives shimmer, -12 an octave-down cascade");
    duck.setTooltip ("Turns the echoes down while you play and lets them bloom in the gaps");
    for (auto* d : { &timeL, &timeR, &feedback, &cross, &width, &pan, &drive, &diffusion, &pitch, &duck })
        addAndMakeVisible (*d);
    driveStyle.bind (P (proc, pid::driveStyle), um);
    driveStyle.fontSize = 11.5f;
    driveStyle.setTooltip ("Drive character");
    addAndMakeVisible (driveStyle);
    diffSize.bind (P (proc, pid::diffSize), um);
    diffSize.setTooltip ("Diffusion size: short and grainy to long and smooth");
    duckRelease.bind (P (proc, pid::duckRelease), um);
    duckRelease.setTooltip ("How fast the echoes come back after you stop playing");
    addAndMakeVisible (diffSize);
    addAndMakeVisible (duckRelease);
    addAndMakeVisible (timeAltL);
    addAndMakeVisible (timeAltR);
    bindTimeDials();

    // ---------------- filter view
    addAndMakeVisible (filterView);
    filterView.setTooltip ("Filters inside the feedback loop: every repeat passes through them again. Drag a node, scroll for Q, double-click to add or switch a filter, right-click for options");
    addAndMakeVisible (filterCard);
    filterView.onSelect = [this] (int b) { selectBand (b); };
    for (int i = 0; i < numFilters; ++i)
    {
        auto& b = bandBtns[(size_t) i];
        b.setText (juce::String (i + 1));
        b.setTint (col::band (i));
        b.setFilledBackground (true);
        b.setTooltip ("Edit filter " + juce::String (i + 1));
        b.onClick = [this, i] { selectBand (i); };
        addAndMakeVisible (b);
    }
    bandOn.setTooltip ("Turn this filter on or off");
    addAndMakeVisible (bandOn);
    bandType.setTooltip ("Filter type");
    addAndMakeVisible (bandType);
    bandFreq.setTooltip ("Filter frequency");
    bandGain.setTooltip ("Filter gain (Bell and Shelf types)");
    bandQ.setTooltip ("Filter resonance / bandwidth");
    addAndMakeVisible (bandFreq);
    addAndMakeVisible (bandGain);
    addAndMakeVisible (bandQ);
    bandGain.setBipolar (true);

    // ---------------- modulation view
    for (auto& c : srcCards) addAndMakeVisible (c);
    srcCards[0].title = "LFO 1"; srcCards[1].title = "LFO 2"; srcCards[2].title = "Envelope";
    for (int i = 0; i < numLfos; ++i)
    {
        lfoPreview[(size_t) i] = std::make_unique<LfoPreview> (proc, i);
        addAndMakeVisible (*lfoPreview[(size_t) i]);
        auto& rate = lfoRate[(size_t) i];
        rate.setCaption ("Rate"); rate.setColour (product::accentA);
        rate.bind (P (proc, pid::lfo (i, "Rate")), um);
        rate.setTooltip ("LFO speed in Hz (when Sync is off)");
        auto& ph = lfoPhase[(size_t) i];
        ph.setCaption ("Stereo"); ph.setColour (product::accentB);
        ph.bind (P (proc, pid::lfo (i, "Phase")), um);
        ph.setTooltip ("Phase offset of the right channel: 0 moves both sides together, 180 in opposite directions");
        lfoShape[(size_t) i].bind (P (proc, pid::lfo (i, "Shape")), um);
        lfoShape[(size_t) i].setTooltip ("LFO shape");
        lfoDiv[(size_t) i].bind (P (proc, pid::lfo (i, "Div")), um);
        lfoDiv[(size_t) i].setTooltip ("LFO cycle length in notes (when Sync is on)");
        lfoSync[(size_t) i] = std::make_unique<Switch> ("Sync", product::accentA);
        lfoSync[(size_t) i]->bind (P (proc, pid::lfo (i, "Sync")), um);
        lfoSync[(size_t) i]->setTooltip ("Lock the LFO to the host tempo and song position");
        for (juce::Component* c : { (juce::Component*) &rate, (juce::Component*) &ph, (juce::Component*) &lfoShape[(size_t) i],
                                    (juce::Component*) &lfoDiv[(size_t) i], (juce::Component*) lfoSync[(size_t) i].get() })
            addAndMakeVisible (*c);
    }
    addAndMakeVisible (envPreview);
    envAttack.bind (P (proc, pid::envAttack), um);
    envRelease.bind (P (proc, pid::envRelease), um);
    envSens.bind (P (proc, pid::envSens), um);
    envSens.setBipolar (true);
    envAttack.setTooltip ("How fast the envelope follows a rising input");
    envRelease.setTooltip ("How fast the envelope falls back");
    envSens.setTooltip ("Envelope sensitivity: raise it for quiet sources");
    addAndMakeVisible (envAttack);
    addAndMakeVisible (envRelease);
    addAndMakeVisible (envSens);

    juce::StringArray dstNames = modTargetNames();
    for (int i = 0; i < numSlots; ++i)
    {
        auto& card = slotCards[(size_t) i];
        card.title = "Slot " + juce::String (i + 1);
        card.titleColour = col::label2;
        card.radius = 16.0f;
        addAndMakeVisible (card);
        slotSrc[(size_t) i].bind (P (proc, pid::slot (i, "Src")), um);
        slotSrc[(size_t) i].setTooltip ("Modulation source");
        slotDst[(size_t) i].bind (P (proc, pid::slot (i, "Dst")), um);
        slotDst[(size_t) i].setSections ({ { 1, "Delay" }, { 6, "Filters" }, { 10, "Character" }, { 13, "Output" } });
        slotDst[(size_t) i].setTooltip ("What the source moves");
        auto& amt = slotAmt[(size_t) i];
        amt.setCaption ("Amount");
        amt.setColour (product::accentA);
        amt.bind (P (proc, pid::slot (i, "Amt")), um);
        amt.setBipolar (true);
        amt.setTooltip ("Depth and direction. Time: 100 % = 10 ms, filters: 4 octaves, pitch: 12 semitones");
        addAndMakeVisible (slotSrc[(size_t) i]);
        addAndMakeVisible (slotDst[(size_t) i]);
        addAndMakeVisible (amt);
    }

    // ---------------- right column
    inMeter.captionText = "IN";
    outMeter.captionText = "OUT";
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    inputDial.bind (P (proc, pid::input), um);
    mixDial.bind (P (proc, pid::mix), um);
    outputDial.bind (P (proc, pid::output), um);
    inputDial.setBipolar (true);
    outputDial.setBipolar (true);
    inputDial.setTooltip ("Input level into the delay");
    mixDial.setTooltip ("Dry / echo balance. The dry signal stays at full level up to 50 %");
    outputDial.setTooltip ("Output level");
    addAndMakeVisible (inputDial);
    addAndMakeVisible (mixDial);
    addAndMakeVisible (outputDial);

    // ---------------- size and state
    constrainer.setMinimumSize (1000, 640);
    constrainer.setMaximumSize (2000, 1300);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const int w = juce::jlimit (1000, 2000, (int) proc.uiState.getProperty ("w", 1180));
    const int h = juce::jlimit (640, 1300, (int) proc.uiState.getProperty ("h", 720));
    setSize (w, h);
    restoring = false;

    selectBand ((int) proc.uiState.getProperty ("band", 0));
    view = -1;
    const int v = (int) proc.uiState.getProperty ("view", 0);
    viewSeg.setSelected (v, juce::dontSendNotification);
    setView (v);
    updateDynamic();
    startTimerHz (60);
}

AvelTimeEditor::~AvelTimeEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==================================================================================================
void AvelTimeEditor::bindTimeDials()
{
    const int s = V (proc, pid::sync) > 0.5f ? 1 : 0;
    if (s == boundSync) return;
    boundSync = s;
    auto* um = &proc.undo;
    if (s == 1)
    {
        timeL.bind (P (proc, pid::divL), um);
        timeR.bind (P (proc, pid::divR), um);
    }
    else
    {
        timeL.bind (P (proc, pid::timeL), um);
        timeR.bind (P (proc, pid::timeR), um);
    }
}

void AvelTimeEditor::selectBand (int b)
{
    band = juce::jlimit (0, numFilters - 1, b);
    filterView.selected = band;
    proc.uiState.setProperty ("band", band, nullptr);
    auto* um = &proc.undo;
    bandOn.bind (P (proc, pid::filt (band, "On")), um);
    bandType.bind (P (proc, pid::filt (band, "Type")), um);
    bandFreq.bind (P (proc, pid::filt (band, "Freq")), um);
    bandGain.bind (P (proc, pid::filt (band, "Gain")), um);
    bandQ.bind (P (proc, pid::filt (band, "Q")), um);
    const auto c = col::band (band);
    bandFreq.setColour (c); bandGain.setColour (c); bandQ.setColour (c);
    for (int i = 0; i < numFilters; ++i) bandBtns[(size_t) i].setToggled (i == band);
    filterCard.title = "Filter " + juce::String (band + 1);
    filterCard.titleColour = c;
    filterCard.repaint();
}

void AvelTimeEditor::setView (int v)
{
    v = juce::jlimit (0, 2, v);
    if (v == view) return;
    view = v;
    proc.uiState.setProperty ("view", v, nullptr);
    viewSeg.setSelected (v, juce::dontSendNotification);

    const bool d = v == 0, f = v == 1, m = v == 2;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &tapView, &delayCard, &syncSw, &linkSw, &tapBtn, &tempoText, &routingSeg, &timeModeSeg,
                                &timeL, &timeR, &feedback, &cross, &width, &pan, &drive, &diffusion, &pitch, &duck,
                                &driveStyle, &diffSize, &duckRelease, &timeAltL, &timeAltR })
        c->setVisible (d);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &filterView, &filterCard, &bandOn, &bandType, &bandFreq, &bandGain, &bandQ })
        c->setVisible (f);
    for (auto& b : bandBtns) b.setVisible (f);
    for (auto& c : srcCards) c.setVisible (m);
    for (int i = 0; i < numLfos; ++i)
        for (juce::Component* c : { (juce::Component*) lfoPreview[(size_t) i].get(), (juce::Component*) &lfoRate[(size_t) i], (juce::Component*) &lfoPhase[(size_t) i],
                                    (juce::Component*) &lfoShape[(size_t) i], (juce::Component*) &lfoDiv[(size_t) i], (juce::Component*) lfoSync[(size_t) i].get() })
            c->setVisible (m);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &envPreview, &envAttack, &envRelease, &envSens }) c->setVisible (m);
    for (int i = 0; i < numSlots; ++i)
        for (juce::Component* c : { (juce::Component*) &slotCards[(size_t) i], (juce::Component*) &slotSrc[(size_t) i],
                                    (juce::Component*) &slotDst[(size_t) i], (juce::Component*) &slotAmt[(size_t) i] })
            c->setVisible (m);
    resized();
    repaint();
}

//==================================================================================================
void AvelTimeEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141416), 0, 0, juce::Colour (0xff09090a), 0, b.getHeight(), false));
    g.fillAll();

    // logo
    auto logo = juce::Rectangle<float> (16.0f, 16.0f, 26.0f, 26.0f);
    g.setGradientFill (juce::ColourGradient (product::accentA, logo.getX(), logo.getY(), product::accentB, logo.getRight(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (logo.reduced (0.5f), 7.0f, 1.0f);
    icon::draw (g, logoGlyph(), logo.reduced (3.0f), juce::Colours::white, 2.0f);
    g.setFont (font (16.0f, Weight::semibold));
    g.setColour (col::label);
    g.drawText ("Avel", juce::Rectangle<float> (50.0f, 14.0f, 60.0f, 30.0f), juce::Justification::centredLeft, false);
    g.setFont (font (16.0f, Weight::regular));
    g.setColour (col::label2);
    const float aw = textWidth (font (16.0f, Weight::semibold), "Avel");
    g.drawText ("Time", juce::Rectangle<float> (50.0f + aw + 5.0f, 14.0f, 60.0f, 30.0f), juce::Justification::centredLeft, false);

    // display panel
    auto d = displayR.toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17171a), 0, d.getY(), juce::Colour (0xff0c0c0e), 0, d.getBottom(), false));
    g.fillRoundedRectangle (d, 14.0f);
    {
        juce::ColourGradient glow (product::accentA.withAlpha (0.035f), d.getCentreX(), d.getY(), product::accentA.withAlpha (0.0f), d.getCentreX(), d.getY() + d.getHeight() * 0.7f, true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (d, 14.0f);
    }
    g.setColour (col::stroke);
    g.drawRoundedRectangle (d.reduced (0.5f), 14.0f, 1.0f);

    // captions in the delay view overlay
    if (view == 0 && showOverlayCaptions)
    {
        g.setFont (caption());
        g.setColour (col::label3);
        auto tm = timeModeSeg.getBounds().toFloat();
        g.drawText ("TIME CHANGE", juce::Rectangle<float> (tm.getX() - 100.0f, tm.getY(), 92.0f, tm.getHeight()), juce::Justification::centredRight, false);
        auto rt = routingSeg.getBounds().toFloat();
        g.drawText ("ROUTING", juce::Rectangle<float> (rt.getX() - 80.0f, rt.getY(), 72.0f, rt.getHeight()), juce::Justification::centredRight, false);
    }
}

void AvelTimeEditor::resized()
{
    auto b = getLocalBounds();
    if (! restoring)
    {
        proc.uiState.setProperty ("w", getWidth(), nullptr);
        proc.uiState.setProperty ("h", getHeight(), nullptr);
    }

    // top bar
    {
        auto top = b.removeFromTop (58).reduced (16, 14);
        const int nameW = 50 + (int) textWidth (font (16.0f, Weight::semibold), "Avel") + 5 + (int) textWidth (font (16.0f, Weight::regular), "Time") + 18;
        top.removeFromLeft (nameW - 16);
        const int segW = juce::jlimit (240, 330, getWidth() / 4);
        viewSeg.setBounds (juce::Rectangle<int> (segW, 30).withCentre ({ getWidth() / 2, 29 }));
        const int pillW = juce::jlimit (110, 230, viewSeg.getX() - 16 - top.getX() - (28 + 2 + 2 + 28 + 10 + 60));
        presetPrev.setBounds (top.removeFromLeft (28).withSizeKeepingCentre (28, 28));
        top.removeFromLeft (2);
        presetPill.setBounds (top.removeFromLeft (pillW).withSizeKeepingCentre (pillW, 30));
        top.removeFromLeft (2);
        presetNext.setBounds (top.removeFromLeft (28).withSizeKeepingCentre (28, 28));
        top.removeFromLeft (10);
        undoBtn.setBounds (top.removeFromLeft (30).withSizeKeepingCentre (30, 30));
        redoBtn.setBounds (top.removeFromLeft (30).withSizeKeepingCentre (30, 30));

        abCopy.setBounds (top.removeFromRight (30).withSizeKeepingCentre (30, 30));
        top.removeFromRight (4);
        abB.setBounds (top.removeFromRight (30).withSizeKeepingCentre (30, 28));
        top.removeFromRight (4);
        abA.setBounds (top.removeFromRight (30).withSizeKeepingCentre (30, 28));
        top.removeFromRight (14);
        freezeBtn.setBounds (top.removeFromRight (freezeBtn.idealWidth() + 4).withSizeKeepingCentre (freezeBtn.idealWidth() + 4, 30));
    }

    b.removeFromBottom (12);
    b.removeFromLeft (12);
    b.removeFromRight (12);
    auto right = b.removeFromRight (90);
    b.removeFromRight (8);
    displayR = b;

    // right column
    {
        auto r = right;
        auto meters = r.removeFromTop (juce::jmax (120, r.getHeight() - 350)).reduced (6, 6);
        const int mw = (meters.getWidth() - 8) / 2;
        inMeter.setBounds (meters.removeFromLeft (mw));
        meters.removeFromLeft (8);
        outMeter.setBounds (meters.removeFromLeft (mw));
        auto dials = r.removeFromBottom (330);
        const int dh = 104;
        outputDial.setBounds (dials.removeFromBottom (dh).reduced (4, 0));
        dials.removeFromBottom (6);
        mixDial.setBounds (dials.removeFromBottom (dh).reduced (4, 0));
        dials.removeFromBottom (6);
        inputDial.setBounds (dials.removeFromBottom (dh).reduced (4, 0));
    }

    if (view == 0) layoutDelayView (displayR);
    else if (view == 1) layoutFilterView (displayR);
    else layoutModView (displayR);
}

void AvelTimeEditor::layoutDelayView (juce::Rectangle<int> d)
{
    const int cardH = 168;
    const int cardW = juce::jmin (d.getWidth() - 40, 960);
    auto card = juce::Rectangle<int> (cardW, cardH).withCentre ({ d.getCentreX(), 0 }).withBottomY (d.getBottom() - 16);
    delayCard.setGlassBounds (card);

    tapView.setBounds (d);
    tapView.setGraphInsets (54.0f, (float) (d.getBottom() - card.getY()) + 30.0f);

    // overlay row
    auto row = d.reduced (16, 0).withY (d.getY() + 12).withHeight (30);
    syncSw.setBounds (row.removeFromLeft (syncSw.idealWidth()));
    row.removeFromLeft (14);
    linkSw.setBounds (row.removeFromLeft (linkSw.idealWidth()));
    row.removeFromLeft (14);
    tapBtn.setBounds (row.removeFromLeft (tapBtn.idealWidth() + 6));
    row.removeFromLeft (10);
    tempoText.setBounds (row.removeFromLeft (76));
    row.removeFromLeft (8);
    const int segT = 200, segR = 250;
    showOverlayCaptions = row.getWidth() >= segT + segR + 100 + 80 + 10;
    const int spare = row.getWidth() - (showOverlayCaptions ? 190 : 12);
    const int wT = juce::jmin (segT, spare * 45 / 100), wR = juce::jmin (segR, spare - wT);
    timeModeSeg.setBounds (row.removeFromRight (wT));
    row.removeFromRight (showOverlayCaptions ? 108 : 12);
    routingSeg.setBounds (row.removeFromRight (wR));

    // card contents
    auto in = card.reduced (18, 14);
    const int gap = 22;
    const int colW = (in.getWidth() - gap) / 10;
    const int dialH = 104;
    auto dialRow = in.removeFromTop (dialH);
    Dial* dials[] { &timeL, &timeR, &feedback, &cross, &width, &pan, &drive, &diffusion, &pitch, &duck };
    for (int i = 0; i < 10; ++i)
    {
        if (i == 6) dialRow.removeFromLeft (gap);
        dials[i]->setBounds (dialRow.removeFromLeft (colW).reduced (2, 0));
    }
    in.removeFromTop (8);
    auto rowB = in.removeFromTop (24);
    auto under = [&] (Dial& dl, int w) { auto r = dl.getBounds(); return juce::Rectangle<int> (w, 24).withCentre ({ r.getCentreX(), rowB.getCentreY() }); };
    timeAltL.setBounds (under (timeL, colW));
    timeAltR.setBounds (under (timeR, colW));
    driveStyle.setBounds (under (drive, colW - 6));
    diffSize.setBounds (under (diffusion, colW));
    duckRelease.setBounds (under (duck, colW + 6));
}

void AvelTimeEditor::layoutFilterView (juce::Rectangle<int> d)
{
    const int cardH = 128;
    const int cardW = juce::jmin (d.getWidth() - 40, 700);
    auto card = juce::Rectangle<int> (cardW, cardH).withCentre ({ d.getCentreX(), 0 }).withBottomY (d.getBottom() - 30);
    filterCard.setGlassBounds (card);
    filterView.setBounds (d);
    filterView.setGraphInsets (12.0f, (float) (d.getBottom() - card.getY()) + 4.0f);

    auto in = card.reduced (16, 12);
    in.removeFromTop (30);   // title
    auto left = in.removeFromLeft (230);
    auto chips = left.removeFromTop (30);
    for (int i = 0; i < numFilters; ++i)
    {
        bandBtns[(size_t) i].setBounds (chips.removeFromLeft (32).withSizeKeepingCentre (30, 28));
        chips.removeFromLeft (6);
    }
    left.removeFromTop (12);
    auto lrow = left.removeFromTop (28);
    bandOn.setBounds (lrow.removeFromLeft (bandOn.idealWidth()));
    lrow.removeFromLeft (14);
    bandType.setBounds (lrow.removeFromLeft (juce::jmin (lrow.getWidth(), bandType.idealWidth (12.0f))).withSizeKeepingCentre (juce::jmin (lrow.getWidth(), bandType.idealWidth (12.0f)), 26));

    // move the dials up into the title row so they get full height
    auto dialsR = juce::Rectangle<int> (in.getRight() - 3 * 92, card.getY() + 10, 3 * 92, card.getHeight() - 20);
    bandFreq.setBounds (dialsR.removeFromLeft (92).reduced (4, 0));
    bandGain.setBounds (dialsR.removeFromLeft (92).reduced (4, 0));
    bandQ.setBounds (dialsR.removeFromLeft (92).reduced (4, 0));
}

void AvelTimeEditor::layoutModView (juce::Rectangle<int> d)
{
    auto in = d.reduced (16);
    const int gap = 14;
    const int srcH = juce::jmin (276, (in.getHeight() - 2 * gap) / 2 + 20);
    auto top = in.removeFromTop (srcH);
    const int cw = (top.getWidth() - 2 * gap) / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto c = top.removeFromLeft (cw);
        top.removeFromLeft (gap);
        srcCards[(size_t) i].setGlassBounds (c);
        auto cc = c.reduced (14, 10);
        cc.removeFromTop (34);
        auto prev = cc.removeFromTop (juce::jmax (60, cc.getHeight() - 116));
        cc.removeFromTop (10);
        if (i < numLfos)
        {
            lfoPreview[(size_t) i]->setBounds (prev);
            const int shapeW = juce::jmin (150, c.getWidth() - 100);
            lfoShape[(size_t) i].setBounds (juce::Rectangle<int> (shapeW, 26).withPosition (c.getRight() - 14 - shapeW, c.getY() + 12));
            auto ctrl = cc;
            lfoRate[(size_t) i].setBounds (ctrl.removeFromLeft (80).withTrimmedBottom (juce::jmax (0, ctrl.getHeight() - 104)));
            lfoPhase[(size_t) i].setBounds (ctrl.removeFromLeft (80).withTrimmedBottom (juce::jmax (0, ctrl.getHeight() - 104)));
            ctrl.removeFromLeft (8);
            auto col = ctrl.withTrimmedTop (16);
            const int cwid = juce::jmin (col.getWidth(), 130);
            auto r1 = col.removeFromTop (26).withWidth (cwid);
            lfoSync[(size_t) i]->setBounds (r1.withWidth (juce::jmin (cwid, lfoSync[(size_t) i]->idealWidth())));
            col.removeFromTop (12);
            lfoDiv[(size_t) i].setBounds (col.removeFromTop (26).withWidth (cwid));
        }
        else
        {
            envPreview.setBounds (prev);
            auto ctrl = cc;
            const int w3 = ctrl.getWidth() / 3;
            envAttack.setBounds (ctrl.removeFromLeft (w3).withTrimmedBottom (juce::jmax (0, ctrl.getHeight() - 104)));
            envRelease.setBounds (ctrl.removeFromLeft (w3).withTrimmedBottom (juce::jmax (0, ctrl.getHeight() - 104)));
            envSens.setBounds (ctrl.withTrimmedBottom (juce::jmax (0, ctrl.getHeight() - 104)));
        }
    }

    in.removeFromTop (gap);
    const int sh = (in.getHeight() - gap) / 2;
    for (int i = 0; i < numSlots; ++i)
    {
        const int rowI = i / 3, colI = i % 3;
        auto c = juce::Rectangle<int> (in.getX() + colI * (cw + gap), in.getY() + rowI * (sh + gap), cw, sh);
        slotCards[(size_t) i].setGlassBounds (c);
        auto cc = c.reduced (14, 10);
        auto amtR = cc.removeFromRight (82);
        slotAmt[(size_t) i].setBounds (amtR.withSizeKeepingCentre (80, juce::jmin (amtR.getHeight(), 100)));
        cc.removeFromTop (34);
        cc.removeFromRight (10);
        slotSrc[(size_t) i].setBounds (cc.removeFromTop (26));
        cc.removeFromTop (8);
        slotDst[(size_t) i].setBounds (cc.removeFromTop (26));
    }
}

//==================================================================================================
void AvelTimeEditor::updateDynamic()
{
    bindTimeDials();
    const bool sync = V (proc, pid::sync) > 0.5f;
    const bool link = V (proc, pid::link) > 0.5f;
    auto [tl, tr] = proc.resolvedTimesMs();
    timeR.setDimmed (link);
    timeR.setValueOverride (link ? std::function<juce::String()> ([this] { return timeL.getValueText(); }) : std::function<juce::String()>());
    if (sync)
    {
        timeAltL.setText (formatMs (tl));
        timeAltR.setText (formatMs (tr));
    }
    else
    {
        timeAltL.setText ({});
        timeAltR.setText ({});
    }
    tempoText.setText (juce::String (proc.getBpm(), std::abs (proc.getBpm() - std::round (proc.getBpm())) < 0.01 ? 0 : 1) + " BPM", sync ? col::label2 : col::label3);
    const int routing = (int) V (proc, pid::routing);
    cross.setDimmed (routing == 1);
    driveStyle.dimmed = V (proc, pid::drive) < 0.01f;
    diffSize.dimmed = V (proc, pid::diffusion) < 0.01f;
    duckRelease.dimmed = V (proc, pid::duck) < 0.01f;
    driveStyle.repaint(); diffSize.repaint(); duckRelease.repaint();

    const bool frozen = V (proc, pid::freeze) > 0.5f;
    freezeBtn.setToggled (frozen);
    abA.setToggled (proc.activeAB == 0);
    abB.setToggled (proc.activeAB == 1);
    undoBtn.setEnabledLook (proc.undo.canUndo());
    redoBtn.setEnabledLook (proc.undo.canRedo());
    if (presetPill.name != proc.presets.currentName) { presetPill.name = proc.presets.currentName; presetPill.repaint(); }

    // filters
    const auto t = (FilterType) (int) V (proc, pid::filt (band, "Type"));
    const bool gainType = t == FilterType::bell || t == FilterType::lowShelf || t == FilterType::highShelf;
    const bool bOn = V (proc, pid::filt (band, "On")) > 0.5f;
    bandGain.setDimmed (! gainType || ! bOn);
    bandFreq.setDimmed (! bOn);
    bandQ.setDimmed (! bOn);
    for (int i = 0; i < numFilters; ++i)
        bandBtns[(size_t) i].setTint (col::band (i).withAlpha (V (proc, pid::filt (i, "On")) > 0.5f ? 1.0f : 0.55f));

    // modulation
    for (int i = 0; i < numLfos; ++i)
    {
        const bool ls = V (proc, pid::lfo (i, "Sync")) > 0.5f;
        lfoRate[(size_t) i].setDimmed (ls);
        if (lfoDiv[(size_t) i].dimmed != ! ls) { lfoDiv[(size_t) i].dimmed = ! ls; lfoDiv[(size_t) i].repaint(); }
    }
    for (int i = 0; i < numSlots; ++i)
    {
        const bool active = (int) V (proc, pid::slot (i, "Src")) != 0 && (int) V (proc, pid::slot (i, "Dst")) != 0;
        slotAmt[(size_t) i].setDimmed (! active);
        const auto c = active ? product::accentA : col::label2;
        if (slotCards[(size_t) i].titleColour != c) { slotCards[(size_t) i].titleColour = c; slotCards[(size_t) i].repaint(); }
    }
}

void AvelTimeEditor::timerCallback()
{
    inMeter.set (proc.inPeak[0].exchange (0.0f), proc.inPeak[1].exchange (0.0f));
    outMeter.set (proc.outPeak[0].exchange (0.0f), proc.outPeak[1].exchange (0.0f));
    updateDynamic();
    if (view == 0) tapView.tick();
    else if (view == 1) filterView.tick();
    else
    {
        for (auto& l : lfoPreview) l->repaint();
        envPreview.tick();
    }
}

void AvelTimeEditor::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! tapTimes.empty() && now - tapTimes.back() > 2500.0) tapTimes.clear();
    tapTimes.push_back (now);
    if (tapTimes.size() > 5) tapTimes.erase (tapTimes.begin());
    if (tapTimes.size() < 2) return;
    const double avg = (tapTimes.back() - tapTimes.front()) / (double) (tapTimes.size() - 1);
    proc.undo.beginNewTransaction();
    auto set = [this] (const juce::String& id, float v)
    {
        auto& prm = P (proc, id);
        prm.beginChangeGesture();
        prm.setValueNotifyingHost (prm.convertTo0to1 (v));
        prm.endChangeGesture();
    };
    set (pid::sync, 0.0f);
    set (pid::timeL, (float) juce::jlimit (1.0, (double) maxDelayMs, avg));
}

void AvelTimeEditor::showPresetMenu()
{
    juce::PopupMenu m;
    const auto& f = PresetManager::factory();
    std::vector<juce::String> cats;
    for (auto& p : f) if (std::find (cats.begin(), cats.end(), p.category) == cats.end()) cats.push_back (p.category);
    for (auto& c : cats)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) f.size(); ++i)
            if (f[(size_t) i].category == c) sub.addItem (1 + i, f[(size_t) i].name, true, proc.presets.currentFactory == i);
        m.addSubMenu (c, sub);
    }
    const auto users = proc.presets.userPresets();
    m.addSeparator();
    if (users.isEmpty()) m.addItem (-1, "No user presets yet", false);
    else
    {
        m.addSectionHeader ("User");
        for (int i = 0; i < users.size(); ++i) m.addItem (1000 + i, users[i].getFileNameWithoutExtension(), true, users[i] == proc.presets.currentFile);
    }
    m.addSeparator();
    m.addItem (900, "Save Preset...");
    m.addItem (901, "Show Preset Folder");
    m.addItem (902, "Reset to Default");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetPill).withMinimumWidth (presetPill.getWidth()),
                     [this, users] (int r)
                     {
                         if (r <= 0) return;
                         if (r == 900) { savePresetDialog(); return; }
                         if (r == 901) { PresetManager::userFolder().createDirectory(); PresetManager::userFolder().startAsProcess(); return; }
                         proc.undo.beginNewTransaction();
                         if (r == 902) { proc.presets.loadFactory (-1); return; }
                         if (r >= 1000) proc.presets.loadUser (users[r - 1000]);
                         else proc.presets.loadFactory (r - 1);
                     });
}

void AvelTimeEditor::savePresetDialog()
{
    alert = std::make_unique<juce::AlertWindow> ("Save Preset", "Name for the new preset:", juce::MessageBoxIconType::NoIcon, this);
    alert->addTextEditor ("name", proc.presets.currentName == "Default" ? "My Preset" : proc.presets.currentName);
    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([this, sp = juce::Component::SafePointer<AvelTimeEditor> (this)] (int r)
    {
        if (r == 1 && alert != nullptr) proc.presets.saveUser (alert->getTextEditorContents ("name"));
        juce::MessageManager::callAsync ([sp] { if (sp) sp->alert.reset(); });
    }), false);
}
