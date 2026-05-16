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
#include "OptoHardwareConfig.h"

#include <utility>

int Protocol::numProtocolsCreated = 0;
int Sequence::numSequencesCreated = 0;
int Condition::numConditionsCreated = 0;
int Stimulus::numStimuliCreated = 0;


CustomStimulus::CustomStimulus(ParameterOwner* owner_,
                       Condition* condition_)
    : Stimulus(owner_, StimulusType::CUSTOM, condition_),
      sample_frequency(owner_, Parameter::VISUALIZER_SCOPE,
                 "sample_frequency",
                 "Sample frequency",
                 "The waveform playback frequency",
                 "Hz",
                 10000.0f,
                 100.0f,
                 500000.f)
{
    sample_frequency.setKey(generateParameterKey("sample_frequency"));
    Parameter::registerParameter(&sample_frequency);
}

float CustomStimulus::getTotalTime()
{
    // Calculate the total time of the pulse train
    
    return stimulus_waveform.size() / sample_frequency.getFloatValue();

}

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
                  1, 1, 100),
      sitesPerSource(sitesPerSource_),
      availableWavelengths(availableWavelengths_),
      source(owner_,
              Parameter::VISUALIZER_SCOPE,
              "source",
              "Source",
              "The source of the optogenetic stimulation",
              availableSources_,
              0)
{
    if (availableSources_.isEmpty())
        availableSources_.add("Source 1");
    else
        for (int i = 0; i < availableSources_.size(); ++i)
            if (availableSources_[i].trim().isEmpty())
                availableSources_.set(i, "Source " + String(i + 1));
    if (sitesPerSource_.isEmpty())
        sitesPerSource_.add(1);
    else
        for (int i = 0; i < sitesPerSource_.size(); ++i)
            sitesPerSource_.set(i, jmax(1, sitesPerSource_[i]));

    if (availableWavelengths_.isEmpty())
        availableWavelengths_.add(638);

    source.setCategories(availableSources_);
    sitesPerSource = sitesPerSource_;
    availableWavelengths = availableWavelengths_;

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
    createPulsePowerParameter(0, 10.f);
    activePulsePowerCount = 1;

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

void Condition::removeStimulus(Stimulus* stimulus)
{
    int stimulusIndex = stimuli.indexOf(stimulus);
    if (stimulusIndex != -1)
    {
        stimuli.removeObject(stimulus, true);
        sequence->createTrials();
    }
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

FloatParameter* Condition::createPulsePowerParameter(int powerIndex, float defaultValue)
{
    auto* parameter = new FloatParameter(owner,
                                         Parameter::VISUALIZER_SCOPE,
                                         "pulse_power_" + String(powerIndex + 1),
                                         powerIndex == 0 ? "Light power" : "Light power " + String(powerIndex + 1),
                                         "Peak output power (scales using hardware JSON power/voltage table)",
                                         "",
                                         defaultValue,
                                         0.f,
                                         1.0e6f);
    parameter->setKey(generateParameterKey("pulse_power_" + String(powerIndex + 1)));
    Parameter::registerParameter(parameter);
    pulse_powers.add(parameter);
    return parameter;
}

float Condition::getPulsePower(int index) const
{
    if (pulse_powers.isEmpty())
        return 10.f;
    const int clampedIndex = jlimit(0, jmax(0, getNumPulsePowers() - 1), index);
    if (auto* parameter = pulse_powers[clampedIndex])
        return parameter->getFloatValue();
    return 10.f;
}
static String getMinimalFloatString(float value)
{
    String text(value, 6);
    while (text.containsChar('.') && text.endsWithChar('0'))
        text = text.dropLastCharacters(1);
    if (text.endsWithChar('.'))
        text = text.dropLastCharacters(1);
    return text == "-0" ? "0" : text;
}

String Condition::getPulsePowersString() const
{
    String text;
    for (int i = 0; i < getNumPulsePowers(); ++i)
    {
        if (i > 0)
            text << ", ";
        text << getMinimalFloatString(getPulsePower(i));
    }
    return text;
}

void Condition::setPulsePowers(const Array<float>& powers)
{
    Array<float> sanitized;
    for (auto power : powers)
        sanitized.add(jlimit(0.0f, 1.0e6f, power));
    if (sanitized.isEmpty())
        sanitized.add(getPulsePower(0));

    for (int i = 0; i < sanitized.size(); ++i)
    {
        if (i >= pulse_powers.size())
            createPulsePowerParameter(i, sanitized[i]);
        pulse_powers[i]->setNextValue(sanitized[i], false);
    }
    activePulsePowerCount = sanitized.size();
}

void Condition::applyHardwareCatalog(const OptoHardwareConfig* cfg)
{
    if (cfg == nullptr || cfg->isEmpty())
        return;

    Array<String> names;
    Array<int> sites;
    for (const auto& d : cfg->devices)
    {
        names.add(d.name);
        sites.add(d.is_np_opto ? kNpOptoSitesPerSource : 1);
    }

    sitesPerSource = sites;
    source.setCategories(names);
    refreshForSelectedSource(cfg);
}

void Condition::refreshForSelectedSource(const OptoHardwareConfig* cfg)
{
    if (cfg == nullptr || cfg->isEmpty())
        return;

    const int di = jlimit(0, (int) cfg->devices.size() - 1, source.getSelectedIndex());
    const auto& dev = cfg->devices[di];
    sites->setChannelCount(jmax(1, sitesPerSource[di]));

    for (int i = availableWavelengths.size() - 1; i >= 0; --i)
    {
        const int wl = availableWavelengths[i];
        bool ok = false;
        for (const auto& ls : dev.lightSources)
        {
            if (ls.wavelength == wl)
            {
                ok = true;
                break;
            }
        }
        if (!ok)
            removeWavelength(wl);
    }

    if (availableWavelengths.isEmpty() && dev.lightSources.size() > 0)
        addWavelength(dev.lightSources[0].wavelength);
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
    int numWavelengths = availableWavelengths.size();
    int numPowers = getNumPulsePowers();
    int nStim = stimuli.size();
    if (nStim == 0) return 0;
    return numRepeats * numPowers * numSites * numWavelengths * nStim;
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

void Sequence::removeCondition(Condition* condition)
{
    LOGD("Removing condition.");
    conditions.removeObject(condition, true);
    createTrials();
}

void Sequence::createTrials()
{
    iti_values.clear();
    order.clear();
    stimuli.clear();
    trial_block_order.clear();

    std::vector<Sequence::TrialBlock> blocks;

    for (int c = 0; c < conditions.size(); ++c)
    {
        Condition* condition = conditions[c];
        int numRepeats = condition->num_repeats.getIntValue();
        int numSites = condition->sites->getArrayValue().size();
        int numWavelengths = condition->availableWavelengths.size();
        int numPowers = condition->getNumPulsePowers();

        LOGD("Condition ", condition->index, " has ", numRepeats, " repeats and ", numPowers, " powers and ", numSites, " sites and ", condition->stimuli.size(), " stimuli");

        for (int i = 0; i < numRepeats; ++i)
            for (int p = 0; p < numPowers; ++p)
                for (int w = 0; w < numWavelengths; ++w)
                    for (int j = 0; j < numSites; ++j)
                        blocks.push_back(std::make_tuple(c, i, p, w, j));
    }

    if (randomize.getBoolValue() && blocks.size() > 1)
    {
        LOGD("Randomizing trial block order...");
        auto& rng = Random::getSystemRandom();
        for (int i = (int)blocks.size() - 1; i > 0; --i)
            std::swap(blocks[i], blocks[rng.nextInt(i + 1)]);
    }

    trial_block_order = blocks;

    int trialIndex = 0;
    for (const auto& b : blocks)
    {
        int c = std::get<0>(b);
        Condition* condition = conditions[c];
        float minITI = min_iti.getFloatValue();
        float maxITI = max_iti.getFloatValue();

        for (auto* stimulus : condition->stimuli)
        {
            stimuli.add(stimulus);
            order.add(trialIndex++);
            float iti = Random::getSystemRandom().nextFloat() * (maxITI - minITI) + minITI;
            iti_values.add(iti);
        }
    }

    LOGD("Created ", trialIndex, " total trials");
}

Stimulus* Sequence::getStimulusForTrial(int trialIndex) const
{
    if (order.isEmpty() || trialIndex < 0 || trialIndex >= order.size())
        return nullptr;
    int idx = order[trialIndex];
    if (idx < 0 || idx >= stimuli.size())
        return nullptr;
    return stimuli[idx];
}

float Sequence::getTrialDuration(int trialIndex)
{
    if (order.isEmpty() || trialIndex < 0 || trialIndex >= order.size())
        return 0.0f;
    int idx = order[trialIndex];
    if (idx < 0 || idx >= stimuli.size() || trialIndex >= iti_values.size())
        return 0.0f;
    return stimuli[idx]->getTotalTime() + iti_values[trialIndex];
}

float Sequence::getPulsePowerForTrial(int trialIndex) const
{
    if (trialIndex < 0)
        return 10.f;

    int trialCount = 0;
    for (int blockIndex = 0; blockIndex < (int) trial_block_order.size(); ++blockIndex)
    {
        const int conditionIndex = std::get<0>(trial_block_order[blockIndex]);
        if (conditionIndex < 0 || conditionIndex >= conditions.size())
            continue;
        const int numStimuli = conditions[conditionIndex]->stimuli.size();
        if (trialIndex < trialCount + numStimuli)
            return getTrialBlockPulsePower(blockIndex);
        trialCount += numStimuli;
    }
    return 10.f;
}

float Sequence::getTrialBlockPulsePower(int blockIndex) const
{
    if (blockIndex < 0 || blockIndex >= (int) trial_block_order.size())
        return 10.f;
    const int conditionIndex = std::get<0>(trial_block_order[blockIndex]);
    const int powerIndex = std::get<2>(trial_block_order[blockIndex]);
    if (conditionIndex < 0 || conditionIndex >= conditions.size())
        return 10.f;
    return conditions[conditionIndex]->getPulsePower(powerIndex);
}

int Sequence::getFirstTrialIndexForBlock(int blockIndex) const
{
    if (blockIndex < 0 || blockIndex >= (int) trial_block_order.size())
        return -1;

    int firstTrialIndex = 0;
    for (int i = 0; i < blockIndex; ++i)
    {
        const int conditionIndex = std::get<0>(trial_block_order[i]);
        if (conditionIndex >= 0 && conditionIndex < conditions.size())
            firstTrialIndex += conditions[conditionIndex]->stimuli.size();
    }
    return firstTrialIndex;
}

float Sequence::getTrialBlockDuration(int blockIndex) const
{
    const int firstTrialIndex = getFirstTrialIndexForBlock(blockIndex);
    if (firstTrialIndex < 0)
        return 0.0f;

    const int conditionIndex = std::get<0>(trial_block_order[blockIndex]);
    if (conditionIndex < 0 || conditionIndex >= conditions.size())
        return 0.0f;

    float duration = 0.0f;
    const int numStimuli = conditions[conditionIndex]->stimuli.size();
    for (int i = 0; i < numStimuli; ++i)
    {
        const int trialIndex = firstTrialIndex + i;
        if (trialIndex < 0 || trialIndex >= order.size())
            continue;
        const int stimulusIndex = order[trialIndex];
        if (stimulusIndex >= 0 && stimulusIndex < stimuli.size())
            duration += stimuli[stimulusIndex]->getTotalTime();
        if (trialIndex >= 0 && trialIndex < iti_values.size())
            duration += iti_values[trialIndex];
    }
    return duration;
}

float Sequence::getTrialBlockIti(int blockIndex) const
{
    const int firstTrialIndex = getFirstTrialIndexForBlock(blockIndex);
    if (firstTrialIndex < 0)
        return 0.0f;

    const int conditionIndex = std::get<0>(trial_block_order[blockIndex]);
    if (conditionIndex < 0 || conditionIndex >= conditions.size())
        return 0.0f;

    float iti = 0.0f;
    const int numStimuli = conditions[conditionIndex]->stimuli.size();
    for (int i = 0; i < numStimuli; ++i)
    {
        const int trialIndex = firstTrialIndex + i;
        if (trialIndex >= 0 && trialIndex < iti_values.size())
            iti += iti_values[trialIndex];
    }
    return iti;
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
    if (sequences.isEmpty())
        return;
    if (currentSequenceIndex >= sequences.size())
        currentSequenceIndex = 0;
    if (baselineInterval)
    {
        float baselineInterval = sequences[currentSequenceIndex]->baseline_interval.getFloatValue();

        LOGD("Starting baseline interval for sequence ", currentSequenceIndex, " with duration ", baselineInterval);
        sendActionMessage(String(0));
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

void Protocol::removeSequence(Sequence* sequence)
{
    sequences.removeObject(sequence, true);
}

void Protocol::timerCallback()
{
    stopTimer();
    if (sequences.isEmpty())
    {
        sendActionMessage("FINISHED");
        return;
    }
    if (currentSequenceIndex >= sequences.size())
    {
        sendActionMessage("FINISHED");
        return;
    }

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
            sendActionMessage("FINISHED");
            return;
        }
        baselineInterval = true;
        run();
        return;
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
    float totalTime = 0.0f;

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
