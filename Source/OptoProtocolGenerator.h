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

#ifndef OPTOPROTOCOLGENERATORPLUGIN_H_DEFINED
#define OPTOPROTOCOLGENERATORPLUGIN_H_DEFINED

#include <ProcessorHeaders.h>

class OptoProtocolGenerator : public GenericProcessor
{
public:
	OptoProtocolGenerator();

	~OptoProtocolGenerator();

	AudioProcessorEditor* createEditor() override;

	void saveCustomParametersToXml(XmlElement* parentElement) override;

	void loadCustomParametersFromXml(XmlElement* parentElement) override;

    void process (AudioBuffer<float>& continuousBuffer) override {}

    void sendConfigToNidaqOutput(const String& json);

    Array<int> getAvailableNidaqOutputProcessorIds() const;

    String getNidaqOutputProcessorLabel(int nodeId) const;

    void setSelectedNidaqOutputProcessorId(int nodeId);
    int getSelectedNidaqOutputProcessorId() const;

private:

    GenericProcessor* getSelectedNidaqOutputProcessor() const;
    static bool isNidaqOutputProcessor(GenericProcessor* processor);

    int selectedNidaqOutputProcessorId = -1;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OptoProtocolGenerator);

};

#endif // OPTOPROTOCOLGENERATORPLUGIN_H_DEFINED
