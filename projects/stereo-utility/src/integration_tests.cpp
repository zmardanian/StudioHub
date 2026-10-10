#include "plugin.cpp"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <array>
#include <cstdio>
#include <cmath>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
struct HostBuffer {
    std::array<double, 1024> left{}, right{}, outLeft{}, outRight{};
    double* inputs[2]{left.data(), right.data()};
    double* outputs[2]{outLeft.data(), outRight.data()};
    AudioBusBuffers input{}, output{};
    ProcessData data{};
    HostBuffer() {
        left.fill(1); right.fill(-0.5);
        input.numChannels = output.numChannels = 2;
        input.channelBuffers64 = inputs; output.channelBuffers64 = outputs;
        data.numInputs = data.numOutputs = 1; data.inputs = &input; data.outputs = &output;
        data.numSamples = 1024; data.symbolicSampleSize = kSample64;
    }
};
void point(ParameterChanges& changes, ParamID id, int offset, double value) {
    int32 index = 0; auto* queue = changes.addParameterData(id, index);
    check(queue && queue->addPoint(offset, value, index) == kResultOk, "host parameter point creation");
}
void rewind(MemoryStream& stream) { stream.seek(0, IBStream::kIBSeekSet, nullptr); }
}
int main() {
    Project::Processor p;
    check(p.initialize(nullptr) == kResultOk, "component initialization");
    ProcessSetup setup{}; setup.sampleRate = 48000; setup.maxSamplesPerBlock = 1024;
    setup.symbolicSampleSize = kSample64; setup.processMode = kRealtime;
    check(p.setupProcessing(setup) == kResultOk, "processing setup");
    HostBuffer host;
    check(p.process(host.data) == kResultOk, "default processing");
    check(host.outLeft[0] == 1 && host.outRight[1023] == -0.5, "fresh plugin defaults preserve stereo unity");
    p.dsp().reset();
    ParameterChanges changes(3); point(changes, Project::kGain, 0, .75); point(changes, Project::kBalance, 100, 1);
    host.data.inputParameterChanges = &changes;
    check(p.process(host.data) == kResultOk, "multi-parameter automation processing");
    check(host.outLeft[0] == 1, "gain automation begins from previous value");
    check(host.outLeft[580] == 0, "later balance event executes after an earlier gain queue ends");
    check(std::abs(host.outRight[600] + .5 * std::pow(10., 12./20)) < 1e-12, "unattenuated channel retains gain");
    changes.clearQueue(); point(changes, Project::kGain, 0, 1); point(changes, Project::kBalance, 0, .25); point(changes, Project::kBypass, 0, 1);
    host.data.numSamples = 0; p.process(host.data);
    MemoryStream state; check(p.getState(&state) == kResultOk, "processor state saved");
    Project::Processor restored; rewind(state);
    check(restored.setState(&state) == kResultOk, "processor state restored");
    MemoryStream pendingState;check(restored.getState(&pendingState)==kResultOk,"pending host state is saveable before processing");
    ProcessData flush{};flush.numSamples=0;restored.process(flush);
    check(restored.dsp().target().gainDb == 24 && restored.dsp().target().balance == -.5 && restored.dsp().target().bypass, "all controls survive state round trip");
    Project::Controller controller; check(controller.initialize(nullptr) == kResultOk, "controller initialization");
    rewind(state); check(controller.setComponentState(&state) == kResultOk, "component state synchronizes editor controls");
    check(controller.getParamNormalized(Project::kGain) == 1 && controller.getParamNormalized(Project::kBalance) == .25 && controller.getParamNormalized(Project::kBypass) == 1, "restored GUI parameter values");
    check(controller.getParameterObject(Project::kBalance)->toPlain(0) == -100, "balance endpoint displays actual percent");
    MemoryStream controllerState; check(controller.getState(&controllerState) == kResultOk, "controller state saved");
    rewind(controllerState); studiohub::stereo_utility::State decoded;
    check(Project::load(&controllerState, decoded) == kResultOk && decoded.balance == -.5, "controller percent mapping preserves stored balance");
    MemoryStream legacy; IBStreamer legacyWriter(&legacy, kLittleEndian); legacyWriter.writeInt32(1); rewind(legacy);
    check(restored.setState(&legacy) == kResultOk,"scaffold v1 state accepted");restored.process(flush);
    check(restored.dsp().target().gainDb == 0,"scaffold v1 state compatibility");
    MemoryStream corrupt; IBStreamer bad(&corrupt, kLittleEndian); bad.writeInt32(999); rewind(corrupt);
    check(restored.setState(&corrupt) != kResultOk, "unsupported state version rejected");
    controller.terminate(); p.terminate();
    if (failures) return 1;
    std::puts("Native VST3 defaults, multi-queue automation, state/controller and compatibility checks passed");
}
