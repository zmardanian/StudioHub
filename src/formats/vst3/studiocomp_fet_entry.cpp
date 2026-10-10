#include "studiocomp_fet_processor.hpp"
#include "studiocomp_fet_controller.hpp"
#include "studiocomp_fet_ids.hpp"
#include "public.sdk/source/main/pluginfactory.h"

BEGIN_FACTORY_DEF("StudioHub", "https://studiohub.local", "plugins@studiohub.local")

DEF_CLASS2(INLINE_UID_FROM_FUID(StudioHub::Vst3::FET::kProcessorUID),
    Steinberg::PClassInfo::kManyInstances, kVstAudioEffectClass,
    "StudioComp FET", Steinberg::Vst::kDistributable, "Fx|Dynamics",
    "0.4.0", kVstVersionString,
    StudioHub::Vst3::FET::Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(StudioHub::Vst3::FET::kControllerUID),
    Steinberg::PClassInfo::kManyInstances, kVstComponentControllerClass,
    "StudioComp FET Controller", 0, "", "0.4.0", kVstVersionString,
    StudioHub::Vst3::FET::Controller::createInstance)

END_FACTORY
