/*
    ------------------------------------------------------------------
    Part of opto-protocol-generator plugin for Open Ephys GUI.
    ------------------------------------------------------------------
*/

#pragma once

#include <ProcessorHeaders.h>

/** Neuropixels-style probe: two selectable emission sites per device row. */
constexpr int kNpOptoSitesPerSource = 2;

struct OptoHardwareLightSource
{
    String name;
    int wavelength = 0;
    int outputChannel = 0;
    Array<float> inputVoltages;
    Array<float> outputPowers;
    String outputUnits;
};

struct OptoHardwareDevice
{
    String name;
    bool is_np_opto = false;
    Array<OptoHardwareLightSource> lightSources;
};

class OptoHardwareConfig
{
public:
    Array<OptoHardwareDevice> devices;

    static std::unique_ptr<OptoHardwareConfig> parseJson(const String& jsonText);
    static std::unique_ptr<OptoHardwareConfig> parseFile(const File& file);

    int maxAnalogOutputChannel() const;
    const OptoHardwareLightSource* findLightSource(int deviceIndex, int wavelengthNm) const;

    /** Piecewise-linear map from pulse power to control voltage using output_powers / input_voltages. */
    static float mapPowerToControlVoltage(float power, const OptoHardwareLightSource& ls);

    bool isEmpty() const { return devices.isEmpty(); }
};
