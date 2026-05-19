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

#ifndef OPTOPROTOCOLEDITOR_H_DEFINED
#define OPTOPROTOCOLEDITOR_H_DEFINED

#include <VisualizerEditorHeaders.h>

class OptoProtocolCanvas;

class OptoProtocolEditor : public VisualizerEditor,
                            public ComboBox::Listener,
                            private Timer
{
public:

	OptoProtocolEditor(GenericProcessor* parentNode);

	~OptoProtocolEditor() override;

	Visualizer* createNewCanvas();

	void startAcquisition() override;

	void stopAcquisition() override;

	void startRecording() override;

	void stopRecording() override;

	void refreshOutputProcessorSelector();

private:

	void comboBoxChanged(ComboBox* comboBox) override;

	void timerCallback() override;

	void updateOutputProcessorSelectorEnabled(bool hasOutputs);

	bool outputProcessorSelectorHasOutputs = false;

	std::unique_ptr<Label> outputProcessorLabel;
	std::unique_ptr<ComboBox> outputProcessorSelector;
	String outputProcessorSelectorSignature = "uninitialized";

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OptoProtocolEditor);
};

#endif // OPTOPROTOCOLEDITOR_H_DEFINED
