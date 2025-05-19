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
                       Condition* condition_)
    : Stimulus(owner_, StimulusType::PULSE_TRAIN, condition_),
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
    ramp_duration(owner_, Parameter::VISUALIZER_SCOPE,
                  "ramp_duration",
                  "Ramp",
                  "The duration of the ramp in ms",
                  "ms",
                  0.0f,
                  0.0f,
                  100.f)
{
    pulse_count.setKey(generateParameterKey("pulse_count"));
    Parameter::registerParameter(&pulse_count);
    pulse_width.setKey(generateParameterKey("pulse_width"));
    Parameter::registerParameter(&pulse_width);
    pulse_frequency.setKey(generateParameterKey("pulse_frequency"));
    Parameter::registerParameter(&pulse_frequency);
    ramp_duration.setKey(generateParameterKey("ramp_duration"));
    Parameter::registerParameter(&ramp_duration);

}

float PulseTrain::getTotalTime()
{
    // Calculate the total time of the pulse train
    
    int numPulses = pulse_count.getIntValue();
    float pulseWidth = pulse_width.getFloatValue() / 1000.0f;
    float pulseFrequency = pulse_frequency.getFloatValue();
    
    return (numPulses * pulseWidth) +
           (numPulses - 1) * (1.0f / pulseFrequency);

}


RampStimulus::RampStimulus(ParameterOwner* owner_,
                       Condition* condition_)
    : Stimulus(owner_, StimulusType::RAMP, condition_),
      plateau_duration(owner_, Parameter::VISUALIZER_SCOPE,
                 "plateau_duration",
                 "Plateau",
                 "The ramp plateau width in ms",
                 "ms",
                 100.0f,
                 0.1f,
                 1000.f),
    ramp_onset_duration(owner_, Parameter::VISUALIZER_SCOPE,
               "ramp_onset_duration",
               "Onset",
               "The ramp onset duration in ms",
               "ms",
               10.0f,
               0.1f,
               100.f),
    ramp_offset_duration(owner_, Parameter::VISUALIZER_SCOPE,
               "ramp_offset_duration",
               "Offset",
               "The ramp offset duration in ms",
               "ms",
               10.0f,
               0.1f,
               100.f),
    ramp_profile(owner_, Parameter::VISUALIZER_SCOPE,
                  "ramp_profile",
                  "Ramp",
                  "The ramp profile",
                  {"Linear", "Cosine"},
                  0)
{
    plateau_duration.setKey(generateParameterKey("plateau_duration"));
    Parameter::registerParameter(&plateau_duration);
    ramp_onset_duration.setKey(generateParameterKey("ramp_onset_duration"));
    Parameter::registerParameter(&ramp_onset_duration);
    ramp_offset_duration.setKey(generateParameterKey("ramp_offset_duration"));
    Parameter::registerParameter(&ramp_offset_duration);
    ramp_profile.setKey(generateParameterKey("ramp_profile"));
    Parameter::registerParameter(&ramp_profile);

}

float RampStimulus::getTotalTime()
{
    return (plateau_duration.getFloatValue() + ramp_onset_duration.getFloatValue() + ramp_offset_duration.getFloatValue()) / 1000.0f;
}


SineWave::SineWave(ParameterOwner* owner_,
                       Condition* condition_)
    : Stimulus(owner_, StimulusType::SINUSOID, condition_),
      sine_wave_duration(owner_, Parameter::VISUALIZER_SCOPE,
                 "sine_wave_duration",
                 "Duration",
                 "The sine wave duration in ms",
                 "ms",
                 100.0f,
                 0.1f,
                 10000.f),
    sine_wave_frequency(owner_, Parameter::VISUALIZER_SCOPE,
               "sine_wave_frequency",
               "Frequency",
               "The sine wave frequency in Hz",
               "Hz",
               10.0f,
               0.1f,
               1000.f)
{
    sine_wave_duration.setKey(generateParameterKey("sine_wave_duration"));
    Parameter::registerParameter(&sine_wave_duration);
    sine_wave_frequency.setKey(generateParameterKey("sine_wave_frequency"));
    Parameter::registerParameter(&sine_wave_frequency);
}

float SineWave::getTotalTime()
{
    return sine_wave_duration.getFloatValue() / 1000.0f;
}

Stimulus::Stimulus(ParameterOwner* owner_,
                   StimulusType type_,
                   Condition* condition_)
    : owner(owner_),
      type(type_),
      condition(condition_),
      index(++numStimuliCreated)
{


}

Stimulus::~Stimulus()
{
    // No dynamic memory to deallocate
    --numStimuliCreated;
}

std::string Stimulus::generateParameterKey(const String &name)
{
    return (String(condition->sequence->protocol->index) +
            ":" + String(condition->sequence->index) +
            ":" + String(condition->index) +
            ":" + String(index) +
            ":" + name).toStdString();
}

Condition::Condition(ParameterOwner* owner_,
    Array<String> availableSources_,
    Array<int> sitesPerSource_,
    Array<int> availableWavelengths_,
     Sequence* sequence_)
    : owner(owner_),
      sequence(sequence_),
      index(++numConditionsCreated),
      num_repeats(owner_,
                  Parameter::VISUALIZER_SCOPE,
                  "num_repeats",
                  "Num repeats",
                  "Number of times each stimulus is repeated during a sequence",
                  1, 1, 1000),
      sitesPerSource(sitesPerSource_),
      availableWavelengths(availableWavelengths_),
      source(owner_,
              Parameter::VISUALIZER_SCOPE,
              "source",
              "Source",
              "The source of the optogenetic stimulation",
              availableSources_,
              0),
    pulse_power(owner_, Parameter::VISUALIZER_SCOPE,
               "pulse_power",
               "Light power",
               "Peak output power (in microwatts)",
               "uW",
               10,
               0,
               10000)
{
    // Initialize with no stimuli
    num_repeats.setKey((String(sequence->protocol->index) + ":" + String(sequence->index) + ":" + String(index) + ":num_repeats").toStdString());
    Parameter::registerParameter(&num_repeats);

    // Initialize the selected channels parameter
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
    pulse_power.setKey(generateParameterKey("pulse_power"));
    Parameter::registerParameter(&pulse_power);

    LOGD("Sites per source: ", sitesPerSource[0]);
}

Condition::~Condition()
{
    // OwnedArray will automatically delete all stimuli
    --numConditionsCreated;
}

void Condition::addStimulus(Stimulus* stimulus)
{
    stimuli.add(stimulus);
    sequence->createTrials();
}

void Condition::addWavelength(int wavelength)
{
    if (availableWavelengths.contains(wavelength))
        return;

    availableWavelengths.add(wavelength);
}

void Condition::removeWavelength(int wavelength)
{
    int wavelengthIndex = availableWavelengths.indexOf(wavelength);
    if (wavelengthIndex != -1)
        availableWavelengths.remove(wavelengthIndex);
}


std::string Condition::generateParameterKey(const String &name)
{
    return (String(sequence->protocol->index) +
            ":" + String(sequence->index) +
            ":" + String(index) +
            ":" + name).toStdString();
}


float Condition::getTotalTime() 
{
    float totalTime = 0;
    for (auto* stimulus : stimuli)
        totalTime += stimulus->getTotalTime();

    return totalTime * getTotalTrials();
}

int Condition::getTotalTrials() 
{

    int numRepeats = num_repeats.getIntValue();
    int numSites = sites->getArrayValue().size();
    int numWavelegths = availableWavelengths.size();

    return numRepeats * numSites * numWavelegths;
}

Sequence::Sequence(ParameterOwner* owner_, Protocol* protocol_)
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

    createTrials();
    LOGD("Sequence created with index: ", index);
    LOGD("Baseline interval: ", baseline_interval.getFloatValue());
    LOGD("Min ITI: ", min_iti.getFloatValue());
    LOGD("Max ITI: ", max_iti.getFloatValue());
    LOGD("Randomize: ", randomize.getBoolValue());

}

Sequence::~Sequence()
{
    // OwnedArray will automatically delete all conditions
    --numSequencesCreated;
}

void Sequence::addCondition(Condition* condition)
{
    LOGD("Adding condition.");
    conditions.add(condition);
    createTrials();
}

void Sequence::createTrials()
{
    iti_values.clear();
    order.clear();
    stimuli.clear();

    int trialIndex = 0;

    // Create the trials for each condition
    for (auto* condition : conditions)
    {
        int numRepeats = condition->num_repeats.getIntValue();
        int numSites = condition->sites->getArrayValue().size();
        int numWavelegths = condition->availableWavelengths.size();

        LOGD("Condition ", condition->index, " has ", numRepeats, " repeats and ", numSites, " sites and ", condition->stimuli.size(), " stimuli");

        for (int i = 0; i < numRepeats; ++i)
        {
            for (int j = 0; j < numSites; ++j)
            {
                for (int k = 0; k < numWavelegths; ++k)
                {

                    for (auto* stimulus : condition->stimuli)
                    {
                        // Add the stimulus to the list
                        stimuli.add(stimulus);

                        // Add the order
                        order.add(trialIndex++);

                        // Add the ITI value
                        float minITI = min_iti.getFloatValue();
                        float maxITI = max_iti.getFloatValue();
                        float iti = Random::getSystemRandom().nextFloat() * (maxITI - minITI) + minITI;
                        iti_values.add(iti);
                    }
                }
            }
        }
    }
    
    LOGD("Created ", trialIndex, " total trials");

    if (randomize.getBoolValue())
    {
        LOGD("Randomizing trial order...");
        
        for (int i = order.size() - 1; i > 0; --i)
        {
            int j = Random::getSystemRandom().nextInt(i + 1);
            order.swap(i, j);
        }
    }
}

float Sequence::getTrialDuration(int trialIndex)
{
    int nextTrial = order[trialIndex];
    return stimuli[nextTrial]->getTotalTime() + iti_values[nextTrial];
}

float Sequence::getTotalTime() 
{
    float totalTime = baseline_interval.getFloatValue();

    for (int i = 0; i < order.size(); ++i)
    {
        int stimulusIndex = order[i];

        totalTime += stimuli[stimulusIndex]->getTotalTime();
        totalTime += iti_values[i];
    }
        
    return totalTime;
}

int Sequence::getTotalTrials() 
{
    int totalTrials = 0;
    for (auto* condition : conditions)
        totalTrials += condition->getTotalTrials();

    return totalTrials;
}

Protocol::Protocol(const String& name_, ParameterOwner* owner_)
    : name(name_), owner(owner_), index(++numProtocolsCreated)
{
}

Protocol::~Protocol()
{
    // OwnedArray will automatically delete all sequences
    --numProtocolsCreated;
}

void Protocol::run()
{
    if (baselineInterval)
    {
        float baselineInterval = sequences[currentSequenceIndex]->baseline_interval.getFloatValue();

        LOGD("Starting baseline interval for sequence ", currentSequenceIndex, " with duration ", baselineInterval);
        startTimer(baselineInterval * 1000.0f);
    } else {
        startTimer(0);
    }
}

void Protocol::pause()
{
    stopTimer();
}

void Protocol::reset()
{
    // Stop the protocol
    stopTimer();
    currentTrialIndex = 0;
    currentSequenceIndex = 0;
    baselineInterval = true;
}

void Protocol::addSequence(Sequence* sequence)
{
    sequences.add(sequence);
}

void Protocol::timerCallback()
{
    stopTimer();

    if (baselineInterval)
    {
        LOGD("Ending baseline interval for sequence ", currentSequenceIndex);
        baselineInterval = false;
    }

    if (currentTrialIndex >= sequences[currentSequenceIndex]->getTotalTrials())
    {
        LOGD("Ending sequence ", currentSequenceIndex);
        currentTrialIndex = 0;
        currentSequenceIndex++;

        if (currentSequenceIndex >= sequences.size())
        {
            // All sequences are done
            sendActionMessage("FINISHED");
            return;
        } else {
            baselineInterval = true;
            run();
        }
    }   

    LOGD("Starting sequence ", currentSequenceIndex, " trial ", currentTrialIndex);
    float nextTrialDuration = sequences[currentSequenceIndex]->getTrialDuration(currentTrialIndex);

    currentTrialIndex++;
    sendActionMessage(String(currentTrialIndex));

    startTimer(nextTrialDuration * 1000.0f);

}

void Protocol::createTrials()
{
    for (auto* sequence : sequences)
    {
        sequence->createTrials();
    }
}

float Protocol::getTotalTime()
{
    int totalTime = 0;

    for (auto* sequence : sequences)
    {
        totalTime += sequence->getTotalTime();
    }
    
    return totalTime;
}

int Protocol::getTotalTrials() 
{

    int totalTrials = 0;

    for (auto* sequence : sequences)
    {
        totalTrials += sequence->getTotalTrials();
    }

    return totalTrials;
}
