#pragma once

#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/vsttypes.h>

namespace StudioHub::Vst3::FET {

static const Steinberg::FUID kProcessorUID(
    0x8BDF205A, 0x1E7A4F76, 0xAE350C91, 0xD642B8F3);
static const Steinberg::FUID kControllerUID(
    0x2C4E7A19, 0xB5034FD8, 0x9A7E21C6, 0x5D308B42);

enum ParameterId : Steinberg::Vst::ParamID {
    kThreshold = 0,
    kRatio = 1,
    kAttack = 2,
    kRelease = 3,
    kInputGain = 4,
    kOutputGain = 5,
    kBypass = 6,
    kKnee = 7,
    kGainReduction = 8,
    kParameterCount = 8
};

} // namespace StudioHub::Vst3::FET
