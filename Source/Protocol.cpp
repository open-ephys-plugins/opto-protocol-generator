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

#include "Protocol.h"

int Protocol::numProtocolsCreated = 0;
int Sequence::numSequencesCreated = 0;
int Condition::numConditionsCreated = 0;
int Stimulus::numStimuliCreated = 0;

PulseTrain::PulseTrain(ParameterOwner* owner_,
                       Array<String> availableSources,
                       Array<int> sitesPerSource,
                       Array<int> availableWavelengths,
                       const Condition* condition_)
    : Stimulus(owner_, StimulusType::PULSE_TRAIN,
               availableSources,
               sitesPerSource,
               availableWavelengths, condition_),
      pulse_width(owner_, Parameter::VISUALIZER_SCOPE,
                 "pulse_width",
                 "Pulse width",
                 "The width of the pulse in ms",
                 "ms",
                 10.0f,
                 0.0f,
                 100.f),
    pulse_frequency(owner_, Parameter::VISUALIZER_SCOPE,
               "pulse_frequency",
               "Pulse freq",
               "The frequency of the pulse train in Hz",
               "Hz",
               10.0f,
               0.1f,
               100.f),
    pulse_count(owner_, Parameter::VISUALIZER_SCOPE,
               "pulse_count",
               "Pulse count",
               "The total number of pulses",
               1,
               0,
               100),
    pulse_power(owner_, Parameter::VISUALIZER_SCOPE,
               "pulse_power",
               "Light power",
               "Peak output power (in microwatts)",
               "uW",
               10,
               0,
               10000)
{
    pulse_count.setKey(generateParameterKey("pulse_count"));
    Parameter::registerParameter(&pulse_count);
    pulse_width.setKey(generateParameterKey("pulse_width"));
    Parameter::registerParameter(&pulse_width);
    pulse_frequency.setKey(generateParameterKey("pulse_frequency"));
    Parameter::registerParameter(&pulse_frequency);
    pulse_power.setKey(generateParameterKey("pulse_power"));
    Parameter::registerParameter(&pulse_power);
}

Stimulus::Stimulus(ParameterOwner* owner_,
                   StimulusType type_,
                   Array<String> availableSources,
                   Array<int> sitesPerSource_,
                   Array<int> availableWavelengths_,
                   const Condition* condition_)
    : owner(owner_),
      type(type_),
      condition(condition_),
      index(++numStimuliCreated),
      sitesPerSource(sitesPerSource_),
      availableWavelengths(availableWavelengths_),
      source(owner_,
              Parameter::VISUALIZER_SCOPE,
              "source",
              "Source",
              "The source of the optogenetic stimulation",
              availableSources,
              0)
      
{

    Array<var> defaultSelection;
    for (int i = 0; i < sitesPerSource[0]; i++)
        defaultSelection.add(i);

    sites = std::make_unique<SelectedChannelsParameter>(owner,
        Parameter::VISUALIZER_SCOPE,
        "sites",
        "Sites",
        "The emission sites used for optogenetic stimulation",
        defaultSelection);
    sites->setChannelCount(sitesPerSource[0]);

    sites->setKey(generateParameterKey("sites"));
    Parameter::registerParameter(sites.get());
    source.setKey(generateParameterKey("source"));
    Parameter::registerParameter(&source);

    LOGD("Sites per source: ", sitesPerSource_[0]);
}

Stimulus::~Stimulus()
{
    // No dynamic memory to deallocate
}

std::string Stimulus::generateParameterKey(const String &name)
{
    return (String(condition->sequence->protocol->index) +
            ":" + String(condition->sequence->index) +
            ":" + String(condition->index) +
            ":" + String(index) +
            ":" + name).toStdString();
}

Condition::Condition(ParameterOwner* owner_, const Sequence* sequence_)
    : owner(owner_),
      sequence(sequence_),
      index(++numConditionsCreated),
      num_repeats(owner_,
                  Parameter::VISUALIZER_SCOPE,
                  "num_repeats",
                  "Num repeats",
                  "Number of times each stimulus is repeated during a sequence",
                  1, 1, 1000)
{
    // Initialize with no stimuli
    num_repeats.setKey((String(sequence->protocol->index) + ":" + String(sequence->index) + ":" + String(index) + ":num_repeats").toStdString());
    Parameter::registerParameter(&num_repeats);
}

Condition::~Condition()
{
    // OwnedArray will automatically delete all stimuli
}

Sequence::Sequence(ParameterOwner* owner_, const Protocol* protocol_)
    : owner(owner_),
      index(++numSequencesCreated),
      protocol(protocol_),
      baseline_interval(owner_,
                        Parameter::VISUALIZER_SCOPE,
                        "baseline_interval",
                        "Baseline",
                        "Length of delay period before initiating a sequence",
                        "s",
                        0.0f,
                        0.0f,
                        3600.0f),
    min_iti(owner_,
            Parameter::VISUALIZER_SCOPE,
            "min_iti",
            "Min ITI",
            "Mininum time between trials",
            "s",
            1.0f,
            0.0f,
            60.0f),
    max_iti(owner_,
            Parameter::VISUALIZER_SCOPE,
            "max_iti",
            "Max ITI",
            "Maximum time between trials",
            "s",
            1.0f,
            0.0f,
            60.0f),
    randomize(owner_,
            Parameter::VISUALIZER_SCOPE,
            "randomize",
            "Randomize",
            "Randomize trial order",
            true)

{
    min_iti.setKey((String(protocol->index) + ":" + String(index) + ":min_iti").toStdString());
    Parameter::registerParameter(&min_iti);
    max_iti.setKey((String(protocol->index) + ":" + String(index) + ":max_iti").toStdString());
    Parameter::registerParameter(&max_iti);
    randomize.setKey((String(protocol->index) + ":" + String(index) + ":randomize").toStdString());
    Parameter::registerParameter(&randomize);
    baseline_interval.setKey((String(protocol->index) + ":" + String(index) + ":baseline_interval").toStdString());
    Parameter::registerParameter(&baseline_interval);

}

Sequence::~Sequence()
{
    // OwnedArray will automatically delete all conditions
}


Protocol::Protocol(const String& name_, ParameterOwner* owner_)
    : name(name_), owner(owner_), index(++numProtocolsCreated)
{
}

Protocol::~Protocol()
{
    // OwnedArray will automatically delete all sequences
}
