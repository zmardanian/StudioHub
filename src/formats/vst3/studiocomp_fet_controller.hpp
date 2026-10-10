#pragma once

#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <public.sdk/source/vst/vsteditcontroller.h>

namespace StudioHub::Vst3::FET {

class Controller final : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(new Controller());
    }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream*) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView(const char*) SMTG_OVERRIDE;

private:
    Steinberg::tresult readState(Steinberg::IBStream*);
};

} // namespace StudioHub::Vst3::FET
