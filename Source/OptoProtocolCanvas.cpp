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
#include <tuple>
#include <utility>
using namespace juce;

namespace
{
// Wave-player format (matches nidaq-test.py): sampleRate, maxVoltage, pulse/sine/custom with durations in source samples.
static const double kSourceSampleRate = 30000.0;
static const double kMaxVoltage = 5.0;

static String sequenceToNidaqJson(Sequence* seq, double /*sampleRate*/ = 30000.0)
{
    DynamicObject::Ptr root = new DynamicObject();
    root->setProperty("sampleRate", kSourceSampleRate);
    root->setProperty("maxVoltage", kMaxVoltage);
    root->setProperty("playImmediately", true);
    root->setProperty("patternType", 0);

    if (seq->conditions.size() == 0)
        return JSON::toString(var(root.get()));

    Condition* c = seq->conditions[0];
    float power = jmin(1.0f, jmax(0.0f, c->pulse_power.getFloatValue() / 10000.0f));
    float maxV = (float)(kMaxVoltage * power);
    if (maxV < 0.01f) maxV = (float)kMaxVoltage;

    bool hasPulse = false, hasSine = false, hasCustom = false;
    for (Stimulus* s : c->stimuli)
    {
        if (s->type == PULSE_TRAIN && !hasPulse)
        {
            hasPulse = true;
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
            pulse->setProperty("analogOutputChannel", 0);
            pulse->setProperty("onDuration", onDuration);
            pulse->setProperty("offDuration", offDuration);
            pulse->setProperty("delayDuration", 0);
            pulse->setProperty("repeatNumber", pt->pulse_count.getIntValue());
            int rampSamples = (int)(pt->ramp_duration.getFloatValue() / 1000.0f * kSourceSampleRate);
            pulse->setProperty("rampOnDuration", rampSamples);
            pulse->setProperty("rampOffDuration", rampSamples);
            pulse->setProperty("maxVoltage", maxV);
            root->setProperty("pulse", var(pulse.get()));
        }
        else if (s->type == SINUSOID && !hasSine)
        {
            hasSine = true;
            SineWave* sw = static_cast<SineWave*>(s);
            float durMs = sw->sine_wave_duration.getFloatValue();
            float freqHz = sw->sine_wave_frequency.getFloatValue();
            int cycles = jmax(1, (int)(durMs / 1000.0f * freqHz + 0.5f));

            DynamicObject::Ptr sine = new DynamicObject();
            sine->setProperty("analogOutputChannel", 1);
            sine->setProperty("frequency", (double)freqHz);
            sine->setProperty("cycles", cycles);
            sine->setProperty("delayDuration", 0);
            sine->setProperty("maxVoltage", maxV);
            root->setProperty("sine", var(sine.get()));
        }
        else if (s->type == CUSTOM && !hasCustom)
        {
            CustomStimulus* cs = static_cast<CustomStimulus*>(s);
            if (cs->stimulus_waveform.size() == 0) continue;
            hasCustom = true;
            String str;
            for (int i = 0; i < cs->stimulus_waveform.size(); i++)
            {
                if (i > 0) str << ",";
                float v = jmax(0.0f, jmin(1.0f, cs->stimulus_waveform[i])) * power * (float)kMaxVoltage;
                str << v;
            }
            DynamicObject::Ptr custom = new DynamicObject();
            custom->setProperty("analogOutputChannel", 1);
            custom->setProperty("string", str);
            root->setProperty("custom", var(custom.get()));
        }
    }
    return JSON::toString(var(root.get()));
}
}

ColourSelectorWidget::ColourSelectorWidget(Condition* condition_, OptoProtocolInterface* parent_)
    : condition(condition_), parent(parent_)
{
    redButton = std::make_unique<TextButton>("redButton");
    redButton->setButtonText("638");
    redButton->setClickingTogglesState(true);
    redButton->setToggleState(true, dontSendNotification);
    redButton->setColour(TextButton::buttonColourId, Colours::darkgrey);
    redButton->setColour(TextButton::buttonOnColourId, Colours::red);
    redButton->setColour(TextButton::textColourOnId, Colours::white);
    redButton->setColour(TextButton::textColourOffId, Colours::white);
    redButton->addListener(this);
    addAndMakeVisible(redButton.get());
    redButton->setBounds(46, 0, 40, 20);
    
    blueButton = std::make_unique<TextButton>("blueButton");
    blueButton->setButtonText("450");
    blueButton->setClickingTogglesState(true);
    blueButton->setToggleState(false, dontSendNotification);
    blueButton->setColour(TextButton::buttonColourId, Colours::darkgrey);
    blueButton->setColour(TextButton::buttonOnColourId, Colour(38, 173, 252));
    blueButton->setColour(TextButton::textColourOnId, Colours::white);
    blueButton->setColour(TextButton::textColourOffId, Colours::white);
    blueButton->addListener(this);
    addAndMakeVisible(blueButton.get());
    blueButton->setBounds(0, 0, 40, 20);
    
    wavelengthLabel = std::make_unique<Label>("wavelengthLabel", "Wavelength");
    wavelengthLabel->setFont(FontOptions ("Inter", "Regular", 13.5));
    wavelengthLabel->setJustificationType(Justification::centredLeft);
    addAndMakeVisible(wavelengthLabel.get());
    wavelengthLabel->setBounds(90, 0, 100, 20);
    
}

void ColourSelectorWidget::buttonClicked(Button* button)
{
    if (redButton->getToggleState())
    {
        condition->addWavelength(638);
    } else {
        condition->removeWavelength(638);
    }
        
    if (blueButton->getToggleState())
    {
        condition->addWavelength(450);
    } else {
        condition->removeWavelength(450);
    }
    parent->parameterChangeRequest(nullptr);
    
}


void ColourSelectorWidget::enable()
{
    redButton->setEnabled(true);
    blueButton->setEnabled(true);
    wavelengthLabel->setEnabled(true);

}

void ColourSelectorWidget::disable()
{
    redButton->setEnabled(false);
    blueButton->setEnabled(false);
    wavelengthLabel->setEnabled(false);

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
    
    sourceEditor = std::make_unique<ComboBoxParameterEditor>(&condition->source);
    addAndMakeVisible(sourceEditor.get());
    siteEditor = std::make_unique<SelectedChannelsParameterEditor>(condition->sites.get());
    addAndMakeVisible(siteEditor.get());
    colourSelectorWidget = std::make_unique<ColourSelectorWidget>(condition, parent);
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
}
    

OptoConditionInterface::~OptoConditionInterface()
{
    
}
    
void OptoConditionInterface::resized()
{
    stimulusTypeLabel->setBounds(12, 12, 100, 20);
    sourceEditor->setBounds(190, 15, 180, 20);
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
    sourceEditor->parameterEnabled(true);
    siteEditor->parameterEnabled(true);
    pulsePowerEditor->parameterEnabled(true);
    numRepeatsEditor->parameterEnabled(true);
    
    colourSelectorWidget->enable();
    
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
                                             OptoProtocolInterface* parent_)
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
    
    Array<String> availableSources = {"Probe A", "Probe B"};
    Array<int> sitesPerSource = {14, 14};
    Array<int> availableWavelengths = {638};
    
    Condition* condition = new Condition(parent,                                             availableSources,
                                         sitesPerSource,
                                         availableWavelengths,
                                         sequence);
    
    sequence->addCondition(condition);
    
    PulseTrain* pulseTrain = new PulseTrain(parent,
                                            condition);
    
    
    condition->addStimulus(pulseTrain);
    
    conditionInterfaces.add(new OptoConditionInterface(condition, pulseTrain, parent));
    addAndMakeVisible(conditionInterfaces.getLast());
    
    baselineIntervalEditor = std::make_unique<BoundedValueParameterEditor>(&sequence->baseline_interval);
    addAndMakeVisible(baselineIntervalEditor.get());
    minItiEditor = std::make_unique<BoundedValueParameterEditor>(&sequence->min_iti);
    addAndMakeVisible(minItiEditor.get());
    maxItiEditor = std::make_unique<BoundedValueParameterEditor>(&sequence->max_iti);
    addAndMakeVisible(maxItiEditor.get());
    randomizeEditor = std::make_unique<ToggleParameterEditor>(&sequence->randomize);
    addAndMakeVisible(randomizeEditor.get());
    
    setBounds(0, 0, 0, 230 + conditionInterfaceHeight);
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
        Array<String> availableSources = {"Probe A", "Probe B"};
        Array<int> sitesPerSource = {14, 14};
        Array<int> availableWavelengths = {638};
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
        parent->timeline->setTotalTime(sequence->protocol->getTotalTime());
        parent->timeline->setTotalTrials(sequence->protocol->getTotalTrials());
        parent->refreshConditionsTable();
    }
}

enum ConditionsTableColumns { ColRow = 1, ColSequence, ColCondition, ColProbe, ColWavelength, ColSites, ColLightPower, ColRepeat };

String ConditionsTableModel::getStructureSignature() const
{
    if (!protocol) return {};
    String s;
    for (auto* seq : protocol->sequences)
    {
        for (auto* cond : seq->conditions)
            s << cond->num_repeats.getIntValue() << ",";
        s << (seq->randomize.getBoolValue() ? "1" : "0") << ";";
    }
    return s;
}

void ConditionsTableModel::rebuildRowOrder()
{
    rowOrder.clear();
    if (!protocol) return;
    for (int s = 0; s < protocol->sequences.size(); ++s)
    {
        Sequence* seq = protocol->sequences[s];
        Array<std::pair<int, int>> condRepeat;
        for (int c = 0; c < seq->conditions.size(); ++c)
        {
            int n = seq->conditions[c]->num_repeats.getIntValue();
            for (int r = 0; r < n; ++r)
                condRepeat.add({ c, r });
        }
        if (seq->randomize.getBoolValue() && condRepeat.size() > 1)
        {
            auto& rng = Random::getSystemRandom();
            for (int i = (int)condRepeat.size() - 1; i > 0; --i)
                condRepeat.swap(i, rng.nextInt(i + 1));
        }
        for (auto& p : condRepeat)
            rowOrder.add(std::make_tuple(s + 1, p.first + 1, p.second + 1));
    }
}

void ConditionsTableModel::rowToIndices(int row, int& seqIdx, int& condIdx, int& repeatIdx) const
{
    if (row < 0 || row >= rowOrder.size()) { seqIdx = condIdx = repeatIdx = 0; return; }
    const auto& t = rowOrder[row];
    seqIdx = std::get<0>(t);
    condIdx = std::get<1>(t);
    repeatIdx = std::get<2>(t);
}

String ConditionsTableModel::getConditionName(int seqIdx, int condIdx) const
{
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
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return {};
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (condIdx < 1 || condIdx > seq->conditions.size()) return {};
    return seq->conditions[condIdx - 1]->source.getValueAsString();
}

String ConditionsTableModel::getWavelengthString(int seqIdx, int condIdx) const
{
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return {};
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (condIdx < 1 || condIdx > seq->conditions.size()) return {};
    const auto& wl = seq->conditions[condIdx - 1]->availableWavelengths;
    if (wl.isEmpty()) return {};
    String s = String(wl[0]);
    for (int i = 1; i < wl.size(); ++i)
        s << ", " << wl[i];
    return s;
}

String ConditionsTableModel::getSitesString(int seqIdx, int condIdx) const
{
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return {};
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (condIdx < 1 || condIdx > seq->conditions.size() || !seq->conditions[condIdx - 1]->sites) return {};
    Condition* cond = seq->conditions[condIdx - 1];
    int selected = cond->sites->getArrayValue().size();
    int total = 0;
    if (cond->sitesPerSource.size() > 0)
    {
        int srcIdx = (int)cond->source.getValue();
        if (srcIdx >= 0 && srcIdx < cond->sitesPerSource.size())
            total = cond->sitesPerSource[srcIdx];
        else
            total = cond->sitesPerSource[0];
    }
    return String(selected) + "/" + String(total);
}

String ConditionsTableModel::getLightPowerString(int seqIdx, int condIdx) const
{
    if (!protocol || seqIdx < 1 || seqIdx > protocol->sequences.size()) return {};
    Sequence* seq = protocol->sequences[seqIdx - 1];
    if (condIdx < 1 || condIdx > seq->conditions.size()) return {};
    return seq->conditions[condIdx - 1]->pulse_power.getValueAsString();
}

int ConditionsTableModel::getNumRows()
{
    if (!protocol) return 0;
    String sig = getStructureSignature();
    if (sig != lastStructureSignature)
    {
        lastStructureSignature = sig;
        rebuildRowOrder();
    }
    return rowOrder.size();
}

void ConditionsTableModel::paintRowBackground(Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(Colours::lightblue.withAlpha(0.3f));
    else if (rowNumber % 2 == 1)
        g.fillAll(Colours::white.withAlpha(0.05f));
}

void ConditionsTableModel::paintCell(Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    int seqIdx, condIdx, repeatIdx;
    rowToIndices(rowNumber, seqIdx, condIdx, repeatIdx);
    g.setColour(Colours::white);
    g.setFont(Font(12.0f));
    String text;
    switch (columnId)
    {
        case ColRow: text = String(rowNumber + 1); break;
        case ColSequence: text = String(seqIdx); break;
        case ColCondition: text = getConditionName(seqIdx, condIdx); break;
        case ColProbe: text = getProbeName(seqIdx, condIdx); break;
        case ColWavelength: text = getWavelengthString(seqIdx, condIdx); break;
        case ColSites: text = getSitesString(seqIdx, condIdx); break;
        case ColLightPower: text = getLightPowerString(seqIdx, condIdx); break;
        case ColRepeat: text = String(repeatIdx); break;
        default: break;
    }
    g.drawText(text, 4, 0, width - 6, height, Justification::centredLeft, true);
}

ConditionsTable::ConditionsTable()
{
    table.setModel(&model);
    table.getHeader().addColumn("Trial", ColRow, 44, 36, 80);
    table.getHeader().addColumn("Sequence", ColSequence, 62, 50, 80);
    table.getHeader().addColumn("Condition", ColCondition, 88, 70, 120);
    table.getHeader().addColumn("Probe", ColProbe, 68, 56, 100);
    table.getHeader().addColumn("Wavelength", ColWavelength, 90, 70, 120);
    table.getHeader().addColumn("Sites", ColSites, 85, 60, 150);
    table.getHeader().addColumn("Light Power", ColLightPower, 72, 56, 100);
    table.getHeader().addColumn("Repeat", ColRepeat, 48, 40, 80);
    table.setHeaderHeight(22);
    table.setRowHeight(30);
    addAndMakeVisible(table);
}

void ConditionsTable::setProtocol(Protocol* p)
{
    model.setProtocol(p);
}

void ConditionsTable::refreshTable()
{
    table.updateContent();
    table.repaint();
}

int ConditionsTable::getPreferredHeight()
{
    int n = model.getNumRows();
    return kHeaderHeight + n * kRowHeight;
}

void ConditionsTable::resized()
{
    table.setBounds(getLocalBounds());
}

const int kConditionsTableWidth = 565;
const int kConditionsTableGap = 10;
/** Sequences column width; table is placed immediately to its right. */
const int kSequencesColumnWidth = 400;

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
    int tableHeight = 30 + (conditionsTable ? conditionsTable->getPreferredHeight() : 0);
    int totalHeight = jmax(sequencesHeight, tableHeight);
    int currentScrollDistance = viewport->getViewPositionY();
    setBounds(0, 0, getWidth(), totalHeight);
    viewport->setViewPosition(0, currentScrollDistance);
}

void OptoProtocolInterface::resized()
{
    int leftMargin = 15;
    int currentHeight = 30;
    for (auto interface : sequenceInterfaces)
    {
        interface->setBounds(leftMargin, currentHeight, kSequencesColumnWidth, interface->getHeight());
        currentHeight += interface->getHeight();
    }
    addSequenceButton->setBounds(leftMargin + 15, currentHeight + 5, 150, 20);
    int tableX = leftMargin + kSequencesColumnWidth + kConditionsTableGap;
    if (conditionsTable)
        conditionsTable->setBounds(tableX, 30, kConditionsTableWidth, conditionsTable->getPreferredHeight());
}

void OptoProtocolInterface::paint(Graphics& g)
{
    // Draw the background
    g.fillAll(findColour(ThemeColours::componentBackground));


}


void OptoProtocolInterface::buttonClicked(Button* button)
{
    if (button == addSequenceButton.get())
    {
        LOGD("Add sequence button clicked");
        
        Sequence* defaultSequence = new Sequence(this, protocol.get());
        
        protocol->sequences.add(defaultSequence);
        int numSequences = protocol->sequences.size();
        sequenceInterfaces.add(new OptoSequenceInterface("Sequence " + String(numSequences), defaultSequence, this));
        addAndMakeVisible(sequenceInterfaces.getLast());
       
        updateBounds(sequenceInterfaces.getLast()->getHeight());
        
        timeline->setTotalTime(protocol->getTotalTime());
        timeline->setTotalTrials(protocol->getTotalTrials());
        refreshConditionsTable();
    }
}

void OptoProtocolInterface::removeConditionInterface(OptoConditionInterface* conditionInterface)
{
    for (auto seq : sequenceInterfaces)
        seq->removeCondition(conditionInterface);
    refreshConditionsTable();
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
    timeline->setTotalTime(protocol->getTotalTime());
    timeline->setTotalTrials(protocol->getTotalTrials());
    updateBounds(0);
    resized();
    refreshConditionsTable();
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
    }
    
    timeline->reset();
    protocol->reset();
    protocol->createTrials();
    
    timeline->setTotalTime(protocol->getTotalTime());
    timeline->setTotalTrials(protocol->getTotalTrials());
    // Defer table refresh so parameter value is committed (fixes single-sequence num_repeats update)
    Timer::callAfterDelay(0, [this]()
    {
        refreshConditionsTable();
        updateBounds(0);
        resized();
    });
}

void OptoProtocolInterface::setTimeline(ProtocolTimeline* timeline_)
{
    timeline = timeline_;
    
    timeline->setTotalTime(protocol->getTotalTime());
    timeline->setTotalTrials(protocol->getTotalTrials());
    protocol->addActionListener(timeline);
}

void OptoProtocolInterface::refreshConditionsTable()
{
    if (conditionsTable)
        conditionsTable->refreshTable();
}

void OptoProtocolInterface::enable()
{
    for (auto sequence : sequenceInterfaces)
    {
        sequence->enable();
    }
    
    addSequenceButton->setEnabled(true);
}

void OptoProtocolInterface::disable()
{
    LOGD("Disabling OptoProtocolInterface ");
    
    for (auto sequence : sequenceInterfaces)
    {
        sequence->disable();
    }
    
    addSequenceButton->setEnabled(false);
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
    protocolInterfaces.add(new OptoProtocolInterface("Optotagging 1", viewport.get()));
    
    // Set an initial size for the content component
    protocolInterfaces.getLast()->setSize(getWidth(), 500); // Initial height, will be adjusted in resized()
    
    // Set the content component as the viewport's viewed component
    viewport->setViewedComponent(protocolInterfaces[0], false);

    protocolSelector = std::make_unique<ComboBox>("protocolSelector");
    protocolSelector->addItem("Optotagging 1", 1);
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
    addAndMakeVisible(deleteProtocolButton.get());
    
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
    if (message.equalsIgnoreCase("FINISHED"))
    {
        runButton->setButtonText("Run");
        runButton->setEnabled(false);
        protocolInterfaces.getLast()->enable();
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
    
    runButton->setBounds(250, margin*2, buttonWidth, controlHeight);
    resetButton->setBounds(250 + 10 + buttonWidth, margin*2, buttonWidth, controlHeight);
    
    protocolTimeline->setBounds(250, margin*2+controlHeight * 2 -5, 350, controlHeight);


     // Set the viewport below the header
     viewport->setBounds(0, headerHeight, getWidth(), getHeight()-headerHeight);

     // Set the width of the content component to match the viewport's width
    protocolInterfaces[0]->setSize(viewport->getMaximumVisibleWidth(), protocolInterfaces[0]->getHeight());

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
    if (button == runButton.get())
    {
        if (!protocolTimeline->isRunning)
        {
            if (currentProtocol->sequences.size() > 0)
            {
                Sequence* seq1 = currentProtocol->sequences[0];
                String json = sequenceToNidaqJson(seq1);
                LOGC("NIDAQ config: ", json);
                processor->sendConfigToNidaqOutput(json);
            }
            protocolTimeline->start();
            currentProtocol->run();
            button->setButtonText("Pause");
            
        } else {
            protocolTimeline->pause();
            currentProtocol->pause();
            button->setButtonText("Run");
        }
        
        protocolInterfaces.getLast()->disable();
        
    } else if (button == resetButton.get())
    {
        protocolTimeline->reset();
        currentProtocol->reset();
        runButton->setEnabled(true);
        protocolInterfaces.getLast()->enable();
    }
}

void OptoProtocolCanvas::comboBoxChanged(ComboBox* comboBox)
{
    
}

void OptoProtocolCanvas::paint(Graphics& g)
{
    g.fillAll(findColour(ThemeColours::componentBackground));

    g.setColour(findColour(ThemeColours::defaultText));
    g.drawLine(10, 99, (float)getWidth() - 30, 99, 1.0f);
    
}
