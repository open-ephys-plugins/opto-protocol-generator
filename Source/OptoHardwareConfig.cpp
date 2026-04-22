/*
    ------------------------------------------------------------------
    Part of opto-protocol-generator plugin for Open Ephys GUI.
    ------------------------------------------------------------------
*/

#include "OptoHardwareConfig.h"

using namespace juce;

static void parseFloatArray(const var& v, Array<float>& out)
{
    out.clear();
    if (auto* a = v.getArray())
        for (auto& x : *a)
            out.add((float) static_cast<double>(x));
}

static OptoHardwareLightSource parseLightSource(DynamicObject& o)
{
    OptoHardwareLightSource ls;
    ls.name = o.getProperty("name").toString();
    ls.wavelength = (int) o.getProperty("wavelength");
    ls.outputChannel = (int) o.getProperty("output_channel");
    ls.outputUnits = o.getProperty("output_units").toString();
    parseFloatArray(o.getProperty("input_voltages"), ls.inputVoltages);
    parseFloatArray(o.getProperty("output_powers"), ls.outputPowers);
    return ls;
}

static OptoHardwareDevice parseDevice(DynamicObject& o)
{
    OptoHardwareDevice d;
    d.name = o.getProperty("name").toString();
    d.is_np_opto = (bool) o.getProperty("is_np_opto");
    var lsVar = o.getProperty("light_sources");
    if (auto* arr = lsVar.getArray())
        for (auto& item : *arr)
            if (auto* lo = item.getDynamicObject())
                d.lightSources.add(parseLightSource(*lo));
    return d;
}

std::unique_ptr<OptoHardwareConfig> OptoHardwareConfig::parseJson(const String& jsonText)
{
    auto parsed = JSON::parse(jsonText);
    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return nullptr;
    var devVar = root->getProperty("devices");
    auto* arr = devVar.getArray();
    if (arr == nullptr || arr->isEmpty())
        return nullptr;
    auto cfg = std::make_unique<OptoHardwareConfig>();
    for (auto& dv : *arr)
    {
        if (auto* dObj = dv.getDynamicObject())
        {
            auto d = parseDevice(*dObj);
            if (d.name.isNotEmpty() && d.lightSources.size() > 0)
                cfg->devices.add(d);
        }
    }
    if (cfg->devices.isEmpty())
        return nullptr;
    return cfg;
}

std::unique_ptr<OptoHardwareConfig> OptoHardwareConfig::parseFile(const File& file)
{
    if (!file.existsAsFile())
        return nullptr;
    return parseJson(file.loadFileAsString());
}

int OptoHardwareConfig::maxAnalogOutputChannel() const
{
    int m = 0;
    for (auto& d : devices)
        for (auto& ls : d.lightSources)
            m = jmax(m, ls.outputChannel);
    return m;
}

const OptoHardwareLightSource* OptoHardwareConfig::findLightSource(int deviceIndex, int wavelengthNm) const
{
    if (deviceIndex < 0 || deviceIndex >= devices.size())
        return nullptr;
    for (auto& ls : devices[deviceIndex].lightSources)
        if (ls.wavelength == wavelengthNm)
            return &ls;
    return nullptr;
}

float OptoHardwareConfig::mapPowerToControlVoltage(float power, const OptoHardwareLightSource& ls)
{
    const int n = jmin(ls.outputPowers.size(), ls.inputVoltages.size());
    if (n <= 0)
        return 0.f;
    if (n == 1)
        return ls.inputVoltages[0];
    const float pLo = ls.outputPowers[0];
    const float pHi = ls.outputPowers[n - 1];
    const float pClamp = jlimit(pLo, pHi, power);
    for (int i = 1; i < n; ++i)
    {
        const float p0 = ls.outputPowers[i - 1];
        const float p1 = ls.outputPowers[i];
        if (pClamp <= p1 || i == n - 1)
        {
            const float t = (p1 > p0) ? jlimit(0.f, 1.f, (pClamp - p0) / (p1 - p0)) : 0.f;
            return ls.inputVoltages[i - 1] + t * (ls.inputVoltages[i] - ls.inputVoltages[i - 1]);
        }
    }
    return ls.inputVoltages[n - 1];
}
