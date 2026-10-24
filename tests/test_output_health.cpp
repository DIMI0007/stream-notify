#include "../src/output_health.hpp"
#include <cassert>
#include <iostream>
int main(){
    sn::OutputHealth twitch,youtube;
    sn::OutputSample good;good.active=true;good.targetKbps=6000;
    auto bad=good;sn::OutputStats a,b;
    for(int64_t time=0;time<=30000;time+=2000) {
        if(time){good.bytes+=1500000;good.frames+=120;bad.bytes+=time>=12000?500000:1500000;bad.frames+=120;}
        a=twitch.update(bad,time);b=youtube.update(good,time);
    }
    assert(a.problemNow && a.recentProblem && !b.recentProblem && b.kbps==6000);
    for(int64_t time=32000;time<=40000;time+=2000){bad.bytes+=1500000;bad.frames+=120;a=twitch.update(bad,time);}
    assert(!a.problemNow && a.recentProblem); // Recovery retains the five-minute warning.
    a=twitch.update(bad,340001);assert(!a.recentProblem); // expiry, not permanent red.
    auto dropped=good;dropped.dropped=12;
    b=youtube.update(dropped,32000);assert(b.recentProblem && b.dropped==12 && b.dropPercent>0);
    dropped.reconnecting=true;b=youtube.update(dropped,34000);assert(b.problemNow && b.reconnecting);
    dropped.reconnecting=false;dropped.active=false;b=youtube.update(dropped,36000);assert(!b.active);
    dropped.active=true;dropped.bytes=0;dropped.frames=0;dropped.dropped=0;
    b=youtube.update(dropped,38000);assert(!b.recentProblem && !b.rateReady && b.bytes==0);
    sn::OutputHealth unknown;sn::OutputSample zero;zero.active=true;
    unknown.update(zero,0);b=unknown.update(zero,20000);assert(!b.hasCounters && !b.recentProblem);
    sn::OutputHealth reset;reset.update(good,0);
    good.bytes+=1500000;good.frames+=120;reset.update(good,12000);
    good.bytes=100;good.frames=1;a=reset.update(good,14000);
    assert(a.recentProblem && a.bytes>100); // Counter reset/reconnect doesn't subtract transferred data.
    assert(reset.update(good,14000).kbps==a.kbps); // Duplicate timestamp is harmless.
    std::cout<<"Independent outputs, sustained drops, recovery/expiry, loss/reconnect, restart and unknown counters passed\n";
}
