#include "studiocomp_fet_processor.hpp"
#include "studiocomp_fet_ids.hpp"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>

namespace StudioHub::Vst3::FET {
namespace {
constexpr Steinberg::int32 kStateMagic = 0x53434631;
constexpr Steinberg::int32 kStateVersion = 2;

studiohub::dsp::CompressorParams decode(
    const std::array<double, kParameterCount>& values) noexcept
{
    return {
        -60.0 + values[kThreshold] * 60.0,
        1.0 + values[kRatio] * 19.0,
        0.05 + values[kAttack] * 99.95,
        5.0 + values[kRelease] * 1995.0,
        -24.0 + values[kInputGain] * 48.0,
        -24.0 + values[kOutputGain] * 48.0,
        values[kBypass] >= 0.5,
        values[kKnee] * 12.0
    };
}
} // namespace

Processor::Processor() : dsp_(48000.0)
{
    setControllerClass(kControllerUID);
    saved_normalized_[kThreshold].store(0.7);
    saved_normalized_[kRatio].store(3.0 / 19.0);
    saved_normalized_[kAttack].store(0.45 / 99.95);
    saved_normalized_[kRelease].store(75.0 / 1995.0);
    saved_normalized_[kInputGain].store(0.5);
    saved_normalized_[kOutputGain].store(0.5);
    saved_normalized_[kBypass].store(0.0);
    saved_normalized_[kKnee].store(0.0);
    refresh_dsp_params();
}

Steinberg::tresult PLUGIN_API Processor::initialize(Steinberg::FUnknown* context)
{
    const auto result = AudioEffect::initialize(context);
    if (result != Steinberg::kResultOk)
        return result;
    addAudioInput(STR16("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Processor::setBusArrangements(
    Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
    Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts)
{
    if (numIns == 1 && numOuts == 1 &&
        inputs[0] == Steinberg::Vst::SpeakerArr::kStereo &&
        outputs[0] == Steinberg::Vst::SpeakerArr::kStereo)
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API Processor::setupProcessing(
    Steinberg::Vst::ProcessSetup& setup)
{
    setup_ = setup;
    dsp_.prepare(setup.sampleRate);
    refresh_dsp_params();
    return AudioEffect::setupProcessing(setup);
}

void Processor::set_normalized(Steinberg::Vst::ParamID id,
                               double value) noexcept
{
    if (id >= kParameterCount || !std::isfinite(value))
        return;
    const auto safe = std::clamp(value, 0.0, 1.0);
    saved_normalized_[static_cast<std::size_t>(id)].store(
        safe, std::memory_order_relaxed);
    refresh_dsp_params();
}

void Processor::refresh_dsp_params() noexcept
{
    std::array<double, kParameterCount> values{};
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = saved_normalized_[i].load(std::memory_order_relaxed);
    params_ = decode(values);
    dsp_.set_params(params_);
}

Steinberg::tresult PLUGIN_API Processor::process(
    Steinberg::Vst::ProcessData& data)
{
    if (data.numInputs < 1 || data.numOutputs < 1 || data.numSamples <= 0)
        return Steinberg::kResultOk;
    auto* in = data.inputs[0].channelBuffers32;
    auto* out = data.outputs[0].channelBuffers32;
    if (!in || !out || data.inputs[0].numChannels < 2 ||
        data.outputs[0].numChannels < 2)
        return Steinberg::kResultOk;

    std::array<Steinberg::Vst::IParamValueQueue*, kParameterCount> queues{};
    std::array<Steinberg::int32, kParameterCount> point_indices{};
    std::array<Steinberg::int32, kParameterCount> point_counts{};
    if (data.inputParameterChanges) {
        const auto count = data.inputParameterChanges->getParameterCount();
        for (Steinberg::int32 i = 0; i < count; ++i) {
            auto* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue)
                continue;
            const auto id = queue->getParameterId();
            if (id < kParameterCount) {
                const auto index = static_cast<std::size_t>(id);
                queues[index] = queue;
                point_counts[index] = queue->getPointCount();
            }
        }
    }

    for (Steinberg::int32 sample = 0; sample < data.numSamples; ++sample) {
        for (Steinberg::int32 id = 0; id < kParameterCount; ++id) {
            const auto index = static_cast<std::size_t>(id);
            auto* queue = queues[index];
            if (!queue)
                continue;
            while (point_indices[index] < point_counts[index]) {
                Steinberg::int32 offset = 0;
                Steinberg::Vst::ParamValue value = 0.0;
                if (queue->getPoint(point_indices[index], offset, value) !=
                    Steinberg::kResultTrue) {
                    point_indices[index] = point_counts[index];
                    break;
                }
                if (offset > sample)
                    break;
                ++point_indices[index];
                set_normalized(id, value);
            }
        }
        out[0][sample] = in[0][sample];
        out[1][sample] = in[1][sample];
        float* channels[2] = {&out[0][sample], &out[1][sample]};
        dsp_.process(channels, 2, 1);
    }

    const double reduction = std::clamp(dsp_.last_gain_reduction_db(), 0.0, 24.0);
    meter_gr_db_.store(reduction, std::memory_order_relaxed);
    if (data.outputParameterChanges) {
        Steinberg::int32 queue_index = 0;
        auto* queue = data.outputParameterChanges->addParameterData(
            kGainReduction, queue_index);
        if (queue) {
            Steinberg::int32 point_index = 0;
            queue->addPoint(data.numSamples - 1, reduction / 24.0, point_index);
        }
    }
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Processor::setState(Steinberg::IBStream* state)
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
    for (Steinberg::Vst::ParamID id = 0; id < kParameterCount; ++id)
        set_normalized(id, values[static_cast<std::size_t>(id)]);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API Processor::getState(Steinberg::IBStream* state)
{
    if (!state)
        return Steinberg::kResultFalse;
    Steinberg::IBStreamer stream(state, kLittleEndian);
    std::array<double, kParameterCount> values{};
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = saved_normalized_[i].load(std::memory_order_relaxed);
    return stream.writeInt32(kStateMagic) &&
           stream.writeInt32(kStateVersion) &&
           stream.writeDoubleArray(values.data(), kParameterCount)
        ? Steinberg::kResultOk : Steinberg::kResultFalse;
}

} // namespace StudioHub::Vst3::FET
