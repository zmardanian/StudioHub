#include "stereo_utility_dsp.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
using namespace studiohub::stereo_utility;
struct Result{int rate;double gainErrorDb,channelError;bool bypassTransparent;int smoothingSamples;double balanceError;};
Result measure(int rate){const int n=rate/20;Processor p;p.setSampleRate(rate);p.reset({6,0,false});std::vector<double> l(n,.2),r(n,-.1),ol(n),orr(n);p.process(l.data(),r.data(),ol.data(),orr.data(),n);const double expected=std::pow(10.,6./20);double err=0,chan=0;for(int i=static_cast<int>(rate*.01);i<n;++i){err=std::max(err,std::abs(20*std::log10(std::abs(ol[i]/.2))-6));chan=std::max(chan,std::abs(orr[i]/r[i]-expected));}p.reset({6,.5,true});p.process(l.data(),r.data(),ol.data(),orr.data(),n);bool transparent=true;for(int i=0;i<n;++i)transparent&=ol[i]==l[i]&&orr[i]==r[i];p.reset({0,0,false});p.setGain(6);p.process(l.data(),r.data(),ol.data(),orr.data(),n);
int settled=0;for(int i=0;i<n;++i)if(std::abs(ol[i]/l[i]-expected)<1e-12){settled=i+1;break;}
double balanceError=0;for(double b : {-1.,-.5,0.,.5,1.}){p.reset({0,b,false});p.process(l.data(),r.data(),ol.data(),orr.data(),n);const double el=.2*(b>0?1-b:1),er=-.1*(b<0?1+b:1);for(int i=0;i<n;++i)balanceError=std::max({balanceError,std::abs(ol[i]-el),std::abs(orr[i]-er)});}
return{rate,err,chan,transparent,settled,balanceError};}
int main(int argc,char**argv){if(argc!=2){std::fprintf(stderr,"usage: StudioHubStereoUtilityMeasurements <output.json>\n");return 2;}Result r[]{measure(44100),measure(48000),measure(96000)};std::filesystem::path path(argv[1]);if(path.has_parent_path()){std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec){std::fprintf(stderr,"cannot create measurement output directory\n");return 2;}}std::ofstream out(path,std::ios::binary);if(!out){std::fprintf(stderr,"cannot open measurement output path\n");return 2;}out<<"{\n  \"kind\": \"source-dsp-measurements\",\n  \"plugin\": \"StudioHub Stereo Utility\",\n  \"method\": \"deterministic source-DSP render; not a VST3 host render\",\n  \"metrics\": [\n";for(int i=0;i<3;++i){auto&x=r[i];out<<"    {\"sample_rate_hz\": "<<x.rate<<", \"gain_error_db\": "<<x.gainErrorDb<<", \"channel_integrity_max_ratio_error\": "<<x.channelError<<", \"bypass_bit_transparent\": "<<(x.bypassTransparent?"true":"false")<<", \"smoothing_samples\": "<<x.smoothingSamples<<", \"balance_max_absolute_error\": "<<x.balanceError<<"}"<<(i<2?",":"")<<"\n";}out<<"  ]\n}\n";if(!out){std::fprintf(stderr,"failed writing measurement report\n");return 2;}return 0;}
