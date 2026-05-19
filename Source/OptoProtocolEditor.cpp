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


#include "OptoProtocolEditor.h"

#include "OptoProtocolCanvas.h"
#include "OptoProtocolGenerator.h"


OptoProtocolEditor::OptoProtocolEditor(GenericProcessor* p)
    : VisualizerEditor(p, "Opto Protocol", 240)
{
    outputProcessorLabel = std::make_unique<Label>("outputProcessorLabel", "Output Node");
    outputProcessorLabel->setBounds(20, 40, 200, 20);
    addAndMakeVisible(outputProcessorLabel.get());

    outputProcessorSelector = std::make_unique<ComboBox>("outputProcessorSelector");
    outputProcessorSelector->setBounds(20, 60, 200, 20);
    outputProcessorSelector->addListener(this);
    addAndMakeVisible(outputProcessorSelector.get());

    refreshOutputProcessorSelector();
    startTimer(500);
}

OptoProtocolEditor::~OptoProtocolEditor()
{
    stopTimer();
}

Visualizer* OptoProtocolEditor::createNewCanvas()
{
    return new OptoProtocolCanvas((OptoProtocolGenerator*) getProcessor());
}

void OptoProtocolEditor::comboBoxChanged(ComboBox* comboBox)
{
    if (comboBox != outputProcessorSelector.get())
        return;

    const int nodeId = comboBox->getSelectedId() - 1;
    if (nodeId >= 0)
        ((OptoProtocolGenerator*) getProcessor())->setSelectedNidaqOutputProcessorId(nodeId);
}

void OptoProtocolEditor::timerCallback()
{
    refreshOutputProcessorSelector();
}

void OptoProtocolEditor::refreshOutputProcessorSelector()
{
    auto* optoProcessor = (OptoProtocolGenerator*) getProcessor();
    const Array<int> ids = optoProcessor->getAvailableNidaqOutputProcessorIds();
    int selectedNodeId = optoProcessor->getSelectedNidaqOutputProcessorId();

    if (ids.isEmpty())
    {
        selectedNodeId = -1;
    }
    else if (! ids.contains(selectedNodeId))
    {
        selectedNodeId = ids[0];
    }

    if (selectedNodeId != optoProcessor->getSelectedNidaqOutputProcessorId())
        optoProcessor->setSelectedNidaqOutputProcessorId(selectedNodeId);

    String signature;

    for (int nodeId : ids)
        signature += String(nodeId) + ":" + optoProcessor->getNidaqOutputProcessorLabel(nodeId) + "|";
    signature += "selected=" + String(selectedNodeId);

    if (signature == outputProcessorSelectorSignature)
    {
        updateOutputProcessorSelectorEnabled(! ids.isEmpty());
        return;
    }

    outputProcessorSelectorSignature = signature;
    outputProcessorSelector->clear(dontSendNotification);

    if (ids.isEmpty())
    {
        outputProcessorSelector->addItem("No NIDAQ Output found", 1);
        outputProcessorSelector->setSelectedId(1, dontSendNotification);
        updateOutputProcessorSelectorEnabled(false);
        return;
    }

    for (int nodeId : ids)
        outputProcessorSelector->addItem(optoProcessor->getNidaqOutputProcessorLabel(nodeId), nodeId + 1);

    outputProcessorSelector->setSelectedId(selectedNodeId + 1, dontSendNotification);
    updateOutputProcessorSelectorEnabled(true);
}

void OptoProtocolEditor::startAcquisition()
{
    outputProcessorSelector->setEnabled(false);
}

void OptoProtocolEditor::stopAcquisition()
{
    updateOutputProcessorSelectorEnabled(outputProcessorSelectorHasOutputs);
}

void OptoProtocolEditor::updateOutputProcessorSelectorEnabled(bool hasOutputs)
{
    outputProcessorSelectorHasOutputs = hasOutputs;
    const bool acquisitionActive = acquisitionIsActive || CoreServices::getAcquisitionStatus();
    outputProcessorSelector->setEnabled(hasOutputs && ! acquisitionActive);
}

void OptoProtocolEditor::startRecording()
{
    checkForCanvas();

    if (auto* optoCanvas = dynamic_cast<OptoProtocolCanvas*>(canvas.get()))
    {
        File parentDirectory = CoreServices::getRecordingParentDirectory().getChildFile(CoreServices::getRecordingDirectoryName());
        File recordingDirectory = parentDirectory.getChildFile(getProcessor()->getName() + " " + String(getProcessor()->getNodeId()));
        optoCanvas->startRecording(recordingDirectory);
    }
}

void OptoProtocolEditor::stopRecording()
{
    if (auto* optoCanvas = dynamic_cast<OptoProtocolCanvas*>(canvas.get()))
        optoCanvas->stopRecording();
}
