#pragma once

#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <public.sdk/source/vst/vstaudioeffect.h>

#include <atomic>

#include "dsp/core/dsp_core.hpp"
#include "dsp/core/parameter_smoother.hpp"

namespace StudioHub::Vst3 {

class Processor final : public Steinberg::Vst::AudioEffect {
public:
    Processor();

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(
            new Processor());
    }

    Steinberg::tresult PLUGIN_API initialize(
        Steinberg::FUnknown* context) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API setBusArrangements(
        Steinberg::Vst::SpeakerArrangement* inputs,
        Steinberg::int32 numIns,
        Steinberg::Vst::SpeakerArrangement* outputs,
        Steinberg::int32 numOuts) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API setupProcessing(
        Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API process(
        Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API setState(
        Steinberg::IBStream* state) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API getState(
        Steinberg::IBStream* state) SMTG_OVERRIDE;

private:
    Steinberg::Vst::ProcessSetup setup_{};
    studiohub::dsp::DspCore dsp_;
    studiohub::dsp::LinearParameterSmoother output_gain_;
    std::atomic<float> saved_gain_{1.0F};
};

} // namespace StudioHub::Vst3
