/*
	------------------------------------------------------------------

	This file is part of the Open Ephys GUI
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

//This prevents include loops. We recommend changing the macro to a name suitable for your plugin
#ifndef PROTOCOL_H_DEFINED
#define PROTOCOL_H_DEFINED

#include <ProcessorHeaders.h>

class Protocol;
class Sequence;
class Condition;
class Stimulus;

/** Available stimulus types*/
enum StimulusType
{
    PULSE_TRAIN,
    SINUSOID,
    RAMP,
    CUSTOM
};

/** 
	Holds parameters for a specific optogenetic stimulus.
    
    Each condition consists of a pulse train or a custom waveform.
*/

class Stimulus
{
public:
	/** The class constructor, used to initialize any members.*/
	Stimulus(ParameterOwner* owner_, StimulusType type_,
             Condition* condition);

	/** The class destructor, used to deallocate memory*/
	virtual ~Stimulus();

    /** Returns the total time of the stimulus */
    virtual float getTotalTime() = 0;
    
    /** Index of the current stimulus */
    static int numStimuliCreated;
    
    /** Stimulus index */
    const int index;
    
    /** The Condition this stimulus belongs to */
    Condition* condition;

    /** Stimulus type*/
    StimulusType type;
    
protected:
    
    /** Generate a unique parameter key */
    std::string generateParameterKey(const String& name);

    /** The parameter owner */
    ParameterOwner* owner;
    
};

/** Custom stimulus */
class CustomStimulus : public Stimulus
{
public:
    /** The class constructor, used to initialize any members.*/
    CustomStimulus(ParameterOwner* owner_,
             Condition* condition);

    /** The class destructor, used to deallocate memory*/
    ~CustomStimulus() { }

    /** The total time of the stimulus */
    float getTotalTime() override;
    
    /** Sample frequency (Hz) */
    FloatParameter sample_frequency;
    
    /** Stimulus waveform*/
    Array<float> stimulus_waveform;

};


/** Pulse train stimulus */
class PulseTrain : public Stimulus
{
public:
    /** The class constructor, used to initialize any members.*/
    PulseTrain(ParameterOwner* owner_,
             Condition* condition);

    /** The class destructor, used to deallocate memory*/
    ~PulseTrain() { }

    /** The total time of the stimulus */
    float getTotalTime() override;
    
    /** Pulse width (ms) */
    FloatParameter pulse_width;
    
    /** Pulse frequency (Hz)*/
    FloatParameter pulse_frequency;

    /** Pulse ramp (ms)*/
    FloatParameter ramp_duration;
    
    /** Pulse count*/
    IntParameter pulse_count;

};


/** Ramp stimulus */
class RampStimulus : public Stimulus
{
public:
    /** The class constructor, used to initialize any members.*/
    RampStimulus(ParameterOwner* owner_,
             Condition* condition);

    /** The class destructor, used to deallocate memory*/
    ~RampStimulus() { }

    /** The total time of the stimulus */
    float getTotalTime() override;
    
    /** Plateau duration (ms) */
    FloatParameter plateau_duration;
    
    /** Onset duration (ms) */
    FloatParameter ramp_onset_duration;

    /** Offset duration (ms)*/
    FloatParameter ramp_offset_duration;
    
    /** Ramp profile */
    CategoricalParameter ramp_profile;

};


/** Sine wave stimulus */
class SineWave : public Stimulus
{
public:
    /** The class constructor, used to initialize any members.*/
    SineWave(ParameterOwner* owner_,
             Condition* condition);

    /** The class destructor, used to deallocate memory*/
    ~SineWave() { }

    /** The total time of the stimulus */
    float getTotalTime() override;
    
    /** Sine wave duration (ms) */
    FloatParameter sine_wave_duration;
    
    /** Sine wave frequency */
    FloatParameter sine_wave_frequency;

};

/** 
	Holds parameters for a specific optogenetic condition.
    
    Each condition consists of stimuli with a certain
    number of repeats.
*/

class Condition
{
public:
	/** The class constructor, used to initialize any members.*/
	Condition(ParameterOwner* owner_,
        Array<String> availableSources,
        Array<int> sitesPerSource,
        Array<int> availableWavelengths,
        Sequence* sequence_);

	/** The class destructor, used to deallocate memory*/
	~Condition();

    /** Adds stimulus to this condition */
    void addStimulus(Stimulus* stimulus);
    
    /** Removes stimulus from this condition */
    void removeStimulus(Stimulus* stimulus);

    /** Total time for this condition */
    float getTotalTime();

    /** Total number of trials for this condition */
    int getTotalTrials();

    /** Number of repeats for this condition */
    IntParameter num_repeats;

    /** Holds the conditions for this sequence */
    OwnedArray<Stimulus> stimuli;

    /** Stimulus colours (in nm) */
    Array<int> availableWavelengths;
    
    /** Available sites for each source */
    Array<int> sitesPerSource;

    /** The name of the stimulation source (e.g. laser) */
    CategoricalParameter source;

    /** Pulse power (microwatts) */
    FloatParameter pulse_power;

    /** Stimulation sites (if the source has multiple emission sites) */
    std::unique_ptr<SelectedChannelsParameter> sites;
    
    /** Index of the current condition */
    static int numConditionsCreated;
    
    /** Condition index */
    const int index;
    
    /** The Sequence this condition belongs to */
    Sequence* sequence;

    /** Add a light wavelength */
    void addWavelength(int wavelength);

    /** Remove a light wavelength */
    void removeWavelength(int wavelength);
    
private:

    /** Generate a unique parameter key */
    std::string generateParameterKey(const String& name);
    
    /** The parameter owner */
    ParameterOwner* owner;
};

/** 
	Holds parameters for a specific optogenetic stimulation
    sequence, composed of a set of conditions.alignas
    
    Each sequence can have a baseline interval, a minimum and maximum
    inter-trial interval, and a randomization flag.
*/

class Sequence
{
public:
	/** The class constructor, used to initialize any members.*/
	Sequence(ParameterOwner* owner_, Protocol* protocol_);

	/** The class destructor, used to deallocate memory*/
	~Sequence();

    /** Adds a condition to the sequence */
    void addCondition(Condition* condition);
    
    /** Removes a condition from the sequence */
    void removeCondition(Condition* condition);

    /** Returns the total time of this sequence */
    float getTotalTime();

    /** Returns the total number of trials */
    int getTotalTrials();

    /** Gets the duration for the next stimulus */
    float getTrialDuration(int trialIndex);

    /** Creates the trials */
    void createTrials();

    /** Baseline interval in seconds (delay before start of stimulation) */
    FloatParameter baseline_interval;

    /** Minimum inter-trial interval in seconds */
    FloatParameter min_iti;

    /** Maximum inter-trial interval in seconds */
    FloatParameter max_iti;

    /** Whether to randomize the trial order */
    BooleanParameter randomize;

    /** Holds the conditions for this sequence */
    OwnedArray<Condition> conditions;
    
    /** Index of the current protocol */
    static int numSequencesCreated;
    
    /** Sequence index */
    const int index;
    
    /** The protocol this sequence belongs to */
    Protocol* protocol;
    
private:
    /** The parameter owner */
    ParameterOwner* owner;

    /** ITI values */
    Array<float> iti_values;

    /** Stimuli */
    Array<Stimulus*> stimuli;

    /** Order */
    Array<int> order;
    
};


/** 
	Holds parameters for a specific optogenetic stimulation
    protocol.

    Each protocol specifies a set of sequences, each of which
    consists of a set of conditions. Each condition can include
    a variable number of repeats of specific stimuli (pulse trains
    or custom waveforms).)
*/

class Protocol : public Timer,
                 public ActionBroadcaster
{
public:
	/** The class constructor, used to initialize any members.*/
	Protocol(const String& name, ParameterOwner* owner_);

	/** The class destructor, used to deallocate memory*/
	~Protocol();

    /** Run protocol */
    void run();

    /** Pause protocol */
    void pause();

    /** Reset protocol */
    void reset();

    /** The name of the protocol */
    String name;

    /** The description of the protocol */
    String description;

    /** Adds a sequence to this protocol */
    void addSequence(Sequence* sequence);
    
    /** Removes a sequence from this protocol */
    void removeSequence(Sequence* sequence);

     /** Updates the trial info for each sequence */
    void createTrials();

    /** Returns the total time of this protocol */
    float getTotalTime();

    /** Returns the total number of trials */
    int getTotalTrials();

    /** Holds the sequences for this protocol */
    OwnedArray<Sequence> sequences;
    
    /** Index of the current protocol */
    static int numProtocolsCreated;
    
    /** Protocol index */
    const int index;
    
private:

    /** Timer callback */
    void timerCallback() override;

    /** The parameter owner */
    ParameterOwner* owner;

    /** Current sequence */
    int currentSequenceIndex = 0;

    /** Current trial */
    int currentTrialIndex = 0;

    /** Baseline interval */
    bool baselineInterval = true;
    
};

#endif // PROTOCOL_H_DEFINED
