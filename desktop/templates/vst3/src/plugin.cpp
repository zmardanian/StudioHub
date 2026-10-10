#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "base/source/fstreamer.h"
#include <cstring>
namespace Project {
using namespace Steinberg;
using namespace Steinberg::Vst;
const FUID processorUID(@PROCESSOR_UID@);
const FUID controllerUID(@CONTROLLER_UID@);
class Processor final : public AudioEffect {
public:
 Processor() { setControllerClass(controllerUID); }
 static FUnknown* create(void*) { return static_cast<IAudioProcessor*>(new Processor); }
 tresult PLUGIN_API initialize(FUnknown* c) override {
  auto r=AudioEffect::initialize(c); if(r!=kResultOk) return r;
  addAudioInput(STR16("Stereo In"),SpeakerArr::kStereo);
  addAudioOutput(STR16("Stereo Out"),SpeakerArr::kStereo); return r;
 }
 tresult PLUGIN_API setBusArrangements(SpeakerArrangement* i,int32 ni,SpeakerArrangement* o,int32 no) override {
  if(ni!=1 || no!=1 || i[0]!=SpeakerArr::kStereo || o[0]!=SpeakerArr::kStereo) return kResultFalse;
  return AudioEffect::setBusArrangements(i,ni,o,no);
 }
 tresult PLUGIN_API canProcessSampleSize(int32 s) override { return s==kSample32 || s==kSample64 ? kResultTrue:kResultFalse; }
 tresult PLUGIN_API process(ProcessData& d) override {
  if(d.numInputs<1 || d.numOutputs<1) return kResultOk;
  // Initial scaffold is stereo passthrough. Implement SPEC.md before acceptance.
  for(int32 c=0;c<2;++c) for(int32 n=0;n<d.numSamples;++n)
   if(d.symbolicSampleSize==kSample32) d.outputs[0].channelBuffers32[c][n]=d.inputs[0].channelBuffers32[c][n];
   else d.outputs[0].channelBuffers64[c][n]=d.inputs[0].channelBuffers64[c][n];
  d.outputs[0].silenceFlags=d.inputs[0].silenceFlags; return kResultOk;
 }
 tresult PLUGIN_API getState(IBStream* s) override { IBStreamer stream(s,kLittleEndian); return stream.writeInt32(1)?kResultOk:kResultFalse; }
 tresult PLUGIN_API setState(IBStream* s) override { IBStreamer stream(s,kLittleEndian); int32 v=0; return stream.readInt32(v)&&v==1?kResultOk:kResultFalse; }
};
class Controller final : public EditController {
public:
 static FUnknown* create(void*) { return static_cast<IEditController*>(new Controller); }
 IPlugView* PLUGIN_API createView(FIDString name) override {
  return std::strcmp(name,ViewType::kEditor)==0 ? new VSTGUI::VST3Editor(this,"Editor","editor.uidesc") : nullptr;
 }
};
}
BEGIN_FACTORY_DEF("StudioHub","https://studiohub.local","plugins@studiohub.local")
DEF_CLASS2(INLINE_UID_FROM_FUID(Project::processorUID), Steinberg::PClassInfo::kManyInstances,kVstAudioEffectClass,"@DISPLAY@",Steinberg::Vst::kDistributable,"Fx|Tools","0.1.0",kVstVersionString,Project::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(Project::controllerUID), Steinberg::PClassInfo::kManyInstances,kVstComponentControllerClass,"@DISPLAY@ Controller",0,"","0.1.0",kVstVersionString,Project::Controller::create)
END_FACTORY
