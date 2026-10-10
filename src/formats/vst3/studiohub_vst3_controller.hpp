#pragma once

#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <public.sdk/source/vst/vsteditcontroller.h>

namespace StudioHub::Vst3 {

class Controller final : public Steinberg::Vst::EditController {
public:
    Controller() = default;

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(
            new Controller());
    }

    Steinberg::tresult PLUGIN_API initialize(
        Steinberg::FUnknown* context) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API setComponentState(
        Steinberg::IBStream* state) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API setState(
        Steinberg::IBStream* state) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API getState(
        Steinberg::IBStream* state) SMTG_OVERRIDE;
};

} // namespace StudioHub::Vst3
