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

ColourSelectorWidget::ColourSelectorWidget(Stimulus* stimulus_, OptoProtocolInterface* parent_)
    : stimulus(stimulus_), parent(parent_)
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
    
}


PulseTrainInterface::PulseTrainInterface(PulseTrain* pulse_train_,
                                             OptoProtocolInterface* parent_)
    : OptoStimulusInterface(pulse_train_, parent_), pulse_train(pulse_train_)
{
    
    pulseWidthEditor = std::make_unique<BoundedValueParameterEditor>(&pulse_train->pulse_width);
    addAndMakeVisible(pulseWidthEditor.get());
    pulseFrequencyEditor = std::make_unique<BoundedValueParameterEditor>(&pulse_train->pulse_frequency);
    addAndMakeVisible(pulseFrequencyEditor.get());
    pulseCountEditor = std::make_unique<BoundedValueParameterEditor>(&pulse_train->pulse_count);
    addAndMakeVisible(pulseCountEditor.get());
    pulsePowerEditor = std::make_unique<BoundedValueParameterEditor>(&pulse_train->pulse_power);
    addAndMakeVisible(pulsePowerEditor.get());
    
    stimulusTypeLabel = std::make_unique<Label>("stimulusTypeLabel", "Pulse train");
    stimulusTypeLabel->setFont(FontOptions ("Inter", "Regular", 17));
    stimulusTypeLabel->setJustificationType(Justification::centredLeft);
    addAndMakeVisible(stimulusTypeLabel.get());
    
    setBounds(0, 0, 0, 400);
}
    

PulseTrainInterface::~PulseTrainInterface()
{
    
}
    
void PulseTrainInterface::resized()
{
    
    OptoStimulusInterface::resized();
    
    stimulusTypeLabel->setBounds(12, 12, 100, 20);
    pulseWidthEditor->setBounds(190, 50, 150, 20);
    pulseFrequencyEditor->setBounds(190, 80, 150, 20);
    pulseCountEditor->setBounds(190, 110, 150, 20);
    pulsePowerEditor->setBounds(15, 110, 150, 20);
    
}


OptoStimulusInterface::OptoStimulusInterface(Stimulus* stimulus_,
                                             OptoProtocolInterface* parent_)
    : stimulus(stimulus_), parent(parent_)
{
    
    sourceEditor = std::make_unique<ComboBoxParameterEditor>(&stimulus->source);
    addAndMakeVisible(sourceEditor.get());
    siteEditor = std::make_unique<SelectedChannelsParameterEditor>(stimulus->sites.get());
    addAndMakeVisible(siteEditor.get());
    colourSelectorWidget = std::make_unique<ColourSelectorWidget>(stimulus, parent);
    addAndMakeVisible(colourSelectorWidget.get());
    
    setBounds(0, 0, 0, 400);
}
    

OptoStimulusInterface::~OptoStimulusInterface()
{
    
}
    
void OptoStimulusInterface::resized()
{
    sourceEditor->setBounds(190, 15, 180, 20);
    colourSelectorWidget->setBounds(15, 50, 180, 20);
    siteEditor->setBounds(15, 80, 150, 20);
}
    
void OptoStimulusInterface::paint(Graphics& g)
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
    
    addStimulusButton = std::make_unique<TextButton>("addStimulusButton");
    addStimulusButton->setButtonText("Add Stimulus");
    addStimulusButton->addListener(this);
    addAndMakeVisible(addStimulusButton.get());
    
    sequence->conditions.add(new Condition(parent, sequence));
    
    Array<String> availableSources = {"Probe A", "Probe B"};
    Array<int> sitesPerSource = {14, 14};
    Array<int> availableWavelengths = {450, 538};
    
    PulseTrain* pulseTrain = new PulseTrain(parent, 
                                            availableSources,
                                            sitesPerSource,
                                            availableWavelengths,
                                            sequence->conditions.getLast());
    
    
    sequence->conditions.getLast()->stimuli.add(pulseTrain);
    
    stimulusInterfaces.add(new PulseTrainInterface(pulseTrain, parent));
    addAndMakeVisible(stimulusInterfaces.getLast());
    
    baselineIntervalEditor = std::make_unique<BoundedValueParameterEditor>(&sequence->baseline_interval);
    addAndMakeVisible(baselineIntervalEditor.get());
    minItiEditor = std::make_unique<BoundedValueParameterEditor>(&sequence->min_iti);
    addAndMakeVisible(minItiEditor.get());
    maxItiEditor = std::make_unique<BoundedValueParameterEditor>(&sequence->max_iti);
    addAndMakeVisible(maxItiEditor.get());
    randomizeEditor = std::make_unique<ToggleParameterEditor>(&sequence->randomize);
    addAndMakeVisible(randomizeEditor.get());
    
    setBounds(0, 0, 0, 230 + stimInterfaceHeight);
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
    
    for (auto interface : stimulusInterfaces)
    {
        interface->setBounds(15, currentHeight, stimInterfaceWidth, stimInterfaceHeight);
        currentHeight += stimInterfaceHeight + 10;
    }
    
    addStimulusButton->setBounds(265, currentHeight+6, 100, 20);
}
    
void OptoSequenceInterface::paint(Graphics& g)
{
    g.setColour(findColour(ThemeColours::defaultText));
    g.drawLine(93, 31, 365, 32, 1.0f);

}


void OptoSequenceInterface::buttonClicked(Button* button)
{
    if (button == addStimulusButton.get())
    {
        // add stimulus
        LOGD("Add stimulus button clicked.");
        
        Array<String> availableSources = {"Probe A", "Probe B"};
        Array<int> sitesPerSource = {14, 14};
        Array<int> availableWavelengths = {450, 538};
        
        sequence->conditions.add(new Condition(parent, sequence));
        
        PulseTrain* pulseTrain = new PulseTrain(parent,
                                                availableSources,
                                                sitesPerSource,
                                                availableWavelengths,
                                                sequence->conditions.getLast());
        
        sequence->conditions.getLast()->stimuli.add(pulseTrain);
        
        stimulusInterfaces.add(new PulseTrainInterface(pulseTrain, parent));
        addAndMakeVisible(stimulusInterfaces.getLast());
        
        int numInterfaces =stimulusInterfaces.size();
        setBounds(0,0,0,230 + (10 + stimInterfaceHeight) * numInterfaces);
        parent->updateBounds(stimInterfaceHeight);
    }
}
    

OptoProtocolInterface::OptoProtocolInterface(const String& name, Viewport* viewport_)
    : ParameterOwner(ParameterOwner::OTHER), viewport(viewport_)
{

    protocol = std::make_unique<Protocol>(name, this);
    
    Sequence* defaultSequence = new Sequence(this, protocol.get());
    
    protocol->sequences.add(defaultSequence);
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
    int currentHeight = 60;
    
    for (auto interface : sequenceInterfaces)
    {
        currentHeight += interface->getHeight();
    }
    
    int currentScrollDistance = viewport->getViewPositionY();
    setBounds(0, 0, getWidth(), currentHeight);
    viewport->setViewPosition(0, currentScrollDistance + expandBy);
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
    
    addSequenceButton->setBounds(leftMargin + 15, currentHeight, 150, 20);
    
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
    }
}


void OptoProtocolInterface::parameterChangeRequest(Parameter* parameter)
{
    LOGD("Parameter name: ", parameter->getName(),
         ", original value: ", parameter->getValueAsString());
    
    parameter->updateValue();
    
    LOGD("Parameter name: ", parameter->getName(),
         ", new value: ", parameter->getValueAsString());
    
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
    protocolInterfaces.getLast()->setSize(getWidth(), 800); // Initial height, will be adjusted in resized()
    
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
    
    runButton = std::make_unique<TextButton>("runButton");
    runButton->setButtonText("Run");
    runButton->addListener(this);
    addAndMakeVisible(runButton.get());
}

OptoProtocolCanvas::~OptoProtocolCanvas()
{
    // The viewport will delete the content component when it's no longer needed
    viewport->setViewedComponent(nullptr, false);
}

void OptoProtocolCanvas::resized()
{

    // Set the bounds of the protocol selector and run button
    const int margin = 15;
    const int controlHeight = 20;
    const int controlWidth = 150;
    const int labelWidth = 180;
    const int headerHeight = margin*3 + controlHeight;

    protocolSelector->setBounds(margin, margin*2, controlWidth, controlHeight);
    protocolLabel->setBounds(margin*2 + controlWidth-5, margin*2, labelWidth, controlHeight);
    runButton->setBounds(margin*3 + controlWidth + labelWidth, margin*2, 100, controlHeight);

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
    
}

void OptoProtocolCanvas::comboBoxChanged(ComboBox* comboBox)
{
    
}

void OptoProtocolCanvas::paint(Graphics& g)
{
    g.fillAll(findColour(ThemeColours::componentBackground));

    g.setColour(findColour(ThemeColours::defaultText));
    g.drawLine(10, 64, (float)getWidth() - 30, 64, 1.0f);
    
}
