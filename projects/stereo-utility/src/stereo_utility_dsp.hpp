#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
namespace studiohub::stereo_utility {
inline constexpr double kMinGainDb=-24, kMaxGainDb=24, kMinBalance=-1, kMaxBalance=1;
inline double gainDb(double v) noexcept { return std::isfinite(v)?std::clamp(v,kMinGainDb,kMaxGainDb):0.; }
inline double balance(double v) noexcept { return std::isfinite(v)?std::clamp(v,kMinBalance,kMaxBalance):0.; }
inline double normalized(double v) noexcept { return std::isfinite(v)?std::clamp(v,0.,1.):0.; }
struct State { double gainDb{0}, balance{0}; bool bypass{false}; };
class Ramp { double x_{}, target_{}, step_{}; std::uint32_t left_{}; public:
 void reset(double x) noexcept {x_=target_=x;step_=0;left_=0;}
 void set(double x,std::uint32_t n) noexcept {if(x==target_)return;target_=x;left_=n;if(!n){x_=x;step_=0;}else step_=(x_-target_)*-1.0/n;}
 double next() noexcept {double v=x_;if(left_&&!--left_)x_=target_;else if(left_)x_+=step_;return v;}
};
class Processor {
 State state_{}; Ramp gain_,left_,right_,bypass_; double rate_{44100}; std::uint32_t ramp_{441};
 static double bal(double b,bool l) noexcept {return b<0&&!l?1+b:b>0&&l?1-b:1;}
 template<class T> static double input(T x) noexcept {double v=static_cast<double>(x);return std::isfinite(v)?v:0.;}
 template<class T> static T output(double x) noexcept {if(!std::isfinite(x))return T(0);const double m=std::numeric_limits<T>::max();return static_cast<T>(std::clamp(x,-m,m));}
public:
 Processor() noexcept { reset(); }
 void setSampleRate(double r) noexcept {rate_=std::isfinite(r)&&r>0?r:44100;ramp_=static_cast<std::uint32_t>(std::max(1.,std::round(rate_*.010)));}
 void reset(State s={}) noexcept {s.gainDb=gainDb(s.gainDb);s.balance=balance(s.balance);state_=s;gain_.reset(std::pow(10.,s.gainDb/20));left_.reset(bal(s.balance,true));right_.reset(bal(s.balance,false));bypass_.reset(s.bypass?1:0);}
 void setGain(double v) noexcept {state_.gainDb=gainDb(v);gain_.set(std::pow(10.,state_.gainDb/20),ramp_);}
 void setBalance(double v) noexcept {state_.balance=balance(v);left_.set(bal(state_.balance,true),ramp_);right_.set(bal(state_.balance,false),ramp_);}
 void setBypass(bool v) noexcept {state_.bypass=v;bypass_.set(v?1:0,ramp_);}
 State target()const noexcept{return state_;} double sampleRate()const noexcept{return rate_;} std::uint32_t rampSamples()const noexcept{return ramp_;}
 template<class T> void process(const T* il,const T* ir,T* ol,T* or_,std::size_t n) noexcept {for(std::size_t i=0;i<n;++i){double l=input(il[i]),r=input(ir[i]),g=gain_.next(),gl=g*left_.next(),gr=g*right_.next(),wet=1-bypass_.next();ol[i]=output<T>(l*(1+(gl-1)*wet));or_[i]=output<T>(r*(1+(gr-1)*wet));}}
};
}
