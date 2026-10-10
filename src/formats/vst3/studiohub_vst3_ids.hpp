#pragma once

#include <pluginterfaces/base/funknown.h>

namespace StudioHub::Vst3 {

static const Steinberg::FUID kProcessorUID(
    0xA71C4E21, 0x5B3D4F82, 0x9C17A6E4, 0x2D8B5310);

static const Steinberg::FUID kControllerUID(
    0xD4826F19, 0x37A5C8E2, 0xB9145D63, 0x7E20AF48);

enum ParameterId : Steinberg::Vst::ParamID {
    kOutputGainId = 0
};

} // namespace StudioHub::Vst3
