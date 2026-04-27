/*
    ------------------------------------------------------------------
    Part of opto-protocol-generator plugin for Open Ephys GUI.
    ------------------------------------------------------------------
*/

#include "OptoHardwareConfig.h"

using namespace juce;

static void parseFloatArray(const var& v, std::vector<float>& out)
{
    out.clear();
    if (auto* a = v.getArray())
        for (auto& x : *a)
            out.push_back((float) static_cast<double>(x));
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

    //print loaded ls to console
    std::cout << "Loaded light source: " << ls.name << " " << ls.wavelength << " " << ls.outputChannel << " " << ls.outputUnits << " " << ls.inputVoltages.size() << " " << ls.outputPowers.size() << std::endl;
    for (int i = 0; i < ls.inputVoltages.size(); i++)
    {
        std::cout << "Input voltage " << i << ": " << ls.inputVoltages[i] << std::endl;
    }
    for (int i = 0; i < ls.outputPowers.size(); i++)
    {
        std::cout << "Output power " << i << ": " << ls.outputPowers[i] << std::endl;
    }
    std::cout << std::endl;
    return ls;
}

static bool isValidLightSource(const OptoHardwareLightSource& ls)
{
    if (ls.name.isEmpty() || ls.wavelength <= 0 || ls.outputChannel < 0)
        return false;

    const int nIn = ls.inputVoltages.size();
    const int nOut = ls.outputPowers.size();
    if (nIn == 0 || nOut == 0 || nIn != nOut)
        return false;

    return true;
}

static bool parseDevice(DynamicObject& o, OptoHardwareDevice& outDevice)
{
    outDevice = {};
    outDevice.name = o.getProperty("name").toString();
    outDevice.is_np_opto = (bool) o.getProperty("is_np_opto");
    if (outDevice.name.isEmpty())
        return false;

    var lsVar = o.getProperty("light_sources");
    auto* arr = lsVar.getArray();
    if (arr == nullptr || arr->isEmpty())
        return false;

    for (auto& item : *arr)
    {
        auto* lo = item.getDynamicObject();
        if (lo == nullptr)
            return false;

        auto ls = parseLightSource(*lo);
        if (!isValidLightSource(ls))
            return false;

        outDevice.lightSources.push_back(ls);
    }

    return !outDevice.lightSources.empty();
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
            OptoHardwareDevice d;
            if (!parseDevice(*dObj, d))
                return nullptr;
            cfg->devices.push_back(d);
        }
        else
            return nullptr;
    }
    if (cfg->devices.empty())
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
    if (deviceIndex < 0 || deviceIndex >= (int) devices.size())
        return nullptr;
    for (auto& ls : devices[deviceIndex].lightSources)
        if (ls.wavelength == wavelengthNm)
            return &ls;
    return nullptr;
}

float OptoHardwareConfig::mapPowerToControlVoltage(float power, const OptoHardwareLightSource& ls)
{
    const int n = jmin((int) ls.outputPowers.size(), (int) ls.inputVoltages.size());
    if (n <= 0)
        return 0.f;
    if (n == 1)
        return ls.inputVoltages[0];

    int minPowerIdx = 0;
    int maxPowerIdx = 0;
    for (int i = 1; i < n; ++i)
    {
        if (ls.outputPowers[i] < ls.outputPowers[minPowerIdx])
            minPowerIdx = i;
        if (ls.outputPowers[i] > ls.outputPowers[maxPowerIdx])
            maxPowerIdx = i;
    }

    if (power <= ls.outputPowers[minPowerIdx])
    {
        LOGC("Mapping output power " + String(power) + " to voltage " + String(ls.inputVoltages[minPowerIdx]));
        return ls.inputVoltages[minPowerIdx];
    }
    if (power >= ls.outputPowers[maxPowerIdx])
    {
        LOGC("Mapping output power " + String(power) + " to voltage " + String(ls.inputVoltages[maxPowerIdx]));
        return ls.inputVoltages[maxPowerIdx];
    }

    for (int i = 1; i < n; ++i)
    {
        const float p0 = ls.outputPowers[i - 1];
        const float p1 = ls.outputPowers[i];
        const float pMin = jmin(p0, p1);
        const float pMax = jmax(p0, p1);
        if (power >= pMin && power <= pMax)
        {
            const float t = (p1 != p0) ? jlimit(0.f, 1.f, (power - p0) / (p1 - p0)) : 0.f;
            const float voltage = ls.inputVoltages[i - 1] + t * (ls.inputVoltages[i] - ls.inputVoltages[i - 1]);
            LOGC("Mapping output power " + String(power) + " to voltage " + String(voltage));
            return voltage;
        }
    }

    LOGC("Mapping output power " + String(power) + " to voltage " + String(ls.inputVoltages[maxPowerIdx]));
    return ls.inputVoltages[maxPowerIdx];
}
