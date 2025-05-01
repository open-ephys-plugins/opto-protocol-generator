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

#ifndef OPTOPROTOCOLCANVAS_H_INCLUDED
#define OPTOPROTOCOLCANVAS_H_INCLUDED

#include <VisualizerWindowHeaders.h>

#include "Protocol.h"

class OptoProtocolGenerator;
class OptoProtocolInterface;

/** 
  Interface for selecting stimulus colours
 */

class ColourSelectorWidget : public Component,
                       public Button::Listener
{
public:
    /** Constructor */
    ColourSelectorWidget(Stimulus* stimulus,
                   OptoProtocolInterface* parent);
    
    /** Destructor */
    ~ColourSelectorWidget() { }
    
    /** Respond to button clicks */
    void buttonClicked(Button* button) override;
    
private:
    /** Buttons */
    std::unique_ptr<TextButton> redButton;
    std::unique_ptr<TextButton> blueButton;
    std::unique_ptr<Label> wavelengthLabel;
    
    Stimulus* stimulus;
    OptoProtocolInterface* parent;
};


/**
* Interface for editing an opto sequence
*/
class OptoStimulusInterface : public Component
{
public:

    /** Constructor */
    OptoStimulusInterface(Stimulus* stimulus,
                          OptoProtocolInterface* parent);
    
    /** Destructor */
    ~OptoStimulusInterface();
    
    /** Sets component layout */
    void resized() override;
    
   /** Draws the content background */
   void paint(Graphics& g) override;
    
protected:
    
    std::unique_ptr<ComboBoxParameterEditor> sourceEditor;
    std::unique_ptr<SelectedChannelsParameterEditor> siteEditor;
    std::unique_ptr<ColourSelectorWidget> colourSelectorWidget;
    
    Stimulus* stimulus;
    OptoProtocolInterface* parent;

};

/**
* Interface for editing a pulse train stimulus
*/
class PulseTrainInterface : public OptoStimulusInterface
{
public:

    /** Constructor */
    PulseTrainInterface(PulseTrain* pulseTrain,
                          OptoProtocolInterface* parent);
    
    /** Destructor */
    ~PulseTrainInterface();
    
    /** Sets component layout */
    void resized() override;

    
private:
    
    std::unique_ptr<Label> stimulusTypeLabel;
    
    std::unique_ptr<BoundedValueParameterEditor> pulseWidthEditor;
    std::unique_ptr<BoundedValueParameterEditor> pulseFrequencyEditor;
    std::unique_ptr<BoundedValueParameterEditor> pulseCountEditor;
    std::unique_ptr<BoundedValueParameterEditor> pulsePowerEditor;
    
    PulseTrain* pulse_train;

};

/**
* Interface for editing an opto sequence
*/
class OptoSequenceInterface : public Component,
                              public Button::Listener
{
public:

    /** Constructor */
    OptoSequenceInterface(const String& name,
                          Sequence* sequence,
                          OptoProtocolInterface* parent);
    
    /** Destructor */
    ~OptoSequenceInterface();
    
    /** Sets component layout */
    void resized() override;
    
   /** Draws the content background */
   void paint(Graphics& g) override;
    
    /** Button clicked*/
    void buttonClicked(Button* button) override;
    
private:
    
    OwnedArray<OptoStimulusInterface> stimulusInterfaces;
    
    std::unique_ptr<TextButton> addStimulusButton;
    std::unique_ptr<Label> sequenceNameLabel;
    
    std::unique_ptr<BoundedValueParameterEditor> baselineIntervalEditor;
    std::unique_ptr<BoundedValueParameterEditor> minItiEditor;
    std::unique_ptr<BoundedValueParameterEditor> maxItiEditor;
    std::unique_ptr<ToggleParameterEditor> randomizeEditor;
    
    Sequence* sequence;
    OptoProtocolInterface* parent;
    
    const int stimInterfaceHeight = 146;
    const int stimInterfaceWidth = 365;

};


/**
* Interface for editing an opto protocol
*/
class OptoProtocolInterface : public Component,
                              public Button::Listener,
                              public ParameterOwner
{
public:

    /** Constructor */
    OptoProtocolInterface(const String& name, Viewport* viewport);
    
    /** Destructor */
    ~OptoProtocolInterface();
    
    /** Sets component layout */
    void resized() override;
    
    /** Changes bounds to fit sequence interfaces */
    void updateBounds(int expandBy = 0);
    
   /** Draws the content background */
   void paint(Graphics& g) override;
    
    /** Responds to button click*/
    void buttonClicked(Button* button) override;
    
    /** Responds to parameter changes */
    void parameterChangeRequest(Parameter* parameter) override;
    
private:
    
    OwnedArray<OptoSequenceInterface> sequenceInterfaces;
    
    std::unique_ptr<TextButton> addSequenceButton;
    
    std::unique_ptr<Protocol> protocol;
    
    Viewport* viewport;

};

/**
* Shows a timeline for the currently selected protocol
*/
class ProtocolTimeline : public Component
{
public:

    /** Constructor */
    ProtocolTimeline() { }
    
    /** Destructor */
    ~ProtocolTimeline() { }
    
   /** Draws the content background */
   void paint(Graphics& g) override;
    
    /** Sets the total time */
    void setTotalTime(float timeInSeconds);
    
    /** Sets the elapsed time */
    void setElapsedTime(float timeInSeconds);
    
    /** Sets the total number of trials */
    void setTotalTrials(int numTrials);
    
    /** Sets the elapsed number of trials */
    void setElapsedTrials(int numTrials);
    
private:
    
    /** Convert seconds to string */
    String getTimeString(float time);
    
    float totalTime = 0;
    float elapsedTime = 0;
    int totalTrials = 0;
    int elapsedTrials = 0;

};

/**
* 
	Holds a drop-down menu for selecting protocols,
    a timeline for the currently selected protocol,
    plus a viewport for scrolling through the protocol
    intefaces.

*/
class OptoProtocolCanvas : public Visualizer,
                            public Button::Listener,
                            public ComboBox::Listener
{
public:

	/** Constructor */
	OptoProtocolCanvas(OptoProtocolGenerator* processor);

	/** Destructor */
	~OptoProtocolCanvas();

	/** Updates boundaries of sub-components whenever the canvas size changes */
	void resized() override;
    
    /** Called by the update() method to allow the visualizer to update its custom settings.*/
    void updateSettings() override;

	/** Called when the visualizer's tab becomes visible again */
	void refreshState() override;

	/** Called instead of "repaint()" to avoid re-painting sub-components*/
	void refresh() override;

	/** Draws the canvas background */
	void paint(Graphics& g) override;

    /** Button click callback */
    void buttonClicked(Button* button) override;

    /** ComboBox selection callback */
    void comboBoxChanged(ComboBox* comboBox) override;

private:

    /** ComboBox for selecting a protocol */
    std::unique_ptr<ComboBox> protocolSelector;
    
    /** Timeline for the current protocol */
    std::unique_ptr<ProtocolTimeline> protocolTimeline;
    
    /** Button for running a protocol */
    std::unique_ptr<TextButton> runButton;

    /** Label for the protocol combo */
    std::unique_ptr<Label> protocolLabel;

    /** Viewport to enable scrolling */
    std::unique_ptr<Viewport> viewport;
    
    /** Interface for editing protocol parameters */
    OwnedArray<OptoProtocolInterface> protocolInterfaces;

	/** Pointer to the processor class */
	OptoProtocolGenerator* processor;

	/** Generates an assertion if this class leaks */
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OptoProtocolCanvas);
};


#endif // OPTOPROTOCOLCANVAS_H_INCLUDED
