#include "stereo_utility_dsp.hpp"
#include "state_exchange.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace Project {
using namespace Steinberg; using namespace Steinberg::Vst; using studiohub::stereo_utility::State; using Dsp=studiohub::stereo_utility::Processor;
const FUID processorUID(0xf4b73757,0x79684baf,0xa2fe9eea,0xe86b0cc1);
const FUID controllerUID(0xcbb2c847,0xe2dd481a,0xbc41eff4,0xb1fafe39);
enum : ParamID { kGain=100,kBalance=101,kBypass=102 };
static tresult save(IBStream* s,State x){if(!s)return kResultFalse;IBStreamer w(s,kLittleEndian);return w.writeInt32(2)&&w.writeDouble(x.gainDb)&&w.writeDouble(x.balance)&&w.writeInt32(x.bypass?1:0)?kResultOk:kResultFalse;}
static tresult load(IBStream* s,State& x){if(!s)return kResultFalse;IBStreamer r(s,kLittleEndian);int32 v=0;if(!r.readInt32(v))return kResultFalse;if(v==1){x={};return kResultOk;}double g,b;int32 y;if(v!=2||!r.readDouble(g)||!r.readDouble(b)||!r.readInt32(y)||!std::isfinite(g)||!std::isfinite(b)||g< -24||g>24||b< -1||b>1||(y!=0&&y!=1))return kResultFalse;x={g,b,y!=0};return kResultOk;}
class Processor final:public AudioEffect {
 Dsp dsp_;
 studiohub::stereo_utility::StateExchange state_;
 void apply(ParamID id,ParamValue v){v=studiohub::stereo_utility::normalized(v);if(id==kGain)dsp_.setGain(-24+48*v);else if(id==kBalance)dsp_.setBalance(-1+2*v);else if(id==kBypass)dsp_.setBypass(v>=.5);state_.cache(dsp_.target());}
public:
 Processor(){setControllerClass(controllerUID);} static FUnknown* create(void*){return static_cast<IAudioProcessor*>(new Processor);}
 tresult PLUGIN_API initialize(FUnknown* c)override{auto r=AudioEffect::initialize(c);if(r!=kResultOk)return r;addAudioInput(STR16("Stereo In"),SpeakerArr::kStereo);addAudioOutput(STR16("Stereo Out"),SpeakerArr::kStereo);return r;}
 tresult PLUGIN_API setBusArrangements(SpeakerArrangement*i,int32 ni,SpeakerArrangement*o,int32 no)override{if(ni!=1||no!=1||i[0]!=SpeakerArr::kStereo||o[0]!=SpeakerArr::kStereo)return kResultFalse;return AudioEffect::setBusArrangements(i,ni,o,no);}
 tresult PLUGIN_API canProcessSampleSize(int32 s)override{return s==kSample32||s==kSample64?kResultTrue:kResultFalse;}
 tresult PLUGIN_API setupProcessing(ProcessSetup&s)override{auto r=AudioEffect::setupProcessing(s);dsp_.setSampleRate(s.sampleRate);return r;}
 tresult PLUGIN_API process(ProcessData&d)override{
  if(d.numSamples<0)return kResultOk;
  State pending;std::uint64_t epoch=0;
  if(state_.pending(pending,epoch)){dsp_.reset(pending);state_.acknowledge(pending,epoch);}
  auto* q=d.inputParameterChanges;
  if(d.numSamples==0||d.numInputs<1||d.numOutputs<1||!d.inputs||!d.outputs){
   if(q){int32 n=std::min<int32>(q->getParameterCount(),3);for(int32 i=0;i<n;++i){auto*p=q->getParameterData(i);if(!p)continue;auto id=p->getParameterId();if(id!=kGain&&id!=kBalance&&id!=kBypass)continue;ParamValue value=0;bool have=false;for(int32 j=0;j<p->getPointCount();++j){int32 offset=0;if(p->getPoint(j,offset,value)==kResultOk)have=true;}if(have)apply(id,value);}}
   if(d.numOutputs>0&&d.outputs)d.outputs[0].silenceFlags=3;return kResultOk;
  }
  int32 pos=0;
  if(d.inputs[0].numChannels!=2 || d.outputs[0].numChannels!=2) return kResultFalse;
  if(d.symbolicSampleSize==kSample32 && (!d.inputs[0].channelBuffers32 || !d.outputs[0].channelBuffers32 || !d.inputs[0].channelBuffers32[0] || !d.inputs[0].channelBuffers32[1] || !d.outputs[0].channelBuffers32[0] || !d.outputs[0].channelBuffers32[1])) return kResultFalse;
  if(d.symbolicSampleSize==kSample64 && (!d.inputs[0].channelBuffers64 || !d.outputs[0].channelBuffers64 || !d.inputs[0].channelBuffers64[0] || !d.inputs[0].channelBuffers64[1] || !d.outputs[0].channelBuffers64[0] || !d.outputs[0].channelBuffers64[1])) return kResultFalse;
  // Host queues are visited once; their points are applied in timestamp order within each parameter.
  // Work is bounded by sample count plus the initial count of supplied host events.
  struct Cursor{IParamValueQueue*q{};int32 i{},offset{},count{};ParamValue value{};ParamID id{};bool active{};} cursors[3];int used=0;
  if(q){int32 n=std::min<int32>(q->getParameterCount(),3);for(int32 i=0;i<n;++i){auto*p=q->getParameterData(i);if(!p)continue;auto id=p->getParameterId();if(id!=kGain&&id!=kBalance&&id!=kBypass)continue;auto&c=cursors[used];c.q=p;c.id=id;c.count=std::max<int32>(0,p->getPointCount());c.active=c.count>0;if(c.active){int32 off=0;if(p->getPoint(0,off,c.value)!=kResultOk)c.active=false;else{c.offset=std::clamp(off,0,d.numSamples);c.i=1;}}++used;}}
  auto run=[&](int32 a,int32 b){if(a>=b)return;if(d.symbolicSampleSize==kSample32)dsp_.process(d.inputs[0].channelBuffers32[0]+a,d.inputs[0].channelBuffers32[1]+a,d.outputs[0].channelBuffers32[0]+a,d.outputs[0].channelBuffers32[1]+a,b-a);else if(d.symbolicSampleSize==kSample64)dsp_.process(d.inputs[0].channelBuffers64[0]+a,d.inputs[0].channelBuffers64[1]+a,d.outputs[0].channelBuffers64[0]+a,d.outputs[0].channelBuffers64[1]+a,b-a);};
  for(;;){int32 next=d.numSamples;for(int i=0;i<used;++i)if(cursors[i].active)next=std::min(next,cursors[i].offset);run(pos,next);pos=next;for(int i=0;i<used;++i){auto&c=cursors[i];if(c.active&&c.offset==pos){apply(c.id,c.value);int32 off=0;if(c.i<c.count&&c.q->getPoint(c.i++,off,c.value)==kResultOk){c.offset=std::clamp(off,pos,d.numSamples);}else c.active=false;}}if(pos>=d.numSamples)break;}
  uint64 silent=0;
  if(d.symbolicSampleSize==kSample32){for(int c=0;c<2;++c){bool zero=true;for(int32 i=0;i<d.numSamples;++i)zero=zero&&d.outputs[0].channelBuffers32[c][i]==0;if(zero)silent|=uint64(1)<<c;}}
  else if(d.symbolicSampleSize==kSample64){for(int c=0;c<2;++c){bool zero=true;for(int32 i=0;i<d.numSamples;++i)zero=zero&&d.outputs[0].channelBuffers64[c][i]==0;if(zero)silent|=uint64(1)<<c;}}
  d.outputs[0].silenceFlags=silent;return kResultOk;
 }
 tresult PLUGIN_API getState(IBStream*s)override{return save(s,state_.requested());} tresult PLUGIN_API setState(IBStream*s)override{State x;if(load(s,x)!=kResultOk)return kResultFalse;state_.publish(x);return kResultOk;}
 Dsp& dsp(){return dsp_;}
};
class Controller final:public EditController {
public:
 static FUnknown* create(void*){return static_cast<IEditController*>(new Controller);}
 tresult PLUGIN_API initialize(FUnknown*c)override{auto r=EditController::initialize(c);if(r!=kResultOk)return r;constexpr int32 automate=ParameterInfo::kCanAutomate;parameters.addParameter(new RangeParameter(STR16("Gain"),kGain,STR16("dB"),-24,24,0,kStepCountContinuous,automate,kRootUnitId,STR16("Gain")));parameters.addParameter(new RangeParameter(STR16("Balance"),kBalance,STR16("%"),-100,100,0,kStepCountContinuous,automate,kRootUnitId,STR16("Balance")));parameters.addParameter(new RangeParameter(STR16("Bypass"),kBypass,nullptr,0,1,0,1,automate|ParameterInfo::kIsBypass,kRootUnitId,STR16("Bypass")));return r;}
 tresult PLUGIN_API setComponentState(IBStream*s)override{State x;if(load(s,x)!=kResultOk)return kResultFalse;setParamNormalized(kGain,(x.gainDb+24)/48);setParamNormalized(kBalance,(x.balance+1)/2);setParamNormalized(kBypass,x.bypass?1:0);return kResultOk;}
 tresult PLUGIN_API getState(IBStream*s)override{auto*g=getParameterObject(kGain);auto*b=getParameterObject(kBalance);return save(s,{g->toPlain(getParamNormalized(kGain)),b->toPlain(getParamNormalized(kBalance))/100.,getParamNormalized(kBypass)>=.5});}
 tresult PLUGIN_API setState(IBStream*s)override{return setComponentState(s);}
 IPlugView* PLUGIN_API createView(FIDString n)override{return std::strcmp(n,ViewType::kEditor)==0?new VSTGUI::VST3Editor(this,"Editor","editor.uidesc"):nullptr;}
};
}
BEGIN_FACTORY_DEF("StudioHub","https://studiohub.local","plugins@studiohub.local")
DEF_CLASS2(INLINE_UID_FROM_FUID(Project::processorUID),Steinberg::PClassInfo::kManyInstances,kVstAudioEffectClass,"StudioHub Stereo Utility",Steinberg::Vst::kDistributable,"Fx|Tools","0.1.0",kVstVersionString,Project::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(Project::controllerUID),Steinberg::PClassInfo::kManyInstances,kVstComponentControllerClass,"StudioHub Stereo Utility Controller",0,"","0.1.0",kVstVersionString,Project::Controller::create)
END_FACTORY
