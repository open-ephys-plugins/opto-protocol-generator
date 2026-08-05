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

#include "OptoProtocolGenerator.h"

#include "OptoProtocolEditor.h"

namespace
{
    constexpr uint16_t maxNodeIdToScan = 4096;
}

OptoProtocolGenerator::OptoProtocolGenerator() 
    : GenericProcessor("Opto Protocol Gen")
{

}


OptoProtocolGenerator::~OptoProtocolGenerator()
{

}


AudioProcessorEditor* OptoProtocolGenerator::createEditor()
{
    editor = std::make_unique<OptoProtocolEditor>(this);
    return editor.get();
}


void OptoProtocolGenerator::saveCustomParametersToXml(XmlElement* parentElement)
{
    parentElement->setAttribute("outputProcessorId", selectedOutputProcessorId);
}


void OptoProtocolGenerator::loadCustomParametersFromXml(XmlElement* parentElement)
{
    selectedOutputProcessorId = parentElement->getIntAttribute(
        "outputProcessorId",
        parentElement->getIntAttribute("nidaqOutputProcessorId", -1));
}

void OptoProtocolGenerator::sendConfigToOutput(const String& json)
{
    GenericProcessor* output = getSelectedOutputProcessor();

    if (output == nullptr)
    {
        const Array<int> ids = getAvailableOutputProcessorIds();
        if (! ids.isEmpty())
        {
            selectedOutputProcessorId = ids[0];
            output = getSelectedOutputProcessor();
        }
    }

    if (output != nullptr)
        sendConfigMessage(output, json);
    else
        LOGE("Could not find NIDAQ Output or Wave Player processor to send trial config.");
}

Array<int> OptoProtocolGenerator::getAvailableOutputProcessorIds() const
{
    Array<int> ids;

    for (uint16_t nodeId = 1; nodeId <= maxNodeIdToScan; ++nodeId)
    {
        GenericProcessor* processor = CoreServices::getProcessorById(nodeId);
        if (isOutputProcessor(processor))
            ids.add(nodeId);
    }

    return ids;
}

String OptoProtocolGenerator::getOutputProcessorLabel(int nodeId) const
{
    GenericProcessor* processor = CoreServices::getProcessorById((uint16_t) nodeId);
    if (processor == nullptr)
        return {};

    return processor->getName() + " (" + String(nodeId) + ")";
}

void OptoProtocolGenerator::setSelectedOutputProcessorId(int nodeId)
{
    selectedOutputProcessorId = nodeId;
}

int OptoProtocolGenerator::getSelectedOutputProcessorId() const
{
    return selectedOutputProcessorId;
}

GenericProcessor* OptoProtocolGenerator::getSelectedOutputProcessor() const
{
    if (selectedOutputProcessorId < 0 || selectedOutputProcessorId > maxNodeIdToScan)
        return nullptr;

    GenericProcessor* processor = CoreServices::getProcessorById((uint16_t) selectedOutputProcessorId);
    return isOutputProcessor(processor) ? processor : nullptr;
}

bool OptoProtocolGenerator::isOutputProcessor(GenericProcessor* processor)
{
    if (processor == nullptr)
        return false;

    const String name = processor->getName();
    return name == "NIDAQ Output" || name == "Wave Player";
}
