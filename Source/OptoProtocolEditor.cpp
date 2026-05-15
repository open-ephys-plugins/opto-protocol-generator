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

    //addSelectedChannelsParameterEditor("Channels", 20, 105);

}

Visualizer* OptoProtocolEditor::createNewCanvas()
{
    thisCanvas = new OptoProtocolCanvas((OptoProtocolGenerator*) getProcessor());
	return thisCanvas;
}



void OptoProtocolEditor::startRecording()
{

    File parentDirectory = CoreServices::getRecordingParentDirectory().getChildFile(CoreServices::getRecordingDirectoryName());

    File recordingDirectory = parentDirectory.getChildFile(getProcessor()->getName() + " " + String(getProcessor()->getNodeId()));

    thisCanvas->startRecording(recordingDirectory);
}

void OptoProtocolEditor::stopRecording()
{

    thisCanvas->stopRecording();

}
