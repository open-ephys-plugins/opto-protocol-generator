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

}


void OptoProtocolGenerator::loadCustomParametersFromXml(XmlElement* parentElement)
{

}
