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
* Interface for editing a pulse train stimulus
*/
class PulseTrainInterface : public Component
{
public:

    /** Constructor */
    PulseTrainInterface(PulseTrain* pulseTrain,
                          OptoProtocolInterface* parent);
    
    /** Destructor */
    ~PulseTrainInterface();
    
    /** Sets component layout */
    void resized() override;

    /** Enables the PulseTrainInterface*/
    void enable();
    
    /** Disables the PulseTrainInterface */
    void disable();
    
private:
    
    std::unique_ptr<BoundedValueParameterEditor> pulseWidthEditor;
    std::unique_ptr<BoundedValueParameterEditor> pulseFrequencyEditor;
    std::unique_ptr<BoundedValueParameterEditor> pulseCountEditor;
    std::unique_ptr<BoundedValueParameterEditor> rampDurationEditor;
    
    PulseTrain* pulse_train;
    OptoProtocolInterface* parent;

};

/**
  Interface for selecting stimulus colours
 */

class ColourSelectorWidget : public Component,
                       public Button::Listener
{
public:
    /** Constructor */
    ColourSelectorWidget(Condition* condition,
                   OptoProtocolInterface* parent);
    
    /** Destructor */
    ~ColourSelectorWidget() { }
    
    /** Respond to button clicks */
    void buttonClicked(Button* button) override;
    
    /** Enables the colour selector widget */
    void enable();
    
    /** Disables the colour selector widget */
    void disable();
    
private:
    /** Buttons */
    std::unique_ptr<TextButton> redButton;
    std::unique_ptr<TextButton> blueButton;
    std::unique_ptr<Label> wavelengthLabel;
    
    Condition* condition;
    OptoProtocolInterface* parent;
};


/**
* Interface for editing an opto condition
*/
class OptoConditionInterface : public Component
{
public:

    /** Constructor */
    OptoConditionInterface(Condition* condition,
                           Stimulus* stimulus,
                          OptoProtocolInterface* parent);
    
    /** Destructor */
    ~OptoConditionInterface();
    
    /** Sets component layout */
    void resized() override;
    
    /** Enables the condition interface */
    void enable();
    
    /** Disables the condition interface */
    void disable();
    
   /** Draws the content background */
   void paint(Graphics& g) override;
    
protected:
    
    std::unique_ptr<Label> stimulusTypeLabel;
    
    std::unique_ptr<ComboBoxParameterEditor> sourceEditor;
    std::unique_ptr<SelectedChannelsParameterEditor> siteEditor;
    std::unique_ptr<ColourSelectorWidget> colourSelectorWidget;
    std::unique_ptr<BoundedValueParameterEditor> pulsePowerEditor;
    std::unique_ptr<BoundedValueParameterEditor> numRepeatsEditor;
    
    std::unique_ptr<PulseTrainInterface> pulseTrainInterface;
    Condition* condition;
    Stimulus* stimulus;
    OptoProtocolInterface* parent;

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
    
    /** Enables the sequence interface */
    void enable();
    
    /** Disables the sequence interface */
    void disable();
    
    /** Button clicked*/
    void buttonClicked(Button* button) override;
    
private:
    
    OwnedArray<OptoConditionInterface> conditionInterfaces;
    
    std::unique_ptr<TextButton> addConditionButton;
    std::unique_ptr<Label> sequenceNameLabel;
    
    std::unique_ptr<BoundedValueParameterEditor> baselineIntervalEditor;
    std::unique_ptr<BoundedValueParameterEditor> minItiEditor;
    std::unique_ptr<BoundedValueParameterEditor> maxItiEditor;
    std::unique_ptr<ToggleParameterEditor> randomizeEditor;
    
    Sequence* sequence;
    OptoProtocolInterface* parent;
    
    const int conditionInterfaceHeight = 176;
    const int conditionInterfaceWidth = 365;

};


/**
* Shows a timeline for the currently selected protocol
*/
class ProtocolTimeline : public Component,
                         public Timer,
                         public ActionListener
{
public:

    /** Constructor */
    ProtocolTimeline() { }
    
    /** Destructor */
    ~ProtocolTimeline() { }
    
   /** Draws the content background */
   void paint(Graphics& g) override;
    
    /** Starts the timeline */
    void start();

    /** Pauses the timeline*/
    void pause();
    
    /** Resets the timeline */
    void reset();
    
    /** Sets the total time */
    void setTotalTime(float timeInSeconds);
    
    /** Sets the elapsed time */
    void setElapsedTime(float timeInSeconds);
    
    /** Sets the total number of trials */
    void setTotalTrials(int numTrials);
    
    /** Sets the current  trial number */
    void setCurrentTrial(int trialNumber);
    
    /** Receives a trial update notification */
    void actionListenerCallback(const String& message) override;
    
    /** Tracks state of the timeline */
    bool isRunning = false;
    
    /** Tracks the paused state */
    bool isPaused = false;
    
private:
    
    /** Timer callback */
    void timerCallback() override;
    
    /** Convert seconds to string */
    String getTimeString(float timeInSeconds);
    
    float totalTime = 5;
    float elapsedTime = 0;
    int totalTrials = 20;
    int currentTrial = 0;
    
    int64 startTime = 0;
    int64 pauseStart = 0;
    int64 pauseTime = 0;

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
    
    /** Enables editing the interface */
    void enable();
    
    /** Disabled editing the interface */
    void disable();
    
    /** Responds to parameter changes */
    void parameterChangeRequest(Parameter* parameter) override;
    
    /** Get pointer to the owned protocol */
    Protocol* getProtocol() { return protocol.get(); }
    
    /** Sets the timeline for the protocol */
    void setTimeline(ProtocolTimeline* timeline);
    
    /** Pointer to protocol timeline */
    ProtocolTimeline* timeline = nullptr;
    
private:
    
    OwnedArray<OptoSequenceInterface> sequenceInterfaces;
    
    std::unique_ptr<TextButton> addSequenceButton;
    
    std::unique_ptr<Protocol> protocol;
    
    Viewport* viewport = nullptr;

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
                            public ComboBox::Listener,
                            public ActionListener
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
    
    /** Receives a trial update notification */
    void actionListenerCallback(const String& message) override;

private:

    /** ComboBox for selecting a protocol */
    std::unique_ptr<ComboBox> protocolSelector;
    
    /** Timeline for the current protocol */
    std::unique_ptr<ProtocolTimeline> protocolTimeline;
    
    /** Button for creating a new protocol */
    std::unique_ptr<TextButton> newProtocolButton;
    
    /** Button for deleting a protocol */
    std::unique_ptr<TextButton> deleteProtocolButton;
    
    /** Button for running a protocol */
    std::unique_ptr<TextButton> runButton;
    
    /** Button for resetting a protocol */
    std::unique_ptr<TextButton> resetButton;

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
    
    /** Current protocol */
    Protocol* currentProtocol;
};


#endif // OPTOPROTOCOLCANVAS_H_INCLUDED
