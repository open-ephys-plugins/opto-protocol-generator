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
#ifndef OPTOPROTOCOLGENERATORPLUGIN_H_DEFINED
#define OPTOPROTOCOLGENERATORPLUGIN_H_DEFINED

#include <ProcessorHeaders.h>


/** 
	A plugin for defining a custom protocol for optogenetic stimulation.

	The plugin doesn't do any processing, it just creates a user interface
	for building a protocol consisting of a set of conditions.

	This is a meant to be a convenient way to define and share protocols for 
	experiments.
*/

class OptoProtocolGenerator : public GenericProcessor
{
public:
	/** The class constructor, used to initialize any members.*/
	OptoProtocolGenerator();

	/** The class destructor, used to deallocate memory*/
	~OptoProtocolGenerator();

	/** If the processor has a custom editor, this method must be defined to instantiate it. */
	AudioProcessorEditor* createEditor() override;

	/** Saving custom settings to XML. This method is not needed to save the state of
		Parameter objects */
	void saveCustomParametersToXml(XmlElement* parentElement) override;

	/** Load custom settings from XML. This method is not needed to load the state of
		Parameter objects*/
	void loadCustomParametersFromXml(XmlElement* parentElement) override;
    
    
    /** Process function (not used)  */
    void process (AudioBuffer<float>& continuousBuffer) override {}

private:

	/** Generates an assertion if this class leaks */
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OptoProtocolGenerator);

};

#endif // OPTOPROTOCOLGENERATORPLUGIN_H_DEFINED
