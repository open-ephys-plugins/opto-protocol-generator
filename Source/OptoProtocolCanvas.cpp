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
#include <juce_gui_basics/juce_gui_basics.h>
using namespace juce;

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
    if (button == addConditionButton.get())
    {
        // add stimulus
        LOGD("Add condition button clicked.");
        
        Array<String> availableSources = {"Probe A", "Probe B"};
        Array<int> sitesPerSource = {14, 14};
        Array<int> availableWavelengths = {638};
        
        Condition* condition =new Condition(parent,                                                 availableSources,
                                            sitesPerSource,
                                            availableWavelengths,
                                            sequence);
        
        sequence->addCondition(condition);
        
        PopupMenu m;
        m.setLookAndFeel (&getLookAndFeel());

        m.addItem (1, "Pulse Train", true);
        m.addItem (2, "Sine Wave", true);
        m.addItem (3, "Ramp", true);
        m.addItem (4, "Custom", true);
        
        const int result = m.showMenu (PopupMenu::Options {}.withStandardItemHeight (20));

        if (result == 1)
        {
            PulseTrain* pulseTrain = new PulseTrain(parent,
                                                    condition);
            
            condition->addStimulus(pulseTrain);
            
            conditionInterfaces.add(new OptoConditionInterface(condition, pulseTrain, parent));
            
        } else if (result == 2)
        {
            SineWave* sineWave = new SineWave(parent,
                                                    condition);
            
            condition->addStimulus(sineWave);
            
            conditionInterfaces.add(new OptoConditionInterface(condition, sineWave, parent));
            
        } else if (result == 3)
        {
            RampStimulus* rampStimulus = new RampStimulus(parent,
                                                    condition);
            
            condition->addStimulus(rampStimulus);
            
            conditionInterfaces.add(new OptoConditionInterface(condition, rampStimulus, parent));
        } else if (result == 4)
        {
            CustomStimulus* customStimulus = new CustomStimulus(parent,
                                                    condition);
            
            condition->addStimulus(customStimulus);
            
            conditionInterfaces.add(new OptoConditionInterface(condition, customStimulus, parent));
        }
        
        addAndMakeVisible(conditionInterfaces.getLast());
        
        int numInterfaces = conditionInterfaces.size();
        setBounds(0,0,0,230 + (10 + conditionInterfaceHeight) * numInterfaces);
        parent->resized();
        parent->updateBounds(conditionInterfaceHeight-20);
        
        parent->timeline->setTotalTime(sequence->protocol->getTotalTime());
        parent->timeline->setTotalTrials(sequence->protocol->getTotalTrials());
    
    }
}
    

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
    
}


OptoProtocolInterface::~OptoProtocolInterface()
{
    // Destructor implementation
}

void OptoProtocolInterface::updateBounds(int expandBy)
{
    int currentHeight = 90;
    
    for (auto interface : sequenceInterfaces)
    {
        currentHeight += interface->getHeight();
    }
    
    int currentScrollDistance = viewport->getViewPositionY();
    setBounds(0, 0, getWidth(), currentHeight);
    viewport->setViewPosition(0, currentScrollDistance);
}

void OptoProtocolInterface::resized()
{

    int leftMargin = 15;
    int currentHeight = 30;
    
    for (auto interface : sequenceInterfaces)
    {
        interface->setBounds(leftMargin, currentHeight, getWidth()-leftMargin, interface->getHeight());
        currentHeight += interface->getHeight();
    }
    
    addSequenceButton->setBounds(leftMargin + 15, currentHeight+5, 150, 20);
    
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
    }
}

void OptoProtocolInterface::removeConditionInterface(OptoConditionInterface* conditionInterface)
{
    for (auto seq : sequenceInterfaces)
    {
        seq->removeCondition(conditionInterface);
    }
    
    resized();
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
    
}

void OptoProtocolInterface::setTimeline(ProtocolTimeline* timeline_)
{
    timeline = timeline_;
    
    timeline->setTotalTime(protocol->getTotalTime());
    timeline->setTotalTrials(protocol->getTotalTrials());
    protocol->addActionListener(timeline);
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
