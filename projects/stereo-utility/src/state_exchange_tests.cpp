#include "state_exchange.hpp"
#include <thread>
#include <atomic>
#include <cstdio>
using namespace studiohub::stereo_utility;
int main(){StateExchange exchange;std::atomic<bool> done{false};std::atomic<int> failures{0};
 std::thread writer([&]{for(int i=0;i<20000;++i){const State s=i%2?State{24,1,true}:State{-24,-1,false};exchange.publish(s);auto saved=exchange.requested();if(saved.gainDb!=s.gainDb||saved.balance!=s.balance||saved.bypass!=s.bypass)++failures;}done=true;});
 std::thread audio([&]{while(!done){State pending;std::uint64_t version=0;if(exchange.pending(pending,version)){if(!((pending.gainDb==24&&pending.balance==1&&pending.bypass)||(pending.gainDb==-24&&pending.balance==-1&&!pending.bypass)))++failures;exchange.acknowledge(pending,version);}}});
 writer.join();audio.join();if(failures)return 1;puts("Concurrent state publication, coherent mailbox and audio-side consumption passed");}
