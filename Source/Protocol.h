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
             Array<String> availableSources,
             Array<int> sitesPerSource,
             Array<int> availableWavelengths);

	/** The class destructor, used to deallocate memory*/
	~Stimulus();

    /** The name of the stimulation source (e.g. laser) */
    CategoricalParameter source;

    /** Stimulation sites (if the source has multiple emission sites) */
    std::unique_ptr<SelectedChannelsParameter> sites;

    /** Stimulus colours (in nm) */
    Array<int> availableWavelengths;
    
    /** Available sites for each source */
    Array<int> sitesPerSource;
    
protected:
    /** The parameter owner */
    ParameterOwner* owner;
    
    /** Stimulus type*/
    StimulusType type;
    
};

/** Pulse train stimulus */
class PulseTrain : public Stimulus
{
public:
    /** The class constructor, used to initialize any members.*/
    PulseTrain(ParameterOwner* owner_,
             Array<String> availableSources,
             Array<int> sitesPerSource,
             Array<int> availableWavelengths);

    /** The class destructor, used to deallocate memory*/
    ~PulseTrain();
    
    /** Pulse width (ms) */
    FloatParameter pulse_width;
    
    /** Pulse frequency (Hz)*/
    FloatParameter pulse_frequency;
    
    /** Pulse count*/
    IntParameter pulse_count;
    
    /** Pulse power (microwatts) */
    FloatParameter pulse_power;
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
	Condition(ParameterOwner* owner_);

	/** The class destructor, used to deallocate memory*/
	~Condition();

    /** Number of repeats for this condition */
    IntParameter num_repeats;

    /** Holds the conditions for this sequence */
    OwnedArray<Stimulus> stimuli;
    
private:
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
	Sequence(ParameterOwner* owner_);

	/** The class destructor, used to deallocate memory*/
	~Sequence();

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
    
private:
    /** The parameter owner */
    ParameterOwner* owner;
};

/** 
	Holds parameters for a specific optogenetic stimulation
    protocol.

    Each protocol specifies a set of sequences, each of which
    consists of a set of conditions. Each condition can include
    a variable number of repeats of specific stimuli (pulse trains
    or custom waveforms).)
*/

class Protocol
{
public:
	/** The class constructor, used to initialize any members.*/
	Protocol(const String& name, ParameterOwner* owner_);

	/** The class destructor, used to deallocate memory*/
	~Protocol();

    /** The name of the protocol */
    String name;

    /** Holds the sequences for this protocol */
    OwnedArray<Sequence> sequences;
    
private:
    /** The parameter owner */
    ParameterOwner* owner;
};

#endif // PROTOCOL_H_DEFINED
