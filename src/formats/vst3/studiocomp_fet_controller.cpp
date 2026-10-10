#include "studiocomp_fet_controller.hpp"
#include "studiocomp_fet_ids.hpp"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <string_view>

namespace StudioHub::Vst3::FET {
namespace {
constexpr Steinberg::int32 kStateMagic = 0x53434631;
constexpr Steinberg::int32 kStateVersion = 2;
}

Steinberg::tresult PLUGIN_API Controller::initialize(Steinberg::FUnknown* context)
{
    const auto result = EditController::initialize(context);
    if (result != Steinberg::kResultOk)
        return result;

    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Threshold"), kThreshold, STR16("dB"), -60.0, 0.0, -18.0));
    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Ratio"), kRatio, STR16(":1"), 1.0, 20.0, 4.0));
    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Attack"), kAttack, STR16("ms"), 0.05, 100.0, 0.5));
    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Release"), kRelease, STR16("ms"), 5.0, 2000.0, 80.0));
    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Input"), kInputGain, STR16("dB"), -24.0, 24.0, 0.0));
    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Output"), kOutputGain, STR16("dB"), -24.0, 24.0, 0.0));
    parameters.addParameter(new Steinberg::Vst::Parameter(
        STR16("Bypass"), kBypass, nullptr, 0.0, 1,
        Steinberg::Vst::ParameterInfo::kCanAutomate |
        Steinberg::Vst::ParameterInfo::kIsBypass));
    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Knee"), kKnee, STR16("dB"), 0.0, 12.0, 0.0));
    parameters.addParameter(new Steinberg::Vst::Parameter(
        STR16("Gain Reduction"), kGainReduction, STR16("dB"), 0.0, 0,
        Steinberg::Vst::ParameterInfo::kIsReadOnly));
    return result;
}

Steinberg::IPlugView* PLUGIN_API Controller::createView(const char* name)
{
    if (name && std::string_view(name) == Steinberg::Vst::ViewType::kEditor)
        return new VSTGUI::VST3Editor(this, "Editor", "studiocomp_fet.uidesc");
    return nullptr;
}

Steinberg::tresult Controller::readState(Steinberg::IBStream* state)
{
    if (!state)
        return Steinberg::kResultFalse;
    Steinberg::IBStreamer stream(state, kLittleEndian);
    Steinberg::int32 magic = 0;
    Steinberg::int32 version = 0;
    std::array<double, kParameterCount> values{};
    if (!stream.readInt32(magic) || !stream.readInt32(version) ||
        magic != kStateMagic || (version != 1 && version != kStateVersion) ||
        !stream.readDoubleArray(values.data(),
            version == 1 ? kParameterCount - 1 : kParameterCount))
        return Steinberg::kResultFalse;
    if (version == 1)
        values[kKnee] = 0.0;
    for (Steinberg::Vst::ParamID id = 0; id < kParameterCount; ++id) {
        const auto i = static_cast<std::size_t>(id);
        if (id == kBypass)
            values[i] = values[i] >= 0.5 ? 1.0 : 0.0;
        if (!std::isfinite(values[i]))
            return Steinberg::kResultFalse;
        setParamNormalized(id, std::clamp(values[i], 0.0, 1.0));
    }
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Controller::setComponentState(
    Steinberg::IBStream* state)
{
    return readState(state);
}

Steinberg::tresult PLUGIN_API Controller::setState(Steinberg::IBStream* state)
{
    return readState(state);
}

Steinberg::tresult PLUGIN_API Controller::getState(Steinberg::IBStream* state)
{
    if (!state)
        return Steinberg::kResultFalse;
    Steinberg::IBStreamer stream(state, kLittleEndian);
    std::array<double, kParameterCount> values{};
    for (Steinberg::Vst::ParamID id = 0; id < kParameterCount; ++id)
        values[static_cast<std::size_t>(id)] = getParamNormalized(id);
    return stream.writeInt32(kStateMagic) &&
           stream.writeInt32(kStateVersion) &&
           stream.writeDoubleArray(values.data(), kParameterCount)
        ? Steinberg::kResultOk : Steinberg::kResultFalse;
}

} // namespace StudioHub::Vst3::FET
