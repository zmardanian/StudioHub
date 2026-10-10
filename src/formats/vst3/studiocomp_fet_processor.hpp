#pragma once

#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <public.sdk/source/vst/vstaudioeffect.h>

#include <array>
#include <atomic>

#include "dsp/fet/studio_comp_fet.hpp"
#include "studiocomp_fet_ids.hpp"

namespace StudioHub::Vst3::FET {

class Processor final : public Steinberg::Vst::AudioEffect {
public:
    Processor();
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor());
    }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setBusArrangements(
        Steinberg::Vst::SpeakerArrangement*, Steinberg::int32,
        Steinberg::Vst::SpeakerArrangement*, Steinberg::int32) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing(
        Steinberg::Vst::ProcessSetup&) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process(
        Steinberg::Vst::ProcessData&) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) SMTG_OVERRIDE;

private:
    Steinberg::Vst::ProcessSetup setup_{};
    studiohub::dsp::StudioCompFET dsp_;
    studiohub::dsp::CompressorParams params_{};
    std::array<std::atomic<double>, kParameterCount> saved_normalized_;
    std::atomic<double> meter_gr_db_{0.0};

    void set_normalized(Steinberg::Vst::ParamID, double) noexcept;
    void refresh_dsp_params() noexcept;
};

} // namespace StudioHub::Vst3::FET
