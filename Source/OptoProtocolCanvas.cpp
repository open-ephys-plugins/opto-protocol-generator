/*
------------------------------------------------------------------

This file is part of a plugin for the Open Ephys GUI
Copyright (C) 2025 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "OptoProtocolCanvas.h"
#include "OptoProtocolGenerator.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <tuple>
#include <utility>
using namespace juce;

namespace
{
// Wave-player format (matches nidaq-test.py): sampleRate, maxVoltage, pulse/sine/custom with durations in source samples.
static const double kSourceSampleRate = 30000.0;
static const double kMaxVoltage = 5.0;
/** Pad NIDAQ buffer to this many AO channels so trial-to-trial config does not resize tasks. */
static const int kNidaqMinAnalogChannels = 1;

static int getPatternChannelFromRoot(const DynamicObject* root)
{
    if (root == nullptr)
        return 0;

    if (root->hasProperty("pulse"))
        return (int) root->getProperty("pulse").getProperty("analogOutputChannel", 0);
    if (root->hasProperty("sine"))
        return (int) root->getProperty("sine").getProperty("analogOutputChannel", 0);
    if (root->hasProperty("custom"))
        return (int) root->getProperty("custom").getProperty("analogOutputChannel", 0);

    return 0;
}

static int parseFirstIntegerAfterToken(const String& text, const String& token)
{
    const int tokenStart = text.indexOfIgnoreCase(token);
    if (tokenStart < 0)
        return 0;

    int i = tokenStart + token.length();
    while (i < text.length() && !CharacterFunctions::isDigit(text[i]))
        ++i;
    int value = 0;
    while (i < text.length() && CharacterFunctions::isDigit(text[i]))
    {
        value = value * 10 + (text[i] - '0');
        ++i;
    }
    return value;
}

static int lineFromCharacterOffset(const String& text, int characterOffset)
{
    if (characterOffset <= 0)
        return 0;
    int line = 1;
    const int maxChars = jmin(characterOffset, text.length());
    for (int i = 0; i < maxChars; ++i)
        if (text[i] == '\n')
            ++line;
    return line;
}

static String getLineText(const String& text, int lineNumber)
{
    if (lineNumber <= 0)
        return {};
    StringArray lines;
    lines.addLines(text);
    if (lineNumber > lines.size())
        return {};
    return lines[lineNumber - 1].trim();
}

static String getHardwareSchemaErrorMessage(const var& syntaxRoot)
{
    auto* rootObj = syntaxRoot.getDynamicObject();
    if (rootObj == nullptr)
        return "Root JSON object is missing.";

    auto* devices = rootObj->getProperty("devices").getArray();
    if (devices == nullptr || devices->isEmpty())
        return "Missing or empty 'devices' array.";

    for (int di = 0; di < devices->size(); ++di)
    {
        auto* dObj = (*devices)[di].getDynamicObject();
        if (dObj == nullptr)
            return "Device #" + String(di + 1) + " is not a JSON object.";

        const String deviceName = dObj->getProperty("name").toString();
        auto* lightSources = dObj->getProperty("light_sources").getArray();
        if (lightSources == nullptr || lightSources->isEmpty())
            return "Device '" + (deviceName.isNotEmpty() ? deviceName : ("#" + String(di + 1))) + "' has no light_sources.";

        for (int li = 0; li < lightSources->size(); ++li)
        {
            auto* lsObj = (*lightSources)[li].getDynamicObject();
            if (lsObj == nullptr)
                return "Device '" + (deviceName.isNotEmpty() ? deviceName : ("#" + String(di + 1))) + "', light source #" + String(li + 1) + " is not a JSON object.";

            const String lsName = lsObj->getProperty("name").toString();
            const String lsLabel = lsName.isNotEmpty() ? lsName : ("#" + String(li + 1));
            auto* inputVoltages = lsObj->getProperty("input_voltages").getArray();
            auto* outputPowers = lsObj->getProperty("output_powers").getArray();
            if (inputVoltages == nullptr || outputPowers == nullptr)
                return "Device '" + (deviceName.isNotEmpty() ? deviceName : ("#" + String(di + 1))) + "', light source '" + lsLabel + "': missing input_voltages or output_powers array.";

            if (inputVoltages->size() != outputPowers->size())
                return "Device '" + (deviceName.isNotEmpty() ? deviceName : ("#" + String(di + 1))) + "', light source '" + lsLabel + "': input voltage count does not match output power count (" + String(inputVoltages->size()) + " vs " + String(outputPowers->size()) + ").";
        }
    }

    return "JSON structure is invalid for hardware sources.";
}

class HardwareJsonEditorComponent : public Component, public Button::Listener, private Timer
{
public:
    HardwareJsonEditorComponent()
        : saveButton("Save"), resetButton("Reset")
    {
        editor.setMultiLine(true);
        editor.setReturnKeyStartsNewLine(true);
        editor.setScrollbarsShown(true);
        editor.setFont(Font(14.0f));
        editor.onTextChange = [this]() { updateLineNumbers(); };
        editor.setViewportIgnoreDragFlag(true);
        addAndMakeVisible(editor);

        lineNumbers.setMultiLine(true);
        lineNumbers.setReturnKeyStartsNewLine(true);
        lineNumbers.setReadOnly(true);
        lineNumbers.setScrollbarsShown(false);
        lineNumbers.setCaretVisible(false);
        lineNumbers.setPopupMenuEnabled(false);
        lineNumbers.setFont(Font(14.0f));
        lineNumbers.setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
        lineNumbers.setColour(TextEditor::outlineColourId, Colours::transparentBlack);
        lineNumbers.setColour(TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        addAndMakeVisible(lineNumbers);

        saveButton.addListener(this);
        resetButton.addListener(this);
        addAndMakeVisible(saveButton);
        addAndMakeVisible(resetButton);

        startTimerHz(30);
    }

    void setText(const String& text)
    {
        editor.setText(text, dontSendNotification);
        updateLineNumbers();
    }

    String getText() const
    {
        return editor.getText();
    }

    std::function<void()> onSave;
    std::function<void()> onReset;

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto buttons = area.removeFromBottom(30);
        saveButton.setBounds(buttons.removeFromRight(100));
        buttons.removeFromRight(8);
        resetButton.setBounds(buttons.removeFromRight(100));
        area.removeFromBottom(8);

        constexpr int gutterWidth = 56;
        auto gutter = area.removeFromLeft(gutterWidth);
        lineNumbers.setBounds(gutter);
        editor.setBounds(area);
        syncLineNumberViewport();
    }

    void buttonClicked(Button* button) override
    {
        if (button == &saveButton)
        {
            if (onSave) onSave();
            return;
        }
        if (button == &resetButton)
        {
            if (onReset) onReset();
        }
    }

private:
    static Viewport* findViewport(TextEditor& te)
    {
        for (int i = 0; i < te.getNumChildComponents(); ++i)
            if (auto* vp = dynamic_cast<Viewport*>(te.getChildComponent(i)))
                return vp;
        return nullptr;
    }

    void timerCallback() override
    {
        syncLineNumberViewport();
    }

    void syncLineNumberViewport()
    {
        auto* mainVp = findViewport(editor);
        auto* lineVp = findViewport(lineNumbers);
        if (mainVp == nullptr || lineVp == nullptr)
            return;
        const int y = mainVp->getViewPositionY();
        if (lineVp->getViewPositionY() != y)
            lineVp->setViewPosition(0, y);
    }

    void updateLineNumbers()
    {
        const int lineCount = jmax(1, editor.getText().containsChar('\n')
                                       ? StringArray::fromLines(editor.getText()).size()
                                       : 1);
        String text;
        for (int i = 1; i <= lineCount; ++i)
        {
            if (i > 1)
                text << "\n";
            text << String(i);
        }
        lineNumbers.setText(text, dontSendNotification);
        syncLineNumberViewport();
    }

    TextEditor editor;
    TextEditor lineNumbers;
    TextButton saveButton;
    TextButton resetButton;
};

class ValidatingCloseDialogWindow : public DialogWindow
{
public:
    ValidatingCloseDialogWindow(const String& title,
                                Colour backgroundColour,
                                std::function<bool()> validateClose_,
                                std::function<void()> onClosed_)
        : DialogWindow(title, backgroundColour, true),
          validateClose(std::move(validateClose_)),
          onClosed(std::move(onClosed_))
    {
    }

    void closeButtonPressed() override
    {
        if (!validateClose || validateClose())
        {
            exitModalState(0);
            setVisible(false);
            if (onClosed)
                onClosed();
        }
    }

private:
    std::function<bool()> validateClose;
    std::function<void()> onClosed;
};

static bool trialIndexToWavelengthNm(Sequence* seq, int trialIndex, int& outWl)
{
    int t = 0;
    const auto& blocks = seq->getTrialBlockOrder();
    for (size_t bi = 0; bi < blocks.size(); ++bi)
    {
        const int c = std::get<0>(blocks[bi]);
        const int nStim = jmax(1, seq->conditions[c]->stimuli.size());
        if (trialIndex < t + nStim)
        {
            const int widx = std::get<2>(blocks[bi]);
            Condition* cond = seq->conditions[c];
            if (widx >= 0 && widx < cond->availableWavelengths.size())
                outWl = cond->availableWavelengths[widx];
            else
                outWl = 0;
            return true;
        }
        t += nStim;
    }
    return false;
}

static void addStimulusToPattern(Stimulus* s, Condition* c, DynamicObject* patternRoot,
                                 const OptoHardwareConfig* hw, int wavelengthNm)
{
    int analogChannel = 0;
    float maxVoltage = (float) kMaxVoltage;
    const OptoHardwareLightSource* ls = nullptr;
    if (hw != nullptr && !hw->isEmpty())
    {
        // getFloatValue can synchronously run listeners that replace/reallocate the hardware
        // config; never hold a light-source pointer from findLightSource across that call.
        const float pulsePower = c->pulse_power.getFloatValue();
        const int di = jlimit(0, (int) hw->devices.size() - 1, c->source.getSelectedIndex());
        ls = hw->findLightSource(di, wavelengthNm);
        if (ls != nullptr)
        {
            const OptoHardwareLightSource& cal = *ls;
            analogChannel = cal.outputChannel;
            maxVoltage = OptoHardwareConfig::mapPowerToControlVoltage(pulsePower, cal);
        }
    }

    if (hw == nullptr || ls == nullptr)
    {
        analogChannel = 0;
        maxVoltage = (float) kMaxVoltage;
    }

    patternRoot->setProperty("maxVoltage", maxVoltage);

    if (s->type == PULSE_TRAIN)
    {
        PulseTrain* pt = static_cast<PulseTrain*>(s);
        float pwMs = pt->pulse_width.getFloatValue();
        float freqHz = pt->pulse_frequency.getFloatValue();
        int onDuration = (int)(pwMs / 1000.0f * kSourceSampleRate);
        if (onDuration < 1) onDuration = 1;
        float periodSec = freqHz > 0 ? 1.0f / freqHz : 0.1f;
        int periodSamples = (int)(periodSec * kSourceSampleRate);
        int offDuration = periodSamples - onDuration;
        if (offDuration < 1) offDuration = 1;
        DynamicObject::Ptr pulse = new DynamicObject();
        pulse->setProperty("analogOutputChannel", analogChannel);
        pulse->setProperty("onDuration", onDuration);
        pulse->setProperty("offDuration", offDuration);
        pulse->setProperty("delayDuration", 0);
        pulse->setProperty("repeatNumber", pt->pulse_count.getIntValue());
        int rampSamples = (int)(pt->ramp_duration.getFloatValue() / 1000.0f * kSourceSampleRate);
        pulse->setProperty("rampOnDuration", rampSamples);
        pulse->setProperty("rampOffDuration", rampSamples);
        pulse->setProperty("maxVoltage", maxVoltage);
        patternRoot->setProperty("pulse", var(pulse.get()));
    }
    else if (s->type == SINUSOID)
    {
        SineWave* sw = static_cast<SineWave*>(s);
        float durMs = sw->sine_wave_duration.getFloatValue();
        float freqHz = sw->sine_wave_frequency.getFloatValue();
        int cycles = jmax(1, (int)(durMs / 1000.0f * freqHz + 0.5f));
        DynamicObject::Ptr sine = new DynamicObject();
        sine->setProperty("analogOutputChannel", analogChannel);
        sine->setProperty("frequency", (double)freqHz);
        sine->setProperty("cycles", cycles);
        sine->setProperty("delayDuration", 0);
        sine->setProperty("maxVoltage", maxVoltage);
        patternRoot->setProperty("sine", var(sine.get()));
    }
    else if (s->type == RAMP)
    {
        RampStimulus* rs = static_cast<RampStimulus*>(s);
        float plateauSec = rs->plateau_duration.getFloatValue() / 1000.0f;
        int totalSamples = (int)(plateauSec * kSourceSampleRate);
        int rampOn = (int)(rs->ramp_onset_duration.getFloatValue() / 1000.0f * kSourceSampleRate);
        int rampOff = (int)(rs->ramp_offset_duration.getFloatValue() / 1000.0f * kSourceSampleRate);
        DynamicObject::Ptr pulse = new DynamicObject();
        pulse->setProperty("analogOutputChannel", analogChannel);
        pulse->setProperty("onDuration", jmax(1, totalSamples));
        pulse->setProperty("offDuration", 1);
        pulse->setProperty("delayDuration", 0);
        pulse->setProperty("repeatNumber", 1);
        pulse->setProperty("rampOnDuration", rampOn);
        pulse->setProperty("rampOffDuration", rampOff);
        pulse->setProperty("maxVoltage", maxVoltage);
        patternRoot->setProperty("pulse", var(pulse.get()));
    }
    else if (s->type == CUSTOM)
    {
        CustomStimulus* cs = static_cast<CustomStimulus*>(s);
        if (cs->stimulus_waveform.size() > 0)
        {
            String str;
            const float scale = maxVoltage > 0 ? (maxVoltage / (float) kMaxVoltage) : 1.f;
            for (int i = 0; i < cs->stimulus_waveform.size(); i++)
            {
                if (i > 0) str << ",";
                float val = jmax(0.0f, jmin(1.0f, cs->stimulus_waveform[i])) * (float) kMaxVoltage * scale;
                str << val;
            }
            DynamicObject::Ptr custom = new DynamicObject();
            custom->setProperty("analogOutputChannel", analogChannel);
            custom->setProperty("string", str);
            patternRoot->setProperty("custom", var(custom.get()));
        }
    }
}

/** Truncated normal in [lo, hi]: Box–Muller with rejection; σ = span/4. */
static float sampleNormalItiInRange(float lo, float hi, Random& rng)
{
    if (!(lo < hi)) return lo;
    const float mean = 0.5f * (lo + hi);
    const float sigma = (hi - lo) * 0.25f;
    for (int k = 0; k < 64; ++k)
    {
        float u1 = rng.nextFloat();
        if (u1 <= 0.f) u1 = 1.0e-30f;
        const float u2 = rng.nextFloat();
        const float z = std::sqrt(-2.f * std::log(u1)) * std::cos(2.f * MathConstants<float>::pi * u2);
        const float x = mean + z * sigma;
        if (x >= lo && x <= hi) return x;
    }
    return jlimit(lo, hi, mean);
}

/** Builds NIDAQ JSON for a single trial (one row); send at start of that trial. */
static String trialToNidaqJson(Sequence* seq, int trialIndex, const OptoHardwareConfig* hw)
{
    DynamicObject::Ptr root = new DynamicObject();
    root->setProperty("sampleRate", kSourceSampleRate);
    root->setProperty("maxVoltage", kMaxVoltage);
    root->setProperty("playImmediately", true);
    root->setProperty("minAnalogChannels", kNidaqMinAnalogChannels);
    if (seq->conditions.isEmpty())
        return JSON::toString(var(root.get()));
    if (trialIndex < 0 || trialIndex >= seq->getTotalTrials())
        return JSON::toString(var(root.get()));
    Stimulus* s = seq->getStimulusForTrial(trialIndex);
    if (!s || !s->condition)
        return JSON::toString(var(root.get()));
    int wlNm = 638;
    if (!trialIndexToWavelengthNm(seq, trialIndex, wlNm))
        wlNm = 638;
    addStimulusToPattern(s, s->condition, root.get(), hw, wlNm);
    const int requiredChannels = jmax(kNidaqMinAnalogChannels, getPatternChannelFromRoot(root.get()) + 1);
    root->setProperty("minAnalogChannels", requiredChannels);
    return JSON::toString(var(root.get()));
}
}

ColourSelectorWidget::ColourSelectorWidget(Condition* condition_, OptoProtocolInterface* parent_)
    : condition(condition_), parent(parent_)
{
    wavelengthLabel = std::make_unique<Label>("wavelengthLabel", "Wavelength");
    wavelengthLabel->setFont(FontOptions ("Inter", "Regular", 13.5));
    wavelengthLabel->setJustificationType(Justification::centredLeft);
    addAndMakeVisible(wavelengthLabel.get());
    wavelengthLabel->setBounds(90, 0, 100, 20);
    rebuildFromConfig();
}

void ColourSelectorWidget::rebuildFromConfig()
{
    for (auto& b : wavelengthButtons)
    {
        b->removeListener(this);
        removeChildComponent(b.get());
    }
    wavelengthButtons.clear();
    buttonWavelengths.clear();

    const auto* hw = parent != nullptr ? parent->getHardwareConfig() : nullptr;
    if (hw == nullptr || hw->isEmpty())
        return;

    const int di = jlimit(0, (int) hw->devices.size() - 1, condition->source.getSelectedIndex());
    const auto& dev = hw->devices[di];

    int idx450 = -1, idx638 = -1;
    for (int i = 0; i < dev.lightSources.size(); ++i)
    {
        const int w = dev.lightSources[i].wavelength;
        if (w == 450) idx450 = i;
        if (w == 638) idx638 = i;
    }

    if (dev.is_np_opto && idx450 >= 0 && idx638 >= 0)
        layoutNpOptoStyle();
    else
        layoutLinearButtons();
}

void ColourSelectorWidget::layoutNpOptoStyle()
{
    auto makeBtn = [this](int wl, bool onLeft)
    {
        auto b = std::make_unique<TextButton>("wl_" + String(wl));
        b->setButtonText(String(wl));
        b->setClickingTogglesState(true);
        b->setColour(TextButton::buttonColourId, Colours::darkgrey);
        if (wl == 450)
            b->setColour(TextButton::buttonOnColourId, Colour(38, 173, 252));
        else if (wl == 638)
            b->setColour(TextButton::buttonOnColourId, Colours::red);
        else
            b->setColour(TextButton::buttonOnColourId, Colours::green);
        b->setColour(TextButton::textColourOnId, Colours::white);
        b->setColour(TextButton::textColourOffId, Colours::white);
        b->addListener(this);
        addAndMakeVisible(*b);
        b->setBounds(onLeft ? 0 : 46, 0, 40, 20);
        wavelengthButtons.push_back(std::move(b));
        buttonWavelengths.push_back(wl);
    };

    makeBtn(450, true);
    makeBtn(638, false);
}

void ColourSelectorWidget::layoutLinearButtons()
{
    const auto* hw = parent->getHardwareConfig();
    const int di = jlimit(0, (int) hw->devices.size() - 1, condition->source.getSelectedIndex());
    const auto& dev = hw->devices[di];
    int x = 0;
    for (const auto& ls : dev.lightSources)
    {
        const int wl = ls.wavelength;
        auto b = std::make_unique<TextButton>("wl_" + String(wl));
        b->setButtonText(String(wl));
        b->setClickingTogglesState(true);
        b->setColour(TextButton::buttonColourId, Colours::darkgrey);
        if (wl == 450)
            b->setColour(TextButton::buttonOnColourId, Colour(38, 173, 252));
        else if (wl == 638)
            b->setColour(TextButton::buttonOnColourId, Colours::red);
        else if (wl == 520)
            b->setColour(TextButton::buttonOnColourId, Colours::green);
        else
            b->setColour(TextButton::buttonOnColourId, Colours::grey);
        b->setColour(TextButton::textColourOnId, Colours::white);
        b->setColour(TextButton::textColourOffId, Colours::white);
        b->addListener(this);
        addAndMakeVisible(*b);
        b->setBounds(x, 0, 40, 20);
        x += 46;
        wavelengthButtons.push_back(std::move(b));
        buttonWavelengths.push_back(wl);
    }
}

void ColourSelectorWidget::buttonClicked(Button* button)
{
    for (size_t i = 0; i < wavelengthButtons.size(); ++i)
    {
        if (wavelengthButtons[i].get() == button)
        {
            const int wl = buttonWavelengths[i];
            if (wavelengthButtons[i]->getToggleState())
                condition->addWavelength(wl);
            else
                condition->removeWavelength(wl);
            parent->parameterChangeRequest(nullptr);
            return;
        }
    }
}

void ColourSelectorWidget::enable()
{
    for (auto& b : wavelengthButtons)
        b->setEnabled(true);
    wavelengthLabel->setEnabled(true);
}

void ColourSelectorWidget::disable()
{
    for (auto& b : wavelengthButtons)
        b->setEnabled(false);
    wavelengthLabel->setEnabled(false);
}

void ColourSelectorWidget::syncFromCondition()
{
    for (size_t i = 0; i < wavelengthButtons.size(); ++i)
        wavelengthButtons[i]->setToggleState(condition->availableWavelengths.contains(buttonWavelengths[i]),
                                             dontSendNotification);
}

CustomStimulusInterface::CustomStimulusInterface(CustomStimulus* custom_stimulus_,
                                             OptoProtocolInterface* parent_)
    : custom_stimulus(custom_stimulus_), parent(parent_)
{
    
    sampleFrequencyEditor = std::make_unique<BoundedValueParameterEditor>(&custom_stimulus->sample_frequency);
    addAndMakeVisible(sampleFrequencyEditor.get());
    
    setBounds(0, 0, 0, 400);
}

void CustomStimulusInterface::resized()
{
    
    sampleFrequencyEditor->setBounds(0, 0, 150, 20);
    
}

void CustomStimulusInterface::enable()
{
    sampleFrequencyEditor->parameterEnabled(true);

}

void CustomStimulusInterface::disable()
{
    sampleFrequencyEditor->parameterEnabled(false);

}
    

PulseTrainInterface::PulseTrainInterface(PulseTrain* pulse_train_,
                                             OptoProtocolInterface* parent_)
    : pulse_train(pulse_train_), parent(parent_)
{
    
    pulseWidthEditor = std::make_unique<BoundedValueParameterEditor>(&pulse_train->pulse_width);
    addAndMakeVisible(pulseWidthEditor.get());
    pulseFrequencyEditor = std::make_unique<BoundedValueParameterEditor>(&pulse_train->pulse_frequency);
    addAndMakeVisible(pulseFrequencyEditor.get());
    pulseCountEditor = std::make_unique<BoundedValueParameterEditor>(&pulse_train->pulse_count);
    addAndMakeVisible(pulseCountEditor.get());
    rampDurationEditor = std::make_unique<BoundedValueParameterEditor>(&pulse_train->ramp_duration);
    addAndMakeVisible(rampDurationEditor.get());
    
    setBounds(0, 0, 0, 400);
}
    
    
void PulseTrainInterface::resized()
{
    
    pulseWidthEditor->setBounds(0, 0, 150, 20);
    pulseFrequencyEditor->setBounds(0, 30, 150, 20);
    pulseCountEditor->setBounds(0, 60, 150, 20);
    rampDurationEditor->setBounds(0, 90, 150, 20);
    
}

void PulseTrainInterface::enable()
{
    pulseWidthEditor->parameterEnabled(true);
    pulseFrequencyEditor->parameterEnabled(true);
    pulseCountEditor->parameterEnabled(true);
    rampDurationEditor->parameterEnabled(true);

}

void PulseTrainInterface::disable()
{
    pulseWidthEditor->parameterEnabled(false);
    pulseFrequencyEditor->parameterEnabled(false);
    pulseCountEditor->parameterEnabled(false);
    rampDurationEditor->parameterEnabled(false);

}


RampStimulusInterface::RampStimulusInterface(RampStimulus* ramp_stimulus_,
                                             OptoProtocolInterface* parent_)
    : ramp_stimulus(ramp_stimulus_), parent(parent_)
{
    
    plateauDurationEditor = std::make_unique<BoundedValueParameterEditor>(&ramp_stimulus->plateau_duration);
    addAndMakeVisible(plateauDurationEditor.get());
    onsetDurationEditor = std::make_unique<BoundedValueParameterEditor>(&ramp_stimulus->ramp_onset_duration);
    addAndMakeVisible(onsetDurationEditor.get());
    offsetDurationEditor = std::make_unique<BoundedValueParameterEditor>(&ramp_stimulus->ramp_offset_duration);
    addAndMakeVisible(offsetDurationEditor.get());
    profileEditor = std::make_unique<ComboBoxParameterEditor>(&ramp_stimulus->ramp_profile);
    addAndMakeVisible(profileEditor.get());
    
    setBounds(0, 0, 0, 400);
}
    
    
void RampStimulusInterface::resized()
{
    
    plateauDurationEditor->setBounds(0, 0, 150, 20);
    onsetDurationEditor->setBounds(0, 30, 150, 20);
    offsetDurationEditor->setBounds(0, 60, 150, 20);
    profileEditor->setBounds(0, 90, 150, 20);
    
}

void RampStimulusInterface::enable()
{
    plateauDurationEditor->parameterEnabled(true);
    onsetDurationEditor->parameterEnabled(true);
    offsetDurationEditor->parameterEnabled(true);
    profileEditor->parameterEnabled(true);

}

void RampStimulusInterface::disable()
{
    plateauDurationEditor->parameterEnabled(false);
    onsetDurationEditor->parameterEnabled(false);
    offsetDurationEditor->parameterEnabled(false);
    profileEditor->parameterEnabled(false);

}


SineWaveInterface::SineWaveInterface(SineWave* sine_wave_,
                                             OptoProtocolInterface* parent_)
    : sine_wave(sine_wave_), parent(parent_)
{
    
    durationEditor = std::make_unique<BoundedValueParameterEditor>(&sine_wave->sine_wave_duration);
    addAndMakeVisible(durationEditor.get());
    frequencyEditor = std::make_unique<BoundedValueParameterEditor>(&sine_wave->sine_wave_frequency);
    addAndMakeVisible(frequencyEditor.get());
    
    setBounds(0, 0, 0, 400);
}
    
    
void SineWaveInterface::resized()
{
    
    durationEditor->setBounds(0, 0, 150, 20);
    frequencyEditor->setBounds(0, 30, 150, 20);
    
}

void SineWaveInterface::enable()
{
    durationEditor->parameterEnabled(true);
    frequencyEditor->parameterEnabled(true);

}

void SineWaveInterface::disable()
{
    durationEditor->parameterEnabled(false);
    frequencyEditor->parameterEnabled(false);

}

RemoveConditionButton::RemoveConditionButton()
    : DrawableButton("deleteButton", DrawableButton::ImageFitted)
{
    
    Path xPath;
    xPath.startNewSubPath(0, 0);
    xPath.lineTo(10, 10);
    xPath.startNewSubPath(10, 0);
    xPath.lineTo(0, 10);
    
    normalDrawable.setPath(xPath);
    normalDrawable.setStrokeFill(findColour(ThemeColours::defaultText).withAlpha(0.5f));
    normalDrawable.setStrokeType(PathStrokeType(2.0f));
    
    overDrawable.setPath(xPath);
    overDrawable.setStrokeFill(findColour(ThemeColours::defaultText).withAlpha(0.3f));
    overDrawable.setStrokeType(PathStrokeType(2.0f));
    
    setImages(&normalDrawable, &overDrawable, nullptr, nullptr, nullptr);
}

void RemoveConditionButton::colourChanged()
{
    normalDrawable.setStrokeFill(findColour(ThemeColours::defaultText).withAlpha(0.5f));
    overDrawable.setStrokeFill(findColour(ThemeColours::defaultText).withAlpha(0.3f));
    
    setImages(&normalDrawable, &overDrawable, nullptr, nullptr, nullptr);
}

OptoConditionInterface::OptoConditionInterface(Condition* condition_, Stimulus* stimulus_,
                                             OptoProtocolInterface* parent_)
    : condition(condition_), stimulus(stimulus_), parent(parent_)
{
    if (condition == nullptr || stimulus == nullptr)
    {
        jassertfalse;
        return;
    }
    
    sourceEditor = std::make_unique<ComboBoxParameterEditor>(&condition->source);
    addAndMakeVisible(sourceEditor.get());
    loadJsonButton = std::make_unique<TextButton>("loadJson");
    loadJsonButton->setButtonText("Load Sources");
    loadJsonButton->onClick = [this]()
    {
        if (parent != nullptr)
            parent->launchLoadHardwareJsonChooser();
    };
    addAndMakeVisible(loadJsonButton.get());
    siteEditor = std::make_unique<SelectedChannelsParameterEditor>(condition->sites.get());
    addAndMakeVisible(siteEditor.get());
    colourSelectorWidget = std::make_unique<ColourSelectorWidget>(condition, parent);
    colourSelectorWidget->rebuildFromConfig();
    colourSelectorWidget->syncFromCondition();
    addAndMakeVisible(colourSelectorWidget.get());
    pulsePowerEditor = std::make_unique<BoundedValueParameterEditor>(&condition->pulse_power);
    addAndMakeVisible(pulsePowerEditor.get());
    numRepeatsEditor = std::make_unique<BoundedValueParameterEditor>(&condition->num_repeats);
    addAndMakeVisible(numRepeatsEditor.get());
    
    if (stimulus->type == StimulusType::PULSE_TRAIN)
    {
        stimulusTypeLabel = std::make_unique<Label>("stimulusTypeLabel", "Pulse train");
        pulseTrainInterface = std::make_unique<PulseTrainInterface>((PulseTrain*) stimulus, parent);
        addAndMakeVisible(pulseTrainInterface.get());
    } else if (stimulus->type == StimulusType::SINUSOID) {
        stimulusTypeLabel = std::make_unique<Label>("stimulusTypeLabel", "Sine wave");
        sineWaveInterface = std::make_unique<SineWaveInterface>((SineWave*) stimulus, parent);
        addAndMakeVisible(sineWaveInterface.get());
    } else if (stimulus->type == StimulusType::RAMP) {
        stimulusTypeLabel = std::make_unique<Label>("stimulusTypeLabel", "Ramp");
        rampStimulusInterface = std::make_unique<RampStimulusInterface>((RampStimulus*) stimulus, parent);
        addAndMakeVisible(rampStimulusInterface.get());
    } else if (stimulus->type == StimulusType::CUSTOM) {
        stimulusTypeLabel = std::make_unique<Label>("stimulusTypeLabel", "Custom");
        customStimulusInterface = std::make_unique<CustomStimulusInterface>((CustomStimulus*) stimulus, parent);
        addAndMakeVisible(customStimulusInterface.get());
    }
    
    stimulusTypeLabel->setFont(FontOptions ("Inter", "Regular", 17));
    stimulusTypeLabel->setJustificationType(Justification::centredLeft);
    addAndMakeVisible(stimulusTypeLabel.get());

    // DrawableButton for delete (X)
    deleteButton = std::make_unique<RemoveConditionButton>();
    deleteButton->setTooltip("Delete this condition");
    deleteButton->onClick = [this]() { requestDelete(); };
    addAndMakeVisible(deleteButton.get());
    
    setBounds(0, 0, 0, 400);
    refreshSourceLoadUi();
}
    

OptoConditionInterface::~OptoConditionInterface()
{
}

void OptoConditionInterface::refreshSourceLoadUi()
{
    const bool hw = parent != nullptr && parent->hasHardwareConfig();
    if (sourceEditor != nullptr)
    {
        sourceEditor->setVisible(hw);
        sourceEditor->setInterceptsMouseClicks(hw, hw);
    }
    if (loadJsonButton != nullptr)
    {
        loadJsonButton->setVisible(!hw);
        loadJsonButton->setInterceptsMouseClicks(!hw, !hw);
    }
    if (siteEditor != nullptr)
    {
        siteEditor->setVisible(hw);
        siteEditor->setInterceptsMouseClicks(hw, hw);
    }
    if (colourSelectorWidget != nullptr)
    {
        colourSelectorWidget->setVisible(hw);
        colourSelectorWidget->setInterceptsMouseClicks(hw, hw);
    }
    if (pulsePowerEditor != nullptr)
    {
        pulsePowerEditor->setVisible(hw);
        pulsePowerEditor->setInterceptsMouseClicks(hw, hw);
    }
}

void OptoConditionInterface::onHardwareConfigChanged()
{
    refreshSourceLoadUi();
    // CategoricalParameter::setCategories() does not notify listeners when the
    // selected index stays in range, so the ComboBox keeps stale/empty item text.
    if (parent != nullptr && parent->hasHardwareConfig())
    {
        if (sourceEditor != nullptr)
            sourceEditor->updateView();
        if (siteEditor != nullptr)
            siteEditor->updateView();
    }
    if (colourSelectorWidget != nullptr)
    {
        colourSelectorWidget->rebuildFromConfig();
        colourSelectorWidget->syncFromCondition();
    }
    resized();
}
    
void OptoConditionInterface::resized()
{
    stimulusTypeLabel->setBounds(12, 12, 100, 20);
    sourceEditor->setBounds(190, 15, 180, 20);
    if (loadJsonButton != nullptr)
        loadJsonButton->setBounds(190, 15, 112, 20);
    colourSelectorWidget->setBounds(15, 50, 180, 20);
    siteEditor->setBounds(15, 80, 150, 20);
    pulsePowerEditor->setBounds(15, 110, 150, 20);
    numRepeatsEditor->setBounds(15, 140, 150, 20);
    
    if (pulseTrainInterface.get() != nullptr)
        pulseTrainInterface->setBounds(190, 55, getWidth()-190, getHeight()-55);
    
    if (sineWaveInterface.get() != nullptr)
        sineWaveInterface->setBounds(190, 55, getWidth()-190, getHeight()-55);
    
    if (rampStimulusInterface.get() != nullptr)
        rampStimulusInterface->setBounds(190, 55, getWidth()-190, getHeight()-55);

    // Place delete button in top-right corner
    if (deleteButton)
        deleteButton->setBounds(getWidth() - 20, 4, 16, 16);
    
}

void OptoConditionInterface::requestDelete()
{
    if (parent)
        parent->removeConditionInterface(this);
}

void OptoConditionInterface::enable()
{
    const bool hw = parent != nullptr && parent->hasHardwareConfig();
    sourceEditor->parameterEnabled(hw);
    if (loadJsonButton != nullptr)
        loadJsonButton->setEnabled(!hw);
    siteEditor->parameterEnabled(hw);
    pulsePowerEditor->parameterEnabled(hw);
    numRepeatsEditor->parameterEnabled(true);
    
    if (hw)
        colourSelectorWidget->enable();
    else
        colourSelectorWidget->disable();
    
    if (pulseTrainInterface.get() != nullptr)
        pulseTrainInterface->enable();
    
    if (sineWaveInterface.get() != nullptr)
        sineWaveInterface->enable();
    
    if (rampStimulusInterface.get() != nullptr)
        rampStimulusInterface->enable();
}

void OptoConditionInterface::disable()
{
    
    LOGD("Disabling OptoConditionInterface");
    
    sourceEditor->parameterEnabled(false);
    if (loadJsonButton != nullptr)
        loadJsonButton->setEnabled(false);
    siteEditor->parameterEnabled(false);
    pulsePowerEditor->parameterEnabled(false);
    numRepeatsEditor->parameterEnabled(false);
    
    colourSelectorWidget->disable();
    
    if (pulseTrainInterface.get() != nullptr)
        pulseTrainInterface->disable();
    
    if (sineWaveInterface.get() != nullptr)
        sineWaveInterface->disable();
    
    if (rampStimulusInterface.get() != nullptr)
        rampStimulusInterface->disable();
}
    
void OptoConditionInterface::paint(Graphics& g)
{
    g.setColour(findColour(ThemeColours::defaultText).withAlpha(0.5f));
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), 7);
    g.setColour(findColour(ThemeColours::widgetBackground).withAlpha(0.8f));
    g.fillRoundedRectangle(2, 2, getWidth()-4, getHeight()-4, 5);
}


OptoSequenceInterface::OptoSequenceInterface(const String& name,
                                             Sequence* sequence_,
                                             OptoProtocolInterface* parent_,
                                             bool skipDefaultCondition)
    : sequence(sequence_), parent(parent_)
{
    
    sequenceNameLabel = std::make_unique<Label>("sequenceLabel", name);
    sequenceNameLabel->setFont(FontOptions ("Inter", "Regular", 15));
    sequenceNameLabel->setJustificationType(Justification::centredLeft);
    addAndMakeVisible(sequenceNameLabel.get());
    
    addConditionButton = std::make_unique<TextButton>("addConditionButton");
    addConditionButton->setButtonText("Add Condition");
    addConditionButton->addListener(this);
    addAndMakeVisible(addConditionButton.get());
    
    deleteSequenceButton = std::make_unique<TextButton>("deleteSequenceButton");
    deleteSequenceButton->setButtonText("Delete sequence");
    deleteSequenceButton->addListener(this);
    addAndMakeVisible(deleteSequenceButton.get());
    if (parent != nullptr)
        deleteSequenceButton->setVisible(parent->getNumSequenceInterfaces() > 1);
    
    if (!skipDefaultCondition)
    {
        Array<String> availableSources;
        Array<int> sitesPerSource;
        Array<int> availableWavelengths;
        if (parent != nullptr)
            parent->getNewConditionArrays(availableSources, sitesPerSource, availableWavelengths);

        Condition* condition = new Condition(parent, availableSources,
                                             sitesPerSource,
                                             availableWavelengths,
                                             sequence);
        
        sequence->addCondition(condition);
        
        PulseTrain* pulseTrain = new PulseTrain(parent, condition);
        condition->addStimulus(pulseTrain);
        
        conditionInterfaces.add(new OptoConditionInterface(condition, pulseTrain, parent));
        addAndMakeVisible(conditionInterfaces.getLast());
    }
    
    baselineIntervalEditor = std::make_unique<BoundedValueParameterEditor>(&sequence->baseline_interval);
    addAndMakeVisible(baselineIntervalEditor.get());
    minItiEditor = std::make_unique<BoundedValueParameterEditor>(&sequence->min_iti);
    addAndMakeVisible(minItiEditor.get());
    maxItiEditor = std::make_unique<BoundedValueParameterEditor>(&sequence->max_iti);
    addAndMakeVisible(maxItiEditor.get());
    randomizeEditor = std::make_unique<ToggleParameterEditor>(&sequence->randomize);
    addAndMakeVisible(randomizeEditor.get());
    
    int h = 230;
    for (int i = 0; i < conditionInterfaces.size(); ++i)
        h += (i == 0 ? conditionInterfaceHeight : 10 + conditionInterfaceHeight);
    setBounds(0, 0, 0, h);
}
    

OptoSequenceInterface::~OptoSequenceInterface()
{
    
}
    
void OptoSequenceInterface::resized()
{
    
    int leftMargin = 15;
    
    sequenceNameLabel->setBounds(leftMargin-5, 20, 140, 20);
    
    baselineIntervalEditor->setBounds(leftMargin, 50, 150, 20);
    minItiEditor->setBounds(leftMargin, 80, 150, 20);
    maxItiEditor->setBounds(leftMargin, 110, 150, 20);
    randomizeEditor->setBounds(leftMargin, 140, 150, 20);
    
    int currentHeight = 180;
    LOGD("OptoSequenceInterface::resized()");
    LOGD("Starting height: ", currentHeight);
    LOGD("Num condition interfaces: ", conditionInterfaces.size());
    for (auto interface : conditionInterfaces)
    {
        interface->setBounds(15, currentHeight, conditionInterfaceWidth, conditionInterfaceHeight);
        currentHeight += conditionInterfaceHeight + 10;
    }
    LOGD("New current height: ", currentHeight);
    addConditionButton->setBounds(265, currentHeight+6, 100, 20);
    if (deleteSequenceButton)
    {
        deleteSequenceButton->setBounds(leftMargin, currentHeight+6, 105, 22);
        if (parent != nullptr)
            deleteSequenceButton->setVisible(parent->getNumSequenceInterfaces() > 1);
    }
}
    
void OptoSequenceInterface::paint(Graphics& g)
{
    g.setColour(findColour(ThemeColours::defaultText));
    g.drawLine(93, 31, 385, 31, 1.0f);
    g.drawLine(15, getHeight()-5, 385, getHeight()-5, 1.0f);

}

void OptoSequenceInterface::enable()
{
    baselineIntervalEditor->setEnabled(true);
    minItiEditor->setEnabled(true);
    maxItiEditor->setEnabled(true);
    randomizeEditor->setEnabled(true);
    
    for (auto condition : conditionInterfaces)
    {
        condition->enable();
    }
    
    addConditionButton->setEnabled(true);
    if (deleteSequenceButton && parent != nullptr)
        deleteSequenceButton->setVisible(parent->getNumSequenceInterfaces() > 1);
    if (deleteSequenceButton)
        deleteSequenceButton->setEnabled(true);
}

void OptoSequenceInterface::disable()
{
    
    LOGD("Disabling OptoSequenceInterface");
    
    baselineIntervalEditor->setEnabled(false);
    minItiEditor->setEnabled(false);
    maxItiEditor->setEnabled(false);
    randomizeEditor->setEnabled(false);
    
    for (auto condition : conditionInterfaces)
    {
        condition->disable();
    }
    
    addConditionButton->setEnabled(false);
    if (deleteSequenceButton)
        deleteSequenceButton->setEnabled(false);
}

bool OptoSequenceInterface::removeCondition(OptoConditionInterface* conditionInterface)
{
    if (conditionInterfaces.contains(conditionInterface))
    {
        LOGD("Removing condition interface.");
        LOGD("Number of condition interfaces: ", conditionInterfaces.size());
        sequence->removeCondition(conditionInterface->getCondition());
        conditionInterfaces.removeObject(conditionInterface, true);
        LOGD("New number of condition interfaces: ", conditionInterfaces.size());
        int numInterfaces = conditionInterfaces.size();
        setBounds(0,0,0,230 + (10 + conditionInterfaceHeight) * numInterfaces);
        return true;
    } else {
        LOGD("Condition interface not found in this sequence.");
        return false;
    }
    
}

void OptoSequenceInterface::buttonClicked(Button* button)
{
    if (button == deleteSequenceButton.get())
    {
        if (parent != nullptr)
            parent->removeSequenceInterface(this);
        return;
    }
    if (button == addConditionButton.get())
    {
        LOGD("Add condition button clicked.");
        PopupMenu m;
        m.setLookAndFeel(&getLookAndFeel());
        m.addItem(1, "Pulse Train", true);
        m.addItem(2, "Sine Wave", true);
        m.addItem(3, "Ramp", true);
        m.addItem(4, "Custom", true);
        const int result = m.showMenu(PopupMenu::Options{}.withStandardItemHeight(20));
        if (result == 0)
            return;
        Array<String> availableSources;
        Array<int> sitesPerSource;
        Array<int> availableWavelengths;
        if (parent != nullptr)
            parent->getNewConditionArrays(availableSources, sitesPerSource, availableWavelengths);
        Condition* condition = new Condition(parent, availableSources, sitesPerSource, availableWavelengths, sequence);
        sequence->addCondition(condition);
        if (result == 1)
        {
            PulseTrain* pulseTrain = new PulseTrain(parent, condition);
            condition->addStimulus(pulseTrain);
            conditionInterfaces.add(new OptoConditionInterface(condition, pulseTrain, parent));
        }
        else if (result == 2)
        {
            SineWave* sineWave = new SineWave(parent, condition);
            condition->addStimulus(sineWave);
            conditionInterfaces.add(new OptoConditionInterface(condition, sineWave, parent));
        }
        else if (result == 3)
        {
            RampStimulus* rampStimulus = new RampStimulus(parent, condition);
            condition->addStimulus(rampStimulus);
            conditionInterfaces.add(new OptoConditionInterface(condition, rampStimulus, parent));
        }
        else if (result == 4)
        {
            CustomStimulus* customStimulus = new CustomStimulus(parent, condition);
            condition->addStimulus(customStimulus);
            conditionInterfaces.add(new OptoConditionInterface(condition, customStimulus, parent));
        }
        addAndMakeVisible(conditionInterfaces.getLast());
        int numInterfaces = conditionInterfaces.size();
        setBounds(0, 0, 0, 230 + (10 + conditionInterfaceHeight) * numInterfaces);
        parent->resized();
        parent->updateBounds(conditionInterfaceHeight - 20);
        parent->refreshConditionsTable();
        parent->timeline->setTotalTime(parent->getTableTotalDuration());
        parent->timeline->setTotalTrials(sequence->protocol->getTotalTrials());
    }
}

enum ConditionsTableColumns { ColRow = 1, ColSequence, ColCondition, ColProbe, ColWavelength, ColSites, ColLightPower, ColITI, ColStartTime, ColEndTime, ColRepeat };

String ConditionsTableModel::getStructureSignature() const
{
    if (!protocol) return {};
    String s;
    for (auto* seq : protocol->sequences)
    {
        for (auto* cond : seq->conditions)
        {
            s << cond->num_repeats.getIntValue() << ",";
            for (auto wl : cond->availableWavelengths)
                s << wl << ",";
            s << ";";
            if (cond->sites)
                for (auto& v : cond->sites->getArrayValue())
                    s << (int)v << ",";
            s << "|";
        }
        s << "b" << seq->baseline_interval.getFloatValue() << ";";
        s << (seq->randomize.getBoolValue() ? "1" : "0") << ";";
        s << seq->min_iti.getFloatValue() << "," << seq->max_iti.getFloatValue() << "!";
        for (const auto& b : seq->getTrialBlockOrder())
            s << std::get<0>(b) << ',' << std::get<1>(b) << ',' << std::get<2>(b) << ',' << std::get<3>(b) << ';';
        s << "||";
    }
    return s;
}

void ConditionsTableModel::rebuildRowOrder()
{
    rowOrder.clear();
    rowIti.clear();
    if (!protocol) return;
    for (int s = 0; s < protocol->sequences.size(); ++s)
    {
        Sequence* seq = protocol->sequences[s];
        const float minITI = seq->min_iti.getFloatValue();
        const float maxITI = seq->max_iti.getFloatValue();
        auto& rng = Random::getSystemRandom();
        if (seq->baseline_interval.getFloatValue() > 0)
        {
            rowOrder.add(std::make_tuple(s + 1, 0, 0, 0, 0));
            rowIti.add(0.f);
        }
        for (const auto& b : seq->getTrialBlockOrder())
        {
            rowOrder.add(std::make_tuple(s + 1, std::get<0>(b) + 1, std::get<1>(b) + 1, std::get<2>(b), std::get<3>(b)));
            rowIti.add(sampleNormalItiInRange(minITI, maxITI, rng));
        }
    }
}

void ConditionsTableModel::rowToIndices(int row, int& seqIdx, int& condIdx, int& repeatIdx, int& wavelengthIdx, int& siteIdx) const
{
    if (row < 0 || row >= rowOrder.size()) { seqIdx = condIdx = repeatIdx = wavelengthIdx = siteIdx = 0; return; }
    const auto& t = rowOrder[row];
    seqIdx = std::get<0>(t);
    condIdx = std::get<1>(t);
    repeatIdx = std::get<2>(t);
    wavelengthIdx = std::get<3>(t);
    siteIdx = std::get<4>(t);
}

String ConditionsTableModel::getConditionName(int seqIdx, int condIdx) const
{
    if (condIdx == 0) return "None";
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return "Condition";
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (condIdx < 1 || condIdx > seq->conditions.size()) return "Condition";
    Condition* cond = seq->conditions[condIdx - 1];
    if (cond->stimuli.isEmpty()) return "Condition";
    switch (cond->stimuli[0]->type)
    {
        case StimulusType::PULSE_TRAIN: return "Pulse train";
        case StimulusType::SINUSOID: return "Sine wave";
        case StimulusType::RAMP: return "Ramp";
        case StimulusType::CUSTOM: return "Custom";
        default: return "Condition";
    }
}

String ConditionsTableModel::getProbeName(int seqIdx, int condIdx) const
{
    if (condIdx == 0) return "None";
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return {};
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (condIdx < 1 || condIdx > seq->conditions.size()) return {};
    return seq->conditions[condIdx - 1]->source.getValueAsString();
}

String ConditionsTableModel::getWavelengthString(int seqIdx, int condIdx, int wavelengthIdx) const
{
    if (condIdx == 0) return "None";
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return {};
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (condIdx < 1 || condIdx > seq->conditions.size()) return {};
    const auto& wl = seq->conditions[condIdx - 1]->availableWavelengths;
    if (wl.isEmpty() || wavelengthIdx < 0 || wavelengthIdx >= wl.size()) return {};
    return String(wl[wavelengthIdx]);
}

String ConditionsTableModel::getSitesString(int seqIdx, int condIdx, int siteIdx) const
{
    if (condIdx == 0) return "None";
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return {};
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (condIdx < 1 || condIdx > seq->conditions.size() || !seq->conditions[condIdx - 1]->sites) return {};
    Condition* cond = seq->conditions[condIdx - 1];
    const auto& arr = cond->sites->getArrayValue();
    if (siteIdx < 0 || siteIdx >= arr.size()) return {};
    return String((int)arr[siteIdx] + 1);
}

String ConditionsTableModel::getLightPowerString(int seqIdx, int condIdx) const
{
    if (condIdx == 0) return "None";
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return {};
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (condIdx < 1 || condIdx > seq->conditions.size()) return {};
    return seq->conditions[condIdx - 1]->pulse_power.getValueAsString();
}

String ConditionsTableModel::getTrialString(int row) const
{
    if (!protocol || row < 0 || row >= rowOrder.size()) return {};
    int seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx;
    rowToIndices(row, seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx);
    if (condIdx == 0) return "None";
    int trial = 0;
    for (int r = 0; r <= row; ++r)
    {
        if (std::get<0>(rowOrder[r]) != seqIdx) continue;
        if (std::get<1>(rowOrder[r]) != 0) ++trial;
    }
    return String(trial);
}

String ConditionsTableModel::getITIString(int row) const
{
    if (!protocol || row < 0 || row >= rowIti.size()) return {};
    const float iti = rowIti.getUnchecked(row);
    return String(iti, (iti >= 10.f || iti == (int)iti) ? 0 : 1) + "s";
}

static float formatTimeSec(float t) { return (int)(t * 100.0f + 0.5f) / 100.0f; }

String ConditionsTableModel::getStartTimeString(int row) const
{
    if (!protocol || row < 0 || row >= rowOrder.size()) return {};
    int seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx;
    rowToIndices(row, seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx);
    Sequence* seq = protocol->sequences[seqIdx - 1];
    float timeBeforeSeq = 0.f;
    for (int s = 1; s < seqIdx; ++s)
        timeBeforeSeq += getSequenceTableDuration(s);
    int pos = 0;
    for (int r = 0; r < row; ++r)
    {
        int s, c, p, w, st;
        rowToIndices(r, s, c, p, w, st);
        if (s == seqIdx) ++pos;
    }
    const float baseline = seq->baseline_interval.getFloatValue();
    float cumul = 0.f;
    int idx = 0;
    for (int r = 0; r < rowOrder.size(); ++r)
    {
        int s, c, p, w, st;
        rowToIndices(r, s, c, p, w, st);
        if (s != seqIdx) continue;
        const float blockDur = computeRowBlockDuration(r);
        if (idx == pos)
            return String(formatTimeSec(timeBeforeSeq + (c == 0 ? 0.f : cumul))) + "s";
        cumul += blockDur;
        ++idx;
    }
    return {};
}

String ConditionsTableModel::getEndTimeString(int row) const
{
    if (!protocol || row < 0 || row >= rowOrder.size()) return {};
    int seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx;
    rowToIndices(row, seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx);
    Sequence* seq = protocol->sequences[seqIdx - 1];
    float timeBeforeSeq = 0.f;
    for (int s = 1; s < seqIdx; ++s)
        timeBeforeSeq += getSequenceTableDuration(s);
    int pos = 0;
    for (int r = 0; r < row; ++r)
    {
        int s, c, p, w, st;
        rowToIndices(r, s, c, p, w, st);
        if (s == seqIdx) ++pos;
    }
    const float baseline = seq->baseline_interval.getFloatValue();
    float cumul = 0.f;
    int idx = 0;
    for (int r = 0; r < rowOrder.size(); ++r)
    {
        int s, c, p, w, st;
        rowToIndices(r, s, c, p, w, st);
        if (s != seqIdx) continue;
        const float blockDur = computeRowBlockDuration(r);
        if (idx == pos)
            return String(formatTimeSec(timeBeforeSeq + (c == 0 ? baseline : cumul + blockDur))) + "s";
        cumul += blockDur;
        ++idx;
    }
    return {};
}

int ConditionsTableModel::getRowIndexForSequenceAndTrial(int seqIdx, int trialNum) const
{
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return -1;
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (trialNum == 0)
    {
        for (int r = 0; r < rowOrder.size(); ++r)
        {
            if (std::get<0>(rowOrder[r]) != seqIdx) continue;
            if (std::get<1>(rowOrder[r]) == 0) return r;
        }
        return -1;
    }
    int count = 0;
    for (int r = 0; r < rowOrder.size(); ++r)
    {
        if (std::get<0>(rowOrder[r]) != seqIdx) continue;
        int s, c, p, w, st;
        rowToIndices(r, s, c, p, w, st);
        if (c == 0) continue;
        int n = (int)seq->conditions[c - 1]->stimuli.size();
        count += n;
        if (count >= trialNum) return r;
    }
    return -1;
}

void ConditionsTableModel::ensureRowOrderCurrent()
{
    if (!protocol) return;
    String sig = getStructureSignature();
    if (sig != lastStructureSignature)
    {
        lastStructureSignature = sig;
        rebuildRowOrder();
    }
}

int ConditionsTableModel::getNumRows()
{
    if (!protocol) return 0;
    ensureRowOrderCurrent();
    return rowOrder.size();
}

float ConditionsTableModel::computeRowBlockDuration(int row) const
{
    if (!protocol || row < 0 || row >= rowOrder.size()) return 0.f;
    int seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx;
    rowToIndices(row, seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx);
    Sequence* seq = protocol->sequences[seqIdx - 1];
    const float baseline = seq->baseline_interval.getFloatValue();
    if (condIdx == 0)
        return baseline;
    Condition* cond = seq->conditions[condIdx - 1];
    const int n = (int)cond->stimuli.size();
    float stimT = 0.f;
    for (auto* st : cond->stimuli) stimT += st->getTotalTime();
    const float iti = rowIti.getUnchecked(row);
    return stimT + (float)n * iti;
}

float ConditionsTableModel::getSequenceTableDuration(int seqIdx) const
{
    float sum = 0.f;
    for (int r = 0; r < rowOrder.size(); ++r)
    {
        if (std::get<0>(rowOrder[r]) != seqIdx)
            continue;
        sum += computeRowBlockDuration(r);
    }
    return sum;
}

float ConditionsTableModel::getTotalProtocolDuration()
{
    if (!protocol) return 0.f;
    ensureRowOrderCurrent();
    float total = 0.f;
    for (int r = 0; r < rowOrder.size(); ++r)
        total += computeRowBlockDuration(r);
    return total;
}

void ConditionsTableModel::paintRowBackground(Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (isRunning && rowNumber == activeRow)
    {
        int seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx;
        rowToIndices(rowNumber, seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx);
        Colour c;
        if (condIdx == 0)
            c = Colours::orange;
        else if (protocol && seqIdx >= 1 && seqIdx <= protocol->sequences.size())
        {
            const auto& wl = protocol->sequences[seqIdx - 1]->conditions[condIdx - 1]->availableWavelengths;
            if (wavelengthIdx >= 0 && wavelengthIdx < wl.size() && wl[wavelengthIdx] == 450)
                c = Colours::blue;
            else if (wavelengthIdx >= 0 && wavelengthIdx < wl.size() && wl[wavelengthIdx] == 638)
                c = Colours::red;
            else
                c = Colours::green;
        }
        else
            c = Colours::green;
        g.fillAll(c.withAlpha(0.35f));
    }
    else if (rowIsSelected)
        g.fillAll(selectedRowColour);
    else if (rowNumber % 2 == 1)
        g.fillAll(alternateRowColour);
}

String ConditionsTableModel::getCellText(int rowNumber, int columnId) const
{
    int seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx;
    rowToIndices(rowNumber, seqIdx, condIdx, repeatIdx, wavelengthIdx, siteIdx);
    switch (columnId)
    {
        case ColRow: return getTrialString(rowNumber);
        case ColSequence: return String(seqIdx);
        case ColCondition: return getConditionName(seqIdx, condIdx);
        case ColProbe: return getProbeName(seqIdx, condIdx);
        case ColWavelength: return getWavelengthString(seqIdx, condIdx, wavelengthIdx);
        case ColSites: return getSitesString(seqIdx, condIdx, siteIdx);
        case ColLightPower: return getLightPowerString(seqIdx, condIdx);
        case ColITI: return (condIdx == 0) ? "None" : getITIString(rowNumber);
        case ColStartTime: return getStartTimeString(rowNumber);
        case ColEndTime: return getEndTimeString(rowNumber);
        case ColRepeat: return (condIdx == 0) ? "None" : String(repeatIdx);
        default: return {};
    }
}

void ConditionsTableModel::paintCell(Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    g.setColour(textColour);
    g.setFont(FontOptions("Inter", "Regular", 12));
    g.drawText(getCellText(rowNumber, columnId), 4, 0, width - 6, height, Justification::centredLeft, true);
}

String ConditionsTableModel::exportCsv()
{
    if (!protocol) return {};
    static const char* headerNames[] = {
        "Trial", "Sequence", "Condition", "Source", "Wavelength", "Site",
        "Light Power", "ITI", "Start", "End", "Repeat"
    };
    static const int colIds[] = {
        ColRow, ColSequence, ColCondition, ColProbe, ColWavelength, ColSites,
        ColLightPower, ColITI, ColStartTime, ColEndTime, ColRepeat
    };
    static const int numCols = 11;
    auto escape = [](const String& s) -> String {
        if (s.containsAnyOf(",\"\r\n"))
            return "\"" + s.replace("\"", "\"\"") + "\"";
        return s;
    };
    const int n = getNumRows();
    String out;
    for (int c = 0; c < numCols; ++c)
    {
        if (c > 0) out << ",";
        out << escape(String(headerNames[c]));
    }
    out << "\n";
    for (int r = 0; r < n; ++r)
    {
        for (int c = 0; c < numCols; ++c)
        {
            if (c > 0) out << ",";
            out << escape(getCellText(r, colIds[c]));
        }
        out << "\n";
    }
    return out;
}
class ConditionsTableLookAndFeel : public LookAndFeel_V4
{
public:
    void drawTableHeaderBackground(Graphics& g, TableHeaderComponent& header) override
    {
        g.fillAll(header.findColour(TableHeaderComponent::backgroundColourId));

        const auto outlineColour = header.findColour(TableHeaderComponent::outlineColourId);
        g.setColour(outlineColour);
        g.fillRect(header.getLocalBounds().removeFromBottom(1));

        for (int i = header.getNumColumns(true); --i >= 0;)
            g.fillRect(header.getColumnPosition(i).removeFromRight(1));
    }
    void drawTableHeaderColumn(Graphics& g, TableHeaderComponent& header,
                               const String& columnName, int columnId,
                               int width, int height,
                               bool isMouseOver, bool isMouseDown,
                               int columnFlags) override
    {
        auto highlightColour = header.findColour(TableHeaderComponent::highlightColourId);

        if (isMouseDown)
            g.fillAll(highlightColour);
        else if (isMouseOver)
            g.fillAll(highlightColour.withMultipliedAlpha(0.625f));

        Rectangle<int> area(width, height);
        area.reduce(4, 0);

        if ((columnFlags & (TableHeaderComponent::sortedForwards | TableHeaderComponent::sortedBackwards)) != 0)
        {
            Path sortArrow;
            sortArrow.addTriangle(0.0f, 0.0f,
                                  0.5f, (columnFlags & TableHeaderComponent::sortedForwards) != 0 ? -0.8f : 0.8f,
                                  1.0f, 0.0f);

            g.setColour(header.findColour(TableHeaderComponent::textColourId).withAlpha(0.6f));
            g.fillPath(sortArrow, sortArrow.getTransformToScaleToFit(area.removeFromRight(height / 2).reduced(2).toFloat(), true));
        }

        g.setColour(header.findColour(TableHeaderComponent::textColourId));
        g.setFont(FontOptions("Inter", "Semi Bold", (float) height * 0.5f));
        g.drawFittedText(columnName, area, Justification::centredLeft, 1);
    }
};

ConditionsTable::ConditionsTable()
{
    tableLookAndFeel = std::make_unique<ConditionsTableLookAndFeel>();
    const int columnFlags = TableHeaderComponent::notSortable;
    table.setModel(&model);
    table.getHeader().addColumn("Trial", ColRow, 44, 36, 80, columnFlags);
    table.getHeader().addColumn("Sequence", ColSequence, 62, 50, 80, columnFlags);
    table.getHeader().addColumn("Condition", ColCondition, 88, 70, 120, columnFlags);
    table.getHeader().addColumn("Source", ColProbe, 68, 56, 100, columnFlags);
    table.getHeader().addColumn("Wavelength", ColWavelength, 90, 70, 120, columnFlags);
    table.getHeader().addColumn("Site", ColSites, 26, 18, 45, columnFlags);
    table.getHeader().addColumn("Light Power", ColLightPower, 72, 56, 100, columnFlags);
    table.getHeader().addColumn("ITI", ColITI, 29, 21, 42, columnFlags);
    table.getHeader().addColumn("Start", ColStartTime, 62, 50, 100, columnFlags);
    table.getHeader().addColumn("End", ColEndTime, 62, 50, 100, columnFlags);
    table.getHeader().addColumn("Repeat", ColRepeat, 48, 40, 80, columnFlags);
    table.setHeaderHeight(22);
    table.setRowHeight(30);
    table.getHeader().setLookAndFeel(tableLookAndFeel.get());
    tableViewport.setViewedComponent(&table, false);
    tableViewport.setScrollBarsShown(true, false);
    tableViewport.setScrollBarThickness(5);
    addAndMakeVisible(tableViewport);
    applyThemeColours();
    updateTableSize();
}
ConditionsTable::~ConditionsTable()
{
    table.getHeader().setLookAndFeel(nullptr);
}

void ConditionsTable::setProtocol(Protocol* p)
{
    model.setProtocol(p);
}

void ConditionsTable::refreshTable()
{
    table.updateContent();
    updateTableSize();
    table.repaint();
}

void ConditionsTable::setActiveRow(int row)
{
    activeRow = row;
    model.setActiveRow(row);
    if (row >= 0)
    {
        const int rowTop = kHeaderHeight + row * kRowHeight;
        const int rowBottom = rowTop + kRowHeight;
        const int viewTop = tableViewport.getViewPositionY();
        const int viewBottom = viewTop + tableViewport.getViewHeight();
        if (rowTop < viewTop)
            tableViewport.setViewPosition(0, rowTop);
        else if (rowBottom > viewBottom)
            tableViewport.setViewPosition(0, rowBottom - tableViewport.getViewHeight());
    }
    table.repaint();
}

void ConditionsTable::setActiveSequenceAndTrial(int seqIdx, int trialNum)
{
    int row = model.getRowIndexForSequenceAndTrial(seqIdx, trialNum);
    setActiveRow(row);
}

void ConditionsTable::setRunning(bool running)
{
    isRunning = running;
    model.setRunning(running);
    if (!running) { activeRow = -1; model.setActiveRow(-1); }
    table.repaint();
}

int ConditionsTable::getPreferredHeight()
{
    return kViewportHeight;
}

void ConditionsTable::resized()
{
    tableViewport.setBounds(0, 0, getWidth(), jmin(kViewportHeight, getHeight()));
    updateTableSize();
}
void ConditionsTable::colourChanged()
{
    applyThemeColours();
}

void ConditionsTable::lookAndFeelChanged()
{
    applyThemeColours();
}

void ConditionsTable::paintOverChildren(Graphics& g)
{
    g.setColour(findColour(ThemeColours::outline));
    g.drawRect(tableViewport.getBounds(), 1);
}
void ConditionsTable::applyThemeColours()
{
    const Colour textColour = findColour(ThemeColours::defaultText);
    const Colour outlineColour = findColour(ThemeColours::outline);
    const Colour headerColour = findColour(ThemeColours::widgetBackground);
    const Colour componentBackground = findColour(ThemeColours::componentBackground);
    const bool lightTheme = componentBackground.getPerceivedBrightness() > 0.65f;
    model.setTextColour(textColour);
    model.setAlternateRowColour(textColour.withAlpha(lightTheme ? 0.10f : 0.05f));
    model.setSelectedRowColour(findColour(ThemeColours::menuHighlightBackground).withAlpha(0.45f));
    table.getHeader().setColour(TableHeaderComponent::textColourId, textColour);
    table.getHeader().setColour(TableHeaderComponent::backgroundColourId, headerColour);
    table.getHeader().setColour(TableHeaderComponent::outlineColourId, outlineColour);
    table.getHeader().setColour(TableHeaderComponent::highlightColourId, findColour(ThemeColours::menuHighlightBackground).withAlpha(0.45f));
    table.setColour(ListBox::textColourId, textColour);
    table.setColour(ListBox::outlineColourId, outlineColour);
    table.setColour(ListBox::backgroundColourId, componentBackground);
    table.getHeader().repaint();
    table.repaint();
    repaint();
}

void ConditionsTable::updateTableSize()
{
    const int contentHeight = jmax(kViewportHeight, kHeaderHeight + model.getNumRows() * kRowHeight);
    table.setSize(getWidth(), contentHeight);
}

String ConditionsTable::exportTableAsCsv()
{
    return model.exportCsv();
}

int ConditionsTable::getNumRows()
{
    return model.getNumRows();
}

const int kConditionsTableWidth = 651;
const int kConditionsTableGap = 10;
/** Sequences column width; table is placed immediately to its right. */
const int kSequencesColumnWidth = 400;
const int kSaveCsvButtonTop = 30;
const int kSaveCsvButtonH = 20;
const int kSaveCsvButtonW = 150;
const int kExportStatusLabelGap = 8;
const int kExportStatusLabelWidth = 280;
const int kConditionsTableTop = kSaveCsvButtonTop + kSaveCsvButtonH + 4;

OptoProtocolInterface::OptoProtocolInterface(const String& name, Viewport* viewport_)
    : ParameterOwner(ParameterOwner::OTHER), viewport(viewport_)
{

    protocol = std::make_unique<Protocol>(name, this);
    
    Sequence* defaultSequence = new Sequence(this, protocol.get());
    
    protocol->addSequence(defaultSequence);
    sequenceInterfaces.add(new OptoSequenceInterface("Sequence 1", defaultSequence, this));
    addAndMakeVisible(sequenceInterfaces.getLast());
    
    addSequenceButton = std::make_unique<TextButton>("addSequenceButton");
    addSequenceButton->setButtonText("Add Sequence");
    addSequenceButton->addListener(this);
    addAndMakeVisible(addSequenceButton.get());

    loadSourcesGlobalButton = std::make_unique<TextButton>("loadSourcesGlobal");
    loadSourcesGlobalButton->setButtonText("Load Sources");
    loadSourcesGlobalButton->setTooltip("Load a hardware JSON file (replaces the current source configuration).");
    loadSourcesGlobalButton->addListener(this);
    addAndMakeVisible(loadSourcesGlobalButton.get());

    editSourcesGlobalButton = std::make_unique<TextButton>("editSourcesGlobal");
    editSourcesGlobalButton->setButtonText("Edit Sources");
    editSourcesGlobalButton->setTooltip("Edit the currently loaded hardware JSON.");
    editSourcesGlobalButton->addListener(this);
    editSourcesGlobalButton->setEnabled(false);
    addAndMakeVisible(editSourcesGlobalButton.get());
    
    saveTableCsvButton = std::make_unique<TextButton>("saveTableCsv");
    saveTableCsvButton->setButtonText("Export Table");
    saveTableCsvButton->addListener(this);
    addAndMakeVisible(saveTableCsvButton.get());
    
    exportStatusLabel = std::make_unique<Label>("exportStatusLabel", "*");
    exportStatusLabel->setFont(FontOptions("Inter", "Regular", 12));
    exportStatusLabel->setJustificationType(Justification::centredLeft);
    exportStatusLabel->setColour(Label::textColourId, findColour(ThemeColours::defaultText));
    addAndMakeVisible(exportStatusLabel.get());
    
    conditionsTable = std::make_unique<ConditionsTable>();
    conditionsTable->setProtocol(protocol.get());
    addAndMakeVisible(conditionsTable.get());
    refreshConditionsTable();
}


OptoProtocolInterface::~OptoProtocolInterface()
{
    // Destructor implementation
}

void OptoProtocolInterface::updateBounds(int expandBy)
{
    int sequencesHeight = 90;
    for (auto interface : sequenceInterfaces)
        sequencesHeight += interface->getHeight();
    int tableHeight = kConditionsTableTop + (conditionsTable ? conditionsTable->getPreferredHeight() : 0);
    int totalHeight = jmax(sequencesHeight, tableHeight);
    int currentScrollDistance = viewport->getViewPositionY();
    setBounds(0, 0, getWidth(), totalHeight);
    viewport->setViewPosition(0, currentScrollDistance);
}

void OptoProtocolInterface::resized()
{
    int leftMargin = 15;
    if (loadSourcesGlobalButton)
        loadSourcesGlobalButton->setBounds(leftMargin, 4, 112, 20);
    if (editSourcesGlobalButton)
        editSourcesGlobalButton->setBounds(leftMargin + 120, 4, 112, 20);
    int currentHeight = 30;
    for (auto interface : sequenceInterfaces)
    {
        interface->setBounds(leftMargin, currentHeight, kSequencesColumnWidth, interface->getHeight());
        currentHeight += interface->getHeight();
    }
    addSequenceButton->setBounds(leftMargin + 15, currentHeight + 5, 150, 20);
    int tableX = leftMargin + kSequencesColumnWidth + kConditionsTableGap;
    if (saveTableCsvButton)
        saveTableCsvButton->setBounds(tableX, kSaveCsvButtonTop, kSaveCsvButtonW, kSaveCsvButtonH);
    if (exportStatusLabel)
        exportStatusLabel->setBounds(tableX + kSaveCsvButtonW + kExportStatusLabelGap, kSaveCsvButtonTop,
                                       kExportStatusLabelWidth, kSaveCsvButtonH);
    if (conditionsTable)
        conditionsTable->setBounds(tableX, kConditionsTableTop, kConditionsTableWidth, conditionsTable->getPreferredHeight());
}

void OptoProtocolInterface::paint(Graphics& g)
{
    // Draw the background
    g.fillAll(findColour(ThemeColours::componentBackground));


}

void OptoProtocolInterface::buttonClicked(Button* button)
{
    if (button == loadSourcesGlobalButton.get())
    {
        launchLoadHardwareJsonChooser();
        return;
    }
    if (button == editSourcesGlobalButton.get())
    {
        launchEditHardwareJsonDialog();
        return;
    }
    if (button == saveTableCsvButton.get())
    {
        const Time t = Time::getCurrentTime();
        const String stamp = t.formatted("%Y_%m_%d_%H_%M_%S");
        String namePrefix = File::createLegalFileName(protocol->name).trim();
        if (namePrefix.isEmpty())
            namePrefix = "protocol";
        const String defaultName = namePrefix + "_" + stamp + ".csv";
        const File defaultFile = File::getSpecialLocation(File::userDesktopDirectory).getChildFile(defaultName);
        fileChooser = std::make_unique<juce::FileChooser>("Export table as CSV", defaultFile, "*.csv", true, false, this);
        const auto flags = FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(flags, [this, stamp](const juce::FileChooser& c) {
            const File f = c.getResult();
            if (f != File())
            {
                const String csv = conditionsTable->exportTableAsCsv();
                f.replaceWithText(csv);
                lastExportedCsvSnapshot = csv;
                lastSavedTimestampDisplay = stamp;
                updateExportStatusLabel();
            }
            fileChooser.reset();
        });
        return;
    }
    if (button == addSequenceButton.get())
    {
        LOGD("Add sequence button clicked");
        
        Sequence* defaultSequence = new Sequence(this, protocol.get());
        
        protocol->sequences.add(defaultSequence);
        int numSequences = protocol->sequences.size();
        sequenceInterfaces.add(new OptoSequenceInterface("Sequence " + String(numSequences), defaultSequence, this));
        addAndMakeVisible(sequenceInterfaces.getLast());
       
        updateBounds(sequenceInterfaces.getLast()->getHeight());
        
        refreshConditionsTable();
        timeline->setTotalTime(getTableTotalDuration());
        timeline->setTotalTrials(protocol->getTotalTrials());
    }
}

void OptoProtocolInterface::removeConditionInterface(OptoConditionInterface* conditionInterface)
{
    for (auto seq : sequenceInterfaces)
        seq->removeCondition(conditionInterface);
    refreshConditionsTable();
    if (timeline != nullptr)
    {
        timeline->setTotalTime(getTableTotalDuration());
        timeline->setTotalTrials(protocol->getTotalTrials());
    }
    updateBounds(0);
}

void OptoProtocolInterface::removeSequenceInterface(OptoSequenceInterface* sequenceInterface)
{
    if (sequenceInterfaces.size() <= 1)
        return;
    if (!sequenceInterfaces.contains(sequenceInterface))
        return;
    Sequence* seq = sequenceInterface->getSequence();
    sequenceInterfaces.removeObject(sequenceInterface, true);
    protocol->removeSequence(seq);
    refreshConditionsTable();
    timeline->setTotalTime(getTableTotalDuration());
    timeline->setTotalTrials(protocol->getTotalTrials());
    updateBounds(0);
    resized();
    for (auto* si : sequenceInterfaces)
        si->enable();
}

void OptoProtocolInterface::parameterChangeRequest(Parameter* parameter)
{
    if (parameter != nullptr)
    {
        LOGD("Parameter name: ", parameter->getName(),
             ", original value: ", parameter->getValueAsString());
        
        parameter->updateValue();
        
        LOGD("Parameter name: ", parameter->getName(),
             ", new value: ", parameter->getValueAsString());

        if (hardwareConfig != nullptr && !hardwareConfig->isEmpty())
        {
            for (auto* si : sequenceInterfaces)
            {
                for (auto* ci : si->getConditionInterfaces())
                {
                    Condition* cnd = ci->getCondition();
                    if (parameter == &cnd->source)
                    {
                        cnd->refreshForSelectedSource(hardwareConfig.get());
                        ci->onHardwareConfigChanged();
                    }
                    if (parameter == &cnd->pulse_power)
                    {
                        const float pulsePower = cnd->pulse_power.getFloatValue();
                        const int di = jlimit(0, (int) hardwareConfig->devices.size() - 1, cnd->source.getSelectedIndex());
                        auto logForWavelength = [&](int wavelengthNm) {
                            if (const OptoHardwareLightSource* lsp = hardwareConfig->findLightSource(di, wavelengthNm))
                            {
                                const OptoHardwareLightSource& cal = *lsp;
                                const float maxVoltage = OptoHardwareConfig::mapPowerToControlVoltage(pulsePower, cal);
                                LOGD("mapPowerToControlVoltage: pulsePower=", String(pulsePower, 4),
                                     " nm=", wavelengthNm, " -> maxVoltage=", String(maxVoltage, 4));
                            }
                        };
                        if (cnd->availableWavelengths.size() > 0)
                        {
                            for (int wli = 0; wli < cnd->availableWavelengths.size(); ++wli)
                                logForWavelength(cnd->availableWavelengths[wli]);
                        }
                        else
                        {
                            const auto& dev = hardwareConfig->devices[di];
                            for (int lsi = 0; lsi < dev.lightSources.size(); ++lsi)
                                logForWavelength(dev.lightSources[lsi].wavelength);
                        }
                    }
                }
            }
        }
    }
    
    if (timeline != nullptr)
        timeline->reset();
    protocol->reset();
    protocol->createTrials();
    refreshConditionsTable();
    if (timeline != nullptr)
    {
        timeline->setTotalTime(getTableTotalDuration());
        timeline->setTotalTrials(protocol->getTotalTrials());
    }
    Timer::callAfterDelay(0, [this]()
    {
        updateBounds(0);
        resized();
    });
}

void OptoProtocolInterface::setTimeline(ProtocolTimeline* timeline_)
{
    timeline = timeline_;
    
    refreshConditionsTable();
    timeline->setTotalTime(getTableTotalDuration());
    timeline->setTotalTrials(protocol->getTotalTrials());
    protocol->addActionListener(timeline);
}

void OptoProtocolInterface::refreshConditionsTable()
{
    if (conditionsTable)
        conditionsTable->refreshTable();
    updateExportTableButtonState();
    updateExportStatusLabel();
}

float OptoProtocolInterface::getTableTotalDuration()
{
    if (!conditionsTable) return protocol->getTotalTime();
    return conditionsTable->getTotalProtocolDuration();
}

void OptoProtocolInterface::getNewConditionArrays(Array<String>& names, Array<int>& sites, Array<int>& wavelengths) const
{
    names.clear();
    sites.clear();
    wavelengths.clear();
    if (!hasHardwareConfig())
    {
        names.add("Source 1");
        sites.add(1);
        return;
    }
    for (const auto& d : hardwareConfig->devices)
    {
        names.add(d.name);
        sites.add(d.is_np_opto ? kNpOptoSitesPerSource : 1);
    }
    if (names.isEmpty())
    {
        names.add("Source 1");
        sites.add(1);
    }
    if (hardwareConfig->devices.size() > 0)
    {
        const auto& d0 = hardwareConfig->devices[0];
        if (d0.lightSources.size() > 0)
            wavelengths.add(d0.lightSources[0].wavelength);
    }
    if (wavelengths.isEmpty())
        wavelengths.add(638);
}

void OptoProtocolInterface::launchLoadHardwareJsonChooser()
{
    hardwareFileChooser = std::make_unique<FileChooser>("Load hardware JSON", File{}, "*.json", true, false, this);
    const auto flags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles;
    hardwareFileChooser->launchAsync(flags, [this](const FileChooser& c) {
        const File f = c.getResult();
        if (f != File())
        {
            const String jsonText = f.loadFileAsString();
            auto cfg = OptoHardwareConfig::parseJson(jsonText);
            if (cfg != nullptr)
                applyLoadedHardwareConfig(std::move(cfg), f.getFullPathName(), jsonText);
            else
                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Hardware JSON", "Could not parse the selected file.");
        }
        hardwareFileChooser.reset();
    });
}

void OptoProtocolInterface::applyLoadedHardwareConfig(std::unique_ptr<OptoHardwareConfig> cfg,
                                                      const String& pathForXml,
                                                      const String& jsonText)
{
    hardwareConfig = std::move(cfg);
    if (pathForXml.isNotEmpty())
        hardwareConfigPath = pathForXml;
    if (jsonText.isNotEmpty())
        hardwareConfigJsonText = jsonText;
    if (editSourcesGlobalButton != nullptr)
        editSourcesGlobalButton->setEnabled(hardwareConfig != nullptr);
    const bool editingEnabled = addSequenceButton != nullptr && addSequenceButton->isEnabled();
    for (auto* si : sequenceInterfaces)
        for (auto* ci : si->getConditionInterfaces())
            ci->getCondition()->applyHardwareCatalog(hardwareConfig.get());
    for (auto* si : sequenceInterfaces)
        for (auto* ci : si->getConditionInterfaces())
        {
            ci->onHardwareConfigChanged();
            if (editingEnabled)
                ci->enable();
            else
                ci->disable();
        }
    protocol->createTrials();
    refreshConditionsTable();
    if (timeline != nullptr)
    {
        timeline->reset();
        timeline->setTotalTime(getTableTotalDuration());
        timeline->setTotalTrials(protocol->getTotalTrials());
    }
    Timer::callAfterDelay(0, [this]() {
        updateBounds(0);
        resized();
    });
}

void OptoProtocolInterface::launchEditHardwareJsonDialog()
{
    if (!hasHardwareConfig())
    {
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Edit Sources", "Load sources JSON before editing.");
        return;
    }

    if (hardwareJsonEditorWindow != nullptr)
    {
        if (!hardwareJsonEditorWindow->isVisible())
            hardwareJsonEditorWindow->setVisible(true);
        hardwareJsonEditorWindow->toFront(true);
        return;
    }

    if (hardwareConfigJsonText.isEmpty() && hardwareConfigPath.isNotEmpty())
    {
        const File f(hardwareConfigPath);
        if (f.existsAsFile())
            hardwareConfigJsonText = f.loadFileAsString();
    }

    hardwareJsonEditorOriginalText = hardwareConfigJsonText;
    auto editor = std::make_unique<HardwareJsonEditorComponent>();
    editor->setText(hardwareJsonEditorOriginalText);
    editor->onSave = [this]()
    {
        if (hardwareJsonEditorWindow == nullptr)
            return;
        auto* c = dynamic_cast<HardwareJsonEditorComponent*>(hardwareJsonEditorWindow->getContentComponent());
        if (c == nullptr)
            return;
        if (!tryApplyHardwareJsonText(c->getText(), true))
            return;
        hardwareJsonEditorWindow->exitModalState(0);
        hardwareJsonEditorWindow = nullptr;
    };
    editor->onReset = [this]()
    {
        if (hardwareJsonEditorWindow == nullptr)
            return;
        if (auto* c = dynamic_cast<HardwareJsonEditorComponent*>(hardwareJsonEditorWindow->getContentComponent()))
            c->setText(hardwareJsonEditorOriginalText);
    };

    hardwareJsonEditorWindow = std::make_unique<ValidatingCloseDialogWindow>(
        "Edit Sources",
        findColour(ThemeColours::componentBackground),
        [this]() { return tryCloseHardwareJsonEditor(); },
        [this]()
        {
            MessageManager::callAsync([this]() { hardwareJsonEditorWindow = nullptr; });
        });
    hardwareJsonEditorWindow->setUsingNativeTitleBar(true);
    hardwareJsonEditorWindow->setResizable(true, true);
    hardwareJsonEditorWindow->setContentOwned(editor.release(), true);
    hardwareJsonEditorWindow->centreWithSize(760, 1040);
    hardwareJsonEditorWindow->setVisible(true);
    hardwareJsonEditorWindow->enterModalState(true, nullptr, false);
}

bool OptoProtocolInterface::tryApplyHardwareJsonText(const String& jsonText, bool showInvalidAlert)
{
    var syntaxRoot;
    const Result syntaxResult = JSON::parse(jsonText, syntaxRoot);
    if (syntaxResult.failed())
    {
        if (showInvalidAlert)
        {
            const String errorText = syntaxResult.getErrorMessage();
            int lineNumber = parseFirstIntegerAfterToken(errorText, "line");
            if (lineNumber <= 0)
            {
                const int charOffset = parseFirstIntegerAfterToken(errorText, "character");
                lineNumber = lineFromCharacterOffset(jsonText, charOffset);
            }
            String msg = "JSON is invalid: " + errorText;
            const String badLine = getLineText(jsonText, lineNumber);
            if (lineNumber > 0 && badLine.isNotEmpty())
                msg << "\n\nFirst invalid line (" << String(lineNumber) << "):\n" << badLine;
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Edit Sources", msg);
        }
        return false;
    }

    auto cfg = OptoHardwareConfig::parseJson(jsonText);
    if (cfg == nullptr)
    {
        if (showInvalidAlert)
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                             "Edit Sources",
                                             "JSON is valid syntax, but does not match the expected hardware schema.\n\n"
                                                 + getHardwareSchemaErrorMessage(syntaxRoot));
        return false;
    }

    applyLoadedHardwareConfig(std::move(cfg), String(), jsonText);
    return true;
}

bool OptoProtocolInterface::tryCloseHardwareJsonEditor()
{
    if (hardwareJsonEditorWindow == nullptr)
        return true;
    auto* c = dynamic_cast<HardwareJsonEditorComponent*>(hardwareJsonEditorWindow->getContentComponent());
    if (c == nullptr)
        return true;

    const String currentText = c->getText();
    if (currentText == hardwareJsonEditorOriginalText)
        return true;

    const int choice = AlertWindow::showYesNoCancelBox(AlertWindow::QuestionIcon,
                                                        "Edit Sources",
                                                        "You have unsaved changes. Save before closing?",
                                                        "Save",
                                                        "Discard",
                                                        "Cancel",
                                                        this);
    if (choice == 1)
        return tryApplyHardwareJsonText(currentText, true);
    if (choice == 2)
        return true;
    return false;
}

void OptoProtocolInterface::updateExportTableButtonState()
{
    if (!saveTableCsvButton || !conditionsTable)
        return;
    const bool editingEnabled = addSequenceButton->isEnabled();
    saveTableCsvButton->setEnabled(editingEnabled && conditionsTable->getNumRows() > 0);
}

void OptoProtocolInterface::updateExportStatusLabel()
{
    if (exportStatusLabel == nullptr || !conditionsTable)
        return;
    const String current = conditionsTable->exportTableAsCsv();
    if (lastExportedCsvSnapshot.isEmpty())
        exportStatusLabel->setText("*", dontSendNotification);
    else if (current != lastExportedCsvSnapshot)
        exportStatusLabel->setText(lastSavedTimestampDisplay + " *", dontSendNotification);
    else
        exportStatusLabel->setText(lastSavedTimestampDisplay, dontSendNotification);
}

void OptoProtocolInterface::setActiveTrial(int seqIdx, int trialNum)
{
    if (conditionsTable)
        conditionsTable->setActiveSequenceAndTrial(seqIdx, trialNum);
}

void OptoProtocolInterface::setTableRunning(bool running)
{
    if (conditionsTable)
        conditionsTable->setRunning(running);
}

void OptoProtocolInterface::enable()
{
    for (auto sequence : sequenceInterfaces)
    {
        sequence->enable();
    }
    
    addSequenceButton->setEnabled(true);
    if (loadSourcesGlobalButton)
        loadSourcesGlobalButton->setEnabled(true);
    if (editSourcesGlobalButton)
        editSourcesGlobalButton->setEnabled(hasHardwareConfig());
    updateExportTableButtonState();
}

void OptoProtocolInterface::disable()
{
    LOGD("Disabling OptoProtocolInterface ");
    
    for (auto sequence : sequenceInterfaces)
    {
        sequence->disable();
    }
    
    addSequenceButton->setEnabled(false);
    if (loadSourcesGlobalButton)
        loadSourcesGlobalButton->setEnabled(false);
    if (editSourcesGlobalButton)
        editSourcesGlobalButton->setEnabled(false);
    if (saveTableCsvButton)
        saveTableCsvButton->setEnabled(false);
}

namespace
{
static void appendStimulusXml(XmlElement* condEl, Stimulus* s)
{
    XmlElement* st = condEl->createNewChildElement("STIMULUS");
    switch (s->type)
    {
        case PULSE_TRAIN:
        {
            auto* pt = static_cast<PulseTrain*>(s);
            st->setAttribute("type", "pulse");
            st->setAttribute("pulseWidth", pt->pulse_width.getFloatValue());
            st->setAttribute("pulseFrequency", pt->pulse_frequency.getFloatValue());
            st->setAttribute("pulseCount", pt->pulse_count.getIntValue());
            st->setAttribute("rampDuration", pt->ramp_duration.getFloatValue());
            break;
        }
        case SINUSOID:
        {
            auto* sw = static_cast<SineWave*>(s);
            st->setAttribute("type", "sine");
            st->setAttribute("sineWaveDuration", sw->sine_wave_duration.getFloatValue());
            st->setAttribute("sineWaveFrequency", sw->sine_wave_frequency.getFloatValue());
            break;
        }
        case RAMP:
        {
            auto* rs = static_cast<RampStimulus*>(s);
            st->setAttribute("type", "ramp");
            st->setAttribute("plateauDuration", rs->plateau_duration.getFloatValue());
            st->setAttribute("rampOnsetDuration", rs->ramp_onset_duration.getFloatValue());
            st->setAttribute("rampOffsetDuration", rs->ramp_offset_duration.getFloatValue());
            st->setAttribute("rampProfile", rs->ramp_profile.getSelectedIndex());
            break;
        }
        case CUSTOM:
        {
            auto* cs = static_cast<CustomStimulus*>(s);
            st->setAttribute("type", "custom");
            st->setAttribute("sampleFrequency", cs->sample_frequency.getFloatValue());
            String w;
            for (int i = 0; i < cs->stimulus_waveform.size(); ++i)
            {
                if (i > 0) w << ",";
                w << String(cs->stimulus_waveform[i], 8);
            }
            st->setAttribute("waveform", w);
            break;
        }
        default: break;
    }
}

static void appendConditionXml(XmlElement* seqEl, Condition* cond)
{
    XmlElement* c = seqEl->createNewChildElement("CONDITION");
    c->setAttribute("numRepeats", cond->num_repeats.getIntValue());
    c->setAttribute("sourceIndex", cond->source.getSelectedIndex());
    c->setAttribute("pulsePower", cond->pulse_power.getFloatValue());
    String wl;
    for (int i = 0; i < cond->availableWavelengths.size(); ++i)
    {
        if (i > 0) wl << ",";
        wl << cond->availableWavelengths[i];
    }
    c->setAttribute("wavelengths", wl);
    String sitesStr;
    const auto siteArr = cond->sites->getArrayValue();
    for (int i = 0; i < siteArr.size(); ++i)
    {
        if (i > 0) sitesStr << ",";
        sitesStr << String(static_cast<int>(siteArr[i]));
    }
    c->setAttribute("sites", sitesStr);
    for (auto* stimulus : cond->stimuli)
        appendStimulusXml(c, stimulus);
}
} // namespace

String OptoSequenceInterface::getSequenceDisplayName() const
{
    return sequenceNameLabel ? sequenceNameLabel->getText() : String("Sequence");
}

void OptoSequenceInterface::syncDeleteSequenceVisibility()
{
    if (deleteSequenceButton != nullptr && parent != nullptr)
        deleteSequenceButton->setVisible(parent->getNumSequenceInterfaces() > 1);
}

void OptoSequenceInterface::importConditionFromXml(XmlElement* cEl)
{
    if (cEl == nullptr || cEl->getTagName() != "CONDITION")
        return;
    Array<String> availableSources;
    Array<int> sitesPerSource;
    if (parent != nullptr && parent->hasHardwareConfig())
    {
        Array<int> dummy;
        parent->getNewConditionArrays(availableSources, sitesPerSource, dummy);
    }
    else
    {
        availableSources = { "Probe A", "Probe B" };
        sitesPerSource = { kNpOptoSitesPerSource, kNpOptoSitesPerSource };
    }
    Condition* condition = new Condition(parent, availableSources, sitesPerSource, Array<int>{638}, sequence);
    sequence->addCondition(condition);
    condition->availableWavelengths.clear();
    {
        String wl = cEl->getStringAttribute("wavelengths", "638");
        StringArray tokens;
        tokens.addTokens(wl, ",", "");
        for (auto& t : tokens)
            if (t.isNotEmpty())
                condition->addWavelength(t.getIntValue());
        if (condition->availableWavelengths.isEmpty())
            condition->addWavelength(638);
    }
    condition->num_repeats.setNextValue(cEl->getIntAttribute("numRepeats", 1), false);
    condition->source.setNextValue(cEl->getIntAttribute("sourceIndex", 0), false);
    condition->pulse_power.setNextValue((float)cEl->getDoubleAttribute("pulsePower", 10.0), false);
    {
        const int srcIdx = jlimit(0, jmax(0, sitesPerSource.size() - 1), condition->source.getSelectedIndex());
        condition->sites->setChannelCount(sitesPerSource[srcIdx]);
        String sitesStr = cEl->getStringAttribute("sites", "0");
        Array<var> siteVals;
        StringArray siteTok;
        siteTok.addTokens(sitesStr, ",", "");
        for (auto& t : siteTok)
            if (t.isNotEmpty())
                siteVals.add(t.getIntValue());
        condition->sites->setNextValue(var(siteVals), false);
    }
    if (parent != nullptr && parent->hasHardwareConfig())
        condition->refreshForSelectedSource(parent->getHardwareConfig());
    XmlElement* sEl = cEl->getChildByName("STIMULUS");
    if (sEl == nullptr)
        return;
    Stimulus* stimulus = nullptr;
    const String type = sEl->getStringAttribute("type");
    if (type == "pulse")
    {
        auto* pt = new PulseTrain(parent, condition);
        pt->pulse_width.setNextValue((float)sEl->getDoubleAttribute("pulseWidth", 10.0), false);
        pt->pulse_frequency.setNextValue((float)sEl->getDoubleAttribute("pulseFrequency", 10.0), false);
        pt->pulse_count.setNextValue(sEl->getIntAttribute("pulseCount", 1), false);
        pt->ramp_duration.setNextValue((float)sEl->getDoubleAttribute("rampDuration", 0.0), false);
        stimulus = pt;
    }
    else if (type == "sine")
    {
        auto* sw = new SineWave(parent, condition);
        sw->sine_wave_duration.setNextValue((float)sEl->getDoubleAttribute("sineWaveDuration", 100.0), false);
        sw->sine_wave_frequency.setNextValue((float)sEl->getDoubleAttribute("sineWaveFrequency", 10.0), false);
        stimulus = sw;
    }
    else if (type == "ramp")
    {
        auto* rs = new RampStimulus(parent, condition);
        rs->plateau_duration.setNextValue((float)sEl->getDoubleAttribute("plateauDuration", 100.0), false);
        rs->ramp_onset_duration.setNextValue((float)sEl->getDoubleAttribute("rampOnsetDuration", 10.0), false);
        rs->ramp_offset_duration.setNextValue((float)sEl->getDoubleAttribute("rampOffsetDuration", 10.0), false);
        rs->ramp_profile.setNextValue(sEl->getIntAttribute("rampProfile", 0), false);
        stimulus = rs;
    }
    else if (type == "custom")
    {
        auto* cs = new CustomStimulus(parent, condition);
        cs->sample_frequency.setNextValue((float)sEl->getDoubleAttribute("sampleFrequency", 10000.0), false);
        cs->stimulus_waveform.clear();
        String w = sEl->getStringAttribute("waveform");
        StringArray vals;
        vals.addTokens(w, ",", "");
        for (auto& t : vals)
            if (t.isNotEmpty())
                cs->stimulus_waveform.add(t.getFloatValue());
        stimulus = cs;
    }
    if (stimulus == nullptr)
        return;
    condition->addStimulus(stimulus);
    conditionInterfaces.add(new OptoConditionInterface(condition, stimulus, parent));
    addAndMakeVisible(conditionInterfaces.getLast());
    int h = 230;
    for (int i = 0; i < conditionInterfaces.size(); ++i)
        h += (i == 0 ? conditionInterfaceHeight : 10 + conditionInterfaceHeight);
    setBounds(0, 0, 0, h);
}

void OptoProtocolInterface::clearAllSequences()
{
    while (!sequenceInterfaces.isEmpty())
    {
        OptoSequenceInterface* si = sequenceInterfaces.getLast();
        Sequence* seq = si->getSequence();
        sequenceInterfaces.removeObject(si, true);
        protocol->removeSequence(seq);
    }
}

void OptoProtocolInterface::appendProtocolXml(XmlElement* visParent)
{
    XmlElement* p = visParent->createNewChildElement("PROTOCOL");
    p->setAttribute("name", protocol->name);
    p->setAttribute("description", protocol->description);
    if (hardwareConfigPath.isNotEmpty())
        p->setAttribute("hardwareConfigPath", hardwareConfigPath);
    for (int i = 0; i < sequenceInterfaces.size(); ++i)
    {
        Sequence* seq = sequenceInterfaces[i]->getSequence();
        XmlElement* s = p->createNewChildElement("SEQUENCE");
        s->setAttribute("name", sequenceInterfaces[i]->getSequenceDisplayName());
        s->setAttribute("baseline", seq->baseline_interval.getFloatValue());
        s->setAttribute("minIti", seq->min_iti.getFloatValue());
        s->setAttribute("maxIti", seq->max_iti.getFloatValue());
        s->setAttribute("randomize", seq->randomize.getBoolValue() ? 1 : 0);
        for (auto* cond : seq->conditions)
            appendConditionXml(s, cond);
    }
}

void OptoProtocolInterface::loadProtocolFromXml(XmlElement* protocolElement)
{
    if (protocolElement == nullptr)
        return;
    protocol->name = protocolElement->getStringAttribute("name", protocol->name);
    protocol->description = protocolElement->getStringAttribute("description");
    hardwareConfigPath = protocolElement->getStringAttribute("hardwareConfigPath");
    hardwareConfigJsonText.clear();
    hardwareConfig.reset();
    if (hardwareConfigPath.isNotEmpty())
    {
        const File hf(hardwareConfigPath);
        if (hf.existsAsFile())
        {
            hardwareConfigJsonText = hf.loadFileAsString();
            hardwareConfig = OptoHardwareConfig::parseJson(hardwareConfigJsonText);
        }
    }
    if (editSourcesGlobalButton != nullptr)
        editSourcesGlobalButton->setEnabled(hardwareConfig != nullptr && addSequenceButton != nullptr && addSequenceButton->isEnabled());
    clearAllSequences();
    for (auto* seqEl = protocolElement->getFirstChildElement(); seqEl != nullptr; seqEl = seqEl->getNextElement())
    {
        if (seqEl->getTagName() != "SEQUENCE")
            continue;
        Sequence* seq = new Sequence(this, protocol.get());
        protocol->addSequence(seq);
        const String seqName = seqEl->getStringAttribute("name", "Sequence " + String(protocol->sequences.size()));
        auto* si = new OptoSequenceInterface(seqName, seq, this, true);
        sequenceInterfaces.add(si);
        addAndMakeVisible(si);
        seq->baseline_interval.setNextValue((float)seqEl->getDoubleAttribute("baseline", 0.0), false);
        seq->min_iti.setNextValue((float)seqEl->getDoubleAttribute("minIti", 1.0), false);
        seq->max_iti.setNextValue((float)seqEl->getDoubleAttribute("maxIti", 1.0), false);
        seq->randomize.setNextValue(seqEl->getBoolAttribute("randomize", true), false);
        for (auto* cEl = seqEl->getFirstChildElement(); cEl != nullptr; cEl = cEl->getNextElement())
        {
            if (cEl->getTagName() == "CONDITION")
                si->importConditionFromXml(cEl);
        }
        const int nCond = seq->conditions.size();
        int h = 230;
        for (int i = 0; i < nCond; ++i)
            h += (i == 0 ? OptoSequenceInterface::kConditionInterfaceHeight
                         : 10 + OptoSequenceInterface::kConditionInterfaceHeight);
        si->setBounds(0, 0, 0, h);
    }
    if (sequenceInterfaces.isEmpty())
    {
        Sequence* seq = new Sequence(this, protocol.get());
        protocol->addSequence(seq);
        sequenceInterfaces.add(new OptoSequenceInterface("Sequence 1", seq, this, false));
        addAndMakeVisible(sequenceInterfaces.getLast());
    }
    for (auto* si : sequenceInterfaces)
        si->syncDeleteSequenceVisibility();
    protocol->createTrials();
    lastExportedCsvSnapshot.clear();
    lastSavedTimestampDisplay.clear();
    refreshConditionsTable();
    if (timeline != nullptr)
    {
        timeline->reset();
        timeline->setTotalTime(getTableTotalDuration());
        timeline->setTotalTrials(protocol->getTotalTrials());
    }
    updateBounds(0);
    resized();
}

void OptoProtocolCanvas::saveCustomParametersToXml(XmlElement* xml)
{
    XmlElement* vis = xml->createNewChildElement("VISUALIZER");
    vis->setAttribute("selectedProtocolId", protocolSelector->getSelectedId());
    for (auto* pi : protocolInterfaces)
        pi->appendProtocolXml(vis);
}

void OptoProtocolCanvas::loadCustomParametersFromXml(XmlElement* xml)
{
    XmlElement* visNode = xml->getChildByName("VISUALIZER");
    if (visNode != nullptr)
    {
        const int selectedId = visNode->getIntAttribute("selectedProtocolId", 1);
        for (auto* pi : protocolInterfaces)
        {
            pi->getProtocol()->removeActionListener(protocolTimeline.get());
            pi->getProtocol()->removeActionListener(this);
        }
        currentProtocol = nullptr;
        viewport->setViewedComponent(nullptr, false);
        protocolInterfaces.clear(true);
        protocolSelector->clear(dontSendNotification);
        int itemId = 1;
        for (auto* protoEl = visNode->getFirstChildElement(); protoEl != nullptr; protoEl = protoEl->getNextElement())
        {
            if (protoEl->getTagName() != "PROTOCOL")
                continue;
            auto* iface = new OptoProtocolInterface(protoEl->getStringAttribute("name", "Protocol"), viewport.get());
            iface->setTimeline(protocolTimeline.get());
            iface->loadProtocolFromXml(protoEl);
            protocolInterfaces.add(iface);
            protocolSelector->addItem(iface->getProtocol()->name, itemId++);
        }
        if (protocolInterfaces.isEmpty())
        {
            auto* iface = new OptoProtocolInterface("Protocol 1", viewport.get());
            iface->setTimeline(protocolTimeline.get());
            protocolInterfaces.add(iface);
            protocolSelector->addItem(iface->getProtocol()->name, 1);
        }
        protocolSelector->setSelectedId(selectedId, dontSendNotification);
        if (protocolSelector->getSelectedId() == 0)
            protocolSelector->setSelectedId(1, dontSendNotification);
        applySelectedProtocol();
    }
}

OptoProtocolInterface* OptoProtocolCanvas::getCurrentInterface()
{
    const int id = protocolSelector->getSelectedId();
    if (id <= 0 || id > protocolInterfaces.size())
        return nullptr;
    return protocolInterfaces[id - 1];
}

void OptoProtocolCanvas::applySelectedProtocol()
{
    if (protocolTimeline->isRunning)
    {
        protocolTimeline->pause();
        if (currentProtocol != nullptr)
            currentProtocol->pause();
    }
    runButton->setButtonText("Run");
    runButton->setEnabled(true);

    if (currentProtocol != nullptr)
        currentProtocol->removeActionListener(this);

    const int id = protocolSelector->getSelectedId();
    if (id <= 0 || id > protocolInterfaces.size())
        return;

    OptoProtocolInterface* iface = protocolInterfaces[id - 1];
    viewport->setViewedComponent(iface, false);
    currentProtocol = iface->getProtocol();
    currentProtocol->addActionListener(this);

    iface->refreshConditionsTable();
    protocolTimeline->reset();
    protocolTimeline->setTotalTime(iface->getTableTotalDuration());
    protocolTimeline->setTotalTrials(currentProtocol->getTotalTrials());

    iface->setTableRunning(false);
    iface->enable();

    iface->setSize(viewport->getMaximumVisibleWidth(), iface->getHeight());
    iface->updateBounds(0);
    iface->resized();
    resized();

    if (deleteProtocolButton != nullptr)
        deleteProtocolButton->setEnabled(protocolInterfaces.size() > 1);
}


void ProtocolTimeline::paint(Graphics& g)
{
    g.setColour(findColour(ThemeColours::defaultText));
    g.drawText(getTimeString(elapsedTime), 0, 0, 50, 20, Justification::centredLeft);
    g.drawText(getTimeString(totalTime-elapsedTime),getWidth()-150, 0, 50, 20, Justification::centredRight);
    
    if (currentTrial == 0)
    {
        g.drawText("Trials: " + String (totalTrials),
                   getWidth()-90, 0, 90, 20, Justification::centredLeft);
    } else {
        g.drawText("Trial " + String (currentTrial) + "/" + String (totalTrials),
                   getWidth()-90, 0, 90, 20, Justification::centredLeft);
    }
    
    g.setColour(findColour(ThemeColours::menuHighlightBackground));
    float lineWidth = getWidth() - 145 - 45;
    float fractionCompleted;
    if (totalTime > 0)
    {
        fractionCompleted = elapsedTime / totalTime;
    } else {
        fractionCompleted = 0;
    }
     
    
    g.setColour(findColour(ThemeColours::defaultText).withAlpha(0.2f));
    g.drawLine(45,10,lineWidth+45,10, 2.0f);
    
    g.setColour(findColour(ThemeColours::menuHighlightBackground));
    g.drawLine(45,10,lineWidth * fractionCompleted+45,10, 2.0f);
    
}

void ProtocolTimeline::actionListenerCallback(const String& message)
{
    if (!message.equalsIgnoreCase("FINISHED"))
    {
        setCurrentTrial(message.getIntValue());
    }
   
}


String ProtocolTimeline::getTimeString(float timeInSeconds)
{
    // truncate towards zero; if you prefer rounding use std::round
    const int totalSecs = static_cast<int> (timeInSeconds);

    const int mins    = totalSecs / 60;
    const int secs    = totalSecs % 60;

    // %02d → at least 2 digits, pad with zeroes if needed.
    // If mins is 123, it will print "123".
    return juce::String::formatted ("%02d:%02d", mins, secs);
    
}

void ProtocolTimeline::timerCallback()
{
    setElapsedTime(float(Time::currentTimeMillis() - startTime - pauseTime) / 1000.0f);
    
    if (elapsedTime > totalTime)
    {
        pause();
    }
    
}

void ProtocolTimeline::start()
{
    if (!isPaused)
    {
        startTime = Time::currentTimeMillis();
    } else {
        pauseTime += Time::currentTimeMillis() - pauseStart;
    }

    startTimer(100);
    isRunning = true;
    isPaused = false;
    LOGD("Starting protocol timeline");
}

void ProtocolTimeline::pause()
{

    stopTimer();
    
    pauseStart = Time::currentTimeMillis();
    isRunning = false;
    isPaused = true;
    LOGD("Pausing protocol timeline");
}

void ProtocolTimeline::reset()
{
    stopTimer();
    currentTrial = 0;
    setElapsedTime(0);
    isRunning = false;
    isPaused = false;
    pauseTime = 0;
    LOGD("Resetting protocol timeline");
}
 
void ProtocolTimeline::setTotalTime(float timeInSeconds)
{
     totalTime = timeInSeconds;
     repaint();
}
 
void ProtocolTimeline::setElapsedTime(float timeInSeconds)
{
     elapsedTime = timeInSeconds;
     repaint();
}
 
void ProtocolTimeline::setTotalTrials(int numTrials)
{
     totalTrials = numTrials;
     repaint();
}
 
void ProtocolTimeline::setCurrentTrial(int trialNumber)
{
     currentTrial = trialNumber;
     repaint();
}


OptoProtocolCanvas::OptoProtocolCanvas(OptoProtocolGenerator* processor_)
    : processor(processor_)
{
    // Create the viewport
    viewport = std::make_unique<Viewport>();
    viewport->setScrollBarsShown(true, false);
    viewport->setScrollBarThickness(15);
    addAndMakeVisible(viewport.get());
    
    // Create the content component
    protocolInterfaces.add(new OptoProtocolInterface("Protocol 1", viewport.get()));
    
    // Set an initial size for the content component
    protocolInterfaces.getLast()->setSize(getWidth(), 500); // Initial height, will be adjusted in resized()
    
    // Set the content component as the viewport's viewed component
    viewport->setViewedComponent(protocolInterfaces[0], false);

    protocolSelector = std::make_unique<ComboBox>("protocolSelector");
    protocolSelector->addItem("Protocol 1", 1);
    protocolSelector->setSelectedId(1, dontSendNotification);
    protocolSelector->addListener(this);
    addAndMakeVisible(protocolSelector.get());
    
    protocolLabel = std::make_unique<Label>("protocolLabel", "Protocol");
    protocolLabel->setFont(FontOptions ("Inter", "Regular", 15));
    protocolLabel->setJustificationType(Justification::centredLeft);
    addAndMakeVisible(protocolLabel.get());
    
    protocolTimeline = std::make_unique<ProtocolTimeline>();
    addAndMakeVisible(protocolTimeline.get());
    
    protocolInterfaces.getLast()->setTimeline(protocolTimeline.get());
    currentProtocol = protocolInterfaces.getLast()->getProtocol();
    currentProtocol->addActionListener(this);
    
    newProtocolButton = std::make_unique<TextButton>("newProtocolButton");
    newProtocolButton->setButtonText("New");
    newProtocolButton->addListener(this);
    addAndMakeVisible(newProtocolButton.get());
    
    deleteProtocolButton = std::make_unique<TextButton>("deleteProtocolButton");
    deleteProtocolButton->setButtonText("Delete");
    deleteProtocolButton->addListener(this);
    deleteProtocolButton->setEnabled(protocolInterfaces.size() > 1);
    addAndMakeVisible(deleteProtocolButton.get());

    renameProtocolButton = std::make_unique<TextButton>("renameProtocolButton");
    renameProtocolButton->setButtonText("Rename");
    renameProtocolButton->addListener(this);
    addAndMakeVisible(renameProtocolButton.get());
    
    runButton = std::make_unique<TextButton>("runButton");
    runButton->setButtonText("Run");
    runButton->addListener(this);
    addAndMakeVisible(runButton.get());
    
    resetButton = std::make_unique<TextButton>("resetButton");
    resetButton->setButtonText("Reset");
    resetButton->addListener(this);
    addAndMakeVisible(resetButton.get());
}

OptoProtocolCanvas::~OptoProtocolCanvas()
{
    // The viewport will delete the content component when it's no longer needed
    viewport->setViewedComponent(nullptr, false);
}

void OptoProtocolCanvas::actionListenerCallback(const String& message)
{
    OptoProtocolInterface* iface = getCurrentInterface();
    if (message.equalsIgnoreCase("FINISHED"))
    {
        runButton->setButtonText("Run");
        runButton->setEnabled(false);
        if (iface != nullptr)
        {
            iface->setTableRunning(false);
            iface->enable();
        }
    }
    else
    {
        int seqIdx = currentProtocol->getCurrentSequenceIndex();
        int trialNum = message.getIntValue();
        int trialStarted = trialNum - 1; // message is post-increment
        if (seqIdx >= 0 && seqIdx < currentProtocol->sequences.size() && trialStarted >= 0)
        {
            Sequence* seq = currentProtocol->sequences[seqIdx];
            if (trialStarted < seq->getTotalTrials())
            {
                const OptoHardwareConfig* hw = iface != nullptr ? iface->getHardwareConfig() : nullptr;
                String json = trialToNidaqJson(seq, trialStarted, hw);
                processor->sendConfigToNidaqOutput(json);
            }
        }
        if (iface != nullptr)
            iface->setActiveTrial(seqIdx + 1, trialNum);
    }
}

void OptoProtocolCanvas::resized()
{

    // Set the bounds of the protocol selector and run button
    const int margin = 15;
    const int controlHeight = 20;
    const int controlWidth = 150;
    const int buttonWidth = 70;
    const int labelWidth = 180;
    const int headerHeight = margin*4 + controlHeight * 2;

    protocolSelector->setBounds(margin, margin*2, controlWidth, controlHeight);
    protocolLabel->setBounds(margin*2 + controlWidth-10, margin*2, labelWidth, controlHeight);
    
    newProtocolButton->setBounds(margin, margin*3 + controlHeight, buttonWidth, controlHeight);
    deleteProtocolButton->setBounds(margin + buttonWidth + 10, margin*3 + controlHeight, buttonWidth, controlHeight);
    renameProtocolButton->setBounds(margin + (buttonWidth + 10) * 2, margin*3 + controlHeight, buttonWidth, controlHeight);
    
    runButton->setBounds(250, margin*2, buttonWidth, controlHeight);
    resetButton->setBounds(250 + 10 + buttonWidth, margin*2, buttonWidth, controlHeight);
    
    protocolTimeline->setBounds(250, margin*2+controlHeight * 2 -5, 350, controlHeight);


     // Set the viewport below the header
     viewport->setBounds(0, headerHeight, getWidth(), getHeight()-headerHeight);

     // Set the width of the content component to match the viewport's width
    const int selId = protocolSelector->getSelectedId();
    if (selId > 0 && selId <= protocolInterfaces.size())
    {
        auto* pi = protocolInterfaces[selId - 1];
        pi->setSize(viewport->getMaximumVisibleWidth(), pi->getHeight());
    }

}

void OptoProtocolCanvas::updateSettings()
{
    
}

void OptoProtocolCanvas::refreshState()
{
    
}

void OptoProtocolCanvas::refresh()
{
    
}

void OptoProtocolCanvas::buttonClicked(Button* button)
{
    if (button == newProtocolButton.get())
    {
        const int n = protocolInterfaces.size() + 1;
        const String name = "Protocol " + String(n);
        auto* iface = new OptoProtocolInterface(name, viewport.get());
        iface->setTimeline(protocolTimeline.get());
        protocolInterfaces.add(iface);
        protocolSelector->addItem(name, n);
        protocolSelector->setSelectedId(n, dontSendNotification);
        applySelectedProtocol();
    }
    else if (button == renameProtocolButton.get())
    {
        OptoProtocolInterface* iface = getCurrentInterface();
        if (iface == nullptr)
            return;

        AlertWindow w("Rename protocol", "Enter a new name for this protocol.", AlertWindow::QuestionIcon);
        w.addTextEditor("name", iface->getProtocol()->name, "Name:");
        w.addButton("OK", 1, KeyPress(KeyPress::returnKey));
        w.addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));

        if (w.runModalLoop() != 1)
            return;

        const String newName = w.getTextEditorContents("name").trim();
        if (newName.isEmpty())
        {
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Rename protocol", "Name cannot be empty.");
            return;
        }

        iface->getProtocol()->name = newName;
        const int id = protocolSelector->getSelectedId();
        if (id > 0)
        {
            protocolSelector->changeItemText(id, newName);
            protocolSelector->setSelectedId(id, dontSendNotification);
        }
    }
    else if (button == deleteProtocolButton.get())
    {
        if (protocolInterfaces.size() <= 1)
            return;
        OptoProtocolInterface* iface = getCurrentInterface();
        if (iface == nullptr)
            return;
        const String pname = iface->getProtocol()->name;
        if (! AlertWindow::showOkCancelBox(AlertWindow::WarningIcon,
                                           "Delete protocol",
                                           "Are you sure you want to delete \"" + pname + "\"?",
                                           "Delete",
                                           "Cancel",
                                           this))
            return;

        if (protocolTimeline->isRunning)
        {
            protocolTimeline->pause();
            iface->getProtocol()->pause();
        }
        runButton->setButtonText("Run");
        runButton->setEnabled(true);

        iface->getProtocol()->removeActionListener(this);
        iface->getProtocol()->removeActionListener(protocolTimeline.get());

        const int oldIdx = protocolSelector->getSelectedId() - 1;

        viewport->setViewedComponent(nullptr, false);
        currentProtocol = nullptr;

        protocolInterfaces.removeObject(iface, true);

        protocolSelector->clear(dontSendNotification);
        for (int i = 0; i < protocolInterfaces.size(); ++i)
            protocolSelector->addItem(protocolInterfaces[i]->getProtocol()->name, i + 1);

        const int newSelIdx = jmin(oldIdx, protocolInterfaces.size() - 1);
        protocolSelector->setSelectedId(newSelIdx + 1, dontSendNotification);
        applySelectedProtocol();
    }
    else if (button == runButton.get())
    {
        OptoProtocolInterface* iface = getCurrentInterface();
        if (!protocolTimeline->isRunning)
        {
            protocolTimeline->start();
            currentProtocol->run();
            button->setButtonText("Pause");
            if (iface != nullptr)
            {
                iface->setTableRunning(true);
                iface->disable();
            }
        } else {
            protocolTimeline->pause();
            currentProtocol->pause();
            button->setButtonText("Run");
            if (iface != nullptr)
            {
                iface->setTableRunning(false);
                iface->enable();
            }
        }
        
    } else if (button == resetButton.get())
    {
        OptoProtocolInterface* iface = getCurrentInterface();
        if (protocolTimeline->isRunning)
        {
            protocolTimeline->pause();
            currentProtocol->pause();
        }
        runButton->setButtonText("Run");
        protocolTimeline->reset();
        currentProtocol->reset();
        if (iface != nullptr)
        {
            iface->setTableRunning(false);
            iface->enable();
        }
        runButton->setEnabled(true);
    }
}

void OptoProtocolCanvas::comboBoxChanged(ComboBox* comboBox)
{
    if (comboBox == protocolSelector.get())
        applySelectedProtocol();
}

void OptoProtocolCanvas::paint(Graphics& g)
{
    g.fillAll(findColour(ThemeColours::componentBackground));

    g.setColour(findColour(ThemeColours::defaultText));
    g.drawLine(10, 99, (float)getWidth() - 30, 99, 1.0f);
    
}
