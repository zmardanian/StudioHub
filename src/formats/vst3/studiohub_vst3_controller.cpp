#include "studiohub_vst3_controller.hpp"
#include "studiohub_vst3_ids.hpp"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"

#include <algorithm>
#include <cmath>

namespace StudioHub::Vst3 {

namespace {

constexpr Steinberg::int32 kControllerStateMagic = 0x53484731;
constexpr float kMinLinearGain = 0.0630957344F;
constexpr float kMaxLinearGain = 15.8489319F;

} // namespace

Steinberg::tresult PLUGIN_API Controller::initialize(
    Steinberg::FUnknown* context)
{
    const auto result =
        EditController::initialize(context);

    if (result != Steinberg::kResultOk)
        return result;

    parameters.addParameter(
        STR16("Output Gain"),
        STR16("dB"),
        0,
        0.5,
        Steinberg::Vst::ParameterInfo::kCanAutomate,
        kOutputGainId);

    return result;
}

Steinberg::tresult PLUGIN_API Controller::setComponentState(
    Steinberg::IBStream* state)
{
    if (state == nullptr)
        return Steinberg::kResultFalse;

    Steinberg::IBStreamer streamer(
        state,
        kLittleEndian);

    float gain = 1.0F;

    if (!streamer.readFloat(gain))
        return Steinberg::kResultFalse;

    if (!std::isfinite(gain))
        gain = 1.0F;

    gain = std::clamp(gain, kMinLinearGain, kMaxLinearGain);

    setParamNormalized(
        kOutputGainId,
        std::clamp(
            (20.0 * std::log10(
                static_cast<double>(gain)) + 24.0) / 48.0,
            0.0,
            1.0));

    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Controller::setState(
    Steinberg::IBStream* state)
{
    if (state == nullptr)
        return Steinberg::kResultFalse;

    Steinberg::IBStreamer streamer(state, kLittleEndian);
    Steinberg::int32 magic = 0;
    float normalized = 0.5F;
    if (!streamer.readInt32(magic) ||
        !streamer.readFloat(normalized) ||
        magic != kControllerStateMagic ||
        !std::isfinite(normalized))
        return Steinberg::kResultFalse;

    setParamNormalized(
        kOutputGainId,
        std::clamp(static_cast<double>(normalized), 0.0, 1.0));
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Controller::getState(
    Steinberg::IBStream* state)
{
    if (state == nullptr)
        return Steinberg::kResultFalse;

    auto* parameter = parameters.getParameter(kOutputGainId);
    if (parameter == nullptr)
        return Steinberg::kResultFalse;

    Steinberg::IBStreamer streamer(state, kLittleEndian);
    return streamer.writeInt32(kControllerStateMagic) &&
           streamer.writeFloat(
               static_cast<float>(parameter->getNormalized()))
        ? Steinberg::kResultOk
        : Steinberg::kResultFalse;
}

} // namespace StudioHub::Vst3
