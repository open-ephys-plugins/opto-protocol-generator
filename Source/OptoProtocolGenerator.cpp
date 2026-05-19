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
    parentElement->setAttribute("nidaqOutputProcessorId", selectedNidaqOutputProcessorId);
}


void OptoProtocolGenerator::loadCustomParametersFromXml(XmlElement* parentElement)
{
    selectedNidaqOutputProcessorId = parentElement->getIntAttribute("nidaqOutputProcessorId", -1);
}

void OptoProtocolGenerator::sendConfigToNidaqOutput(const String& json)
{
    GenericProcessor* nidaqOut = getSelectedNidaqOutputProcessor();

    if (nidaqOut == nullptr)
    {
        const Array<int> ids = getAvailableNidaqOutputProcessorIds();
        if (! ids.isEmpty())
        {
            selectedNidaqOutputProcessorId = ids[0];
            nidaqOut = getSelectedNidaqOutputProcessor();
        }
    }

    if (nidaqOut != nullptr)
        sendConfigMessage(nidaqOut, json);
    else
        LOGE("Could not find NIDAQ Output processor to send trial config.");
}

Array<int> OptoProtocolGenerator::getAvailableNidaqOutputProcessorIds() const
{
    Array<int> ids;

    for (uint16_t nodeId = 1; nodeId <= maxNodeIdToScan; ++nodeId)
    {
        GenericProcessor* processor = CoreServices::getProcessorById(nodeId);
        if (isNidaqOutputProcessor(processor))
            ids.add(nodeId);
    }

    return ids;
}

String OptoProtocolGenerator::getNidaqOutputProcessorLabel(int nodeId) const
{
    GenericProcessor* processor = CoreServices::getProcessorById((uint16_t) nodeId);
    if (processor == nullptr)
        return {};

    return processor->getName() + " (" + String(nodeId) + ")";
}

void OptoProtocolGenerator::setSelectedNidaqOutputProcessorId(int nodeId)
{
    selectedNidaqOutputProcessorId = nodeId;
}

int OptoProtocolGenerator::getSelectedNidaqOutputProcessorId() const
{
    return selectedNidaqOutputProcessorId;
}

GenericProcessor* OptoProtocolGenerator::getSelectedNidaqOutputProcessor() const
{
    if (selectedNidaqOutputProcessorId < 0 || selectedNidaqOutputProcessorId > maxNodeIdToScan)
        return nullptr;

    GenericProcessor* processor = CoreServices::getProcessorById((uint16_t) selectedNidaqOutputProcessorId);
    return isNidaqOutputProcessor(processor) ? processor : nullptr;
}

bool OptoProtocolGenerator::isNidaqOutputProcessor(GenericProcessor* processor)
{
    if (processor == nullptr)
        return false;

    return processor->getName() == "NIDAQ Output";
}
