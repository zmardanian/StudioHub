#include "studiohub_vst3_processor.hpp"
#include "studiohub_vst3_ids.hpp"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>

namespace StudioHub::Vst3 {

namespace {

constexpr float kMinLinearGain = 0.0630957344F;
constexpr float kMaxLinearGain = 15.8489319F;

} // namespace

Processor::Processor()
    : dsp_(48000.0),
      output_gain_(48000.0)
{
    setControllerClass(kControllerUID);
}

Steinberg::tresult PLUGIN_API Processor::initialize(
    Steinberg::FUnknown* context)
{
    const auto result = AudioEffect::initialize(context);
    if (result != Steinberg::kResultOk)
        return result;

    addAudioInput(
        STR16("Stereo In"),
        Steinberg::Vst::SpeakerArr::kStereo);

    addAudioOutput(
        STR16("Stereo Out"),
        Steinberg::Vst::SpeakerArr::kStereo);

    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Processor::setBusArrangements(
    Steinberg::Vst::SpeakerArrangement* inputs,
    Steinberg::int32 numIns,
    Steinberg::Vst::SpeakerArrangement* outputs,
    Steinberg::int32 numOuts)
{
    if (numIns == 1 && numOuts == 1 &&
        inputs[0] == Steinberg::Vst::SpeakerArr::kStereo &&
        outputs[0] == Steinberg::Vst::SpeakerArr::kStereo)
    {
        return AudioEffect::setBusArrangements(
            inputs, numIns, outputs, numOuts);
    }

    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API Processor::setupProcessing(
    Steinberg::Vst::ProcessSetup& newSetup)
{
    setup_ = newSetup;

    dsp_ = studiohub::dsp::DspCore(newSetup.sampleRate);

    output_gain_.prepare(newSetup.sampleRate);
    output_gain_.reset(
        static_cast<double>(saved_gain_.load(std::memory_order_relaxed)));

    return AudioEffect::setupProcessing(newSetup);
}

Steinberg::tresult PLUGIN_API Processor::process(
    Steinberg::Vst::ProcessData& data)
{
    if (data.numInputs < 1 || data.numOutputs < 1)
        return Steinberg::kResultOk;

    if (data.numSamples <= 0)
        return Steinberg::kResultOk;

    if (data.inputParameterChanges != nullptr)
    {
        const auto count = data.inputParameterChanges->getParameterCount();

        for (Steinberg::int32 i = 0; i < count; ++i)
        {
            auto* queue =
                data.inputParameterChanges->getParameterData(i);

            if (queue == nullptr || queue->getPointCount() <= 0)
                continue;

            Steinberg::int32 sampleOffset = 0;
            Steinberg::Vst::ParamValue value = 1.0;

            if (queue->getPoint(
                    queue->getPointCount() - 1,
                    sampleOffset,
                    value) != Steinberg::kResultTrue)
            {
                continue;
            }

            if (queue->getParameterId() == kOutputGainId)
            {
                const double normalized =
                    std::clamp(value, 0.0, 1.0);

                constexpr double kMinDb = -24.0;
                constexpr double kMaxDb = 24.0;

                const double db =
                    kMinDb + normalized * (kMaxDb - kMinDb);

                const double linear =
                    std::pow(10.0, db / 20.0);

                saved_gain_.store(
                    static_cast<float>(linear),
                    std::memory_order_relaxed);
                output_gain_.set_target(linear, 0.010);
            }
        }
    }

    auto* in = data.inputs[0].channelBuffers32;
    auto* out = data.outputs[0].channelBuffers32;

    if (in == nullptr || out == nullptr)
        return Steinberg::kResultOk;

    for (Steinberg::int32 sample = 0;
         sample < data.numSamples;
         ++sample)
    {
        const float gain =
            static_cast<float>(output_gain_.get_next_value());

        float left = in[0][sample];
        float right = in[1][sample];

        dsp_.process(std::span<float>(&left, 1));
        dsp_.process(std::span<float>(&right, 1));

        out[0][sample] = left * gain;
        out[1][sample] = right * gain;
    }

    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Processor::setState(
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

    output_gain_.reset(static_cast<double>(gain));
    saved_gain_.store(gain, std::memory_order_relaxed);

    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Processor::getState(
    Steinberg::IBStream* state)
{
    if (state == nullptr)
        return Steinberg::kResultFalse;

    Steinberg::IBStreamer streamer(
        state,
        kLittleEndian);

    const float gain = saved_gain_.load(std::memory_order_relaxed);

    return streamer.writeFloat(gain)
        ? Steinberg::kResultOk
        : Steinberg::kResultFalse;
}

} // namespace StudioHub::Vst3
