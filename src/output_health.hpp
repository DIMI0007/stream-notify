#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
#include <array>

namespace sn {
struct OutputSample {
    bool active=false,reconnecting=false;
    uint64_t bytes=0,frames=0,dropped=0;
    double targetKbps=0;
};
struct OutputStats {
    bool active=false,reconnecting=false,rateReady=false,problemNow=false,recentProblem=false,hasCounters=false;
    double kbps=0,dropPercent=0;
    uint64_t bytes=0,dropped=0;
    int64_t ageMs=0,problemAgeMs=0;
    std::array<float,180> bitrateHistory{};
    unsigned historyCount=0,historyNext=0;
};
class OutputHealth {
public:
    OutputStats update(const OutputSample& sample,int64_t now) {
        if(!sample.active) {
            live_=false;out_.active=false;out_.reconnecting=false;
            out_.rateReady=false;out_.kbps=0;out_.problemNow=false;
            ageProblem(now);return out_;
        }
        if(!live_) {
            live_=true;started_=last_=now;previous_=sample;
            out_={};out_.active=true;out_.bytes=sample.bytes;out_.dropped=sample.dropped;
            frames_=sample.frames;recent_.reset();lowSince_.reset();rates_.clear();baseline_=0;historyLast_=0;
            if(sample.reconnecting)recent_=now;
        } else if(now>last_) {
            const bool reset=sample.bytes<previous_.bytes || sample.frames<previous_.frames || sample.dropped<previous_.dropped;
            const uint64_t deltaBytes=sample.bytes>=previous_.bytes?sample.bytes-previous_.bytes:sample.bytes;
            const uint64_t deltaFrames=sample.frames>=previous_.frames?sample.frames-previous_.frames:sample.frames;
            const uint64_t deltaDropped=sample.dropped>=previous_.dropped?sample.dropped-previous_.dropped:sample.dropped;
            out_.bytes+=deltaBytes;out_.dropped+=deltaDropped;frames_+=deltaFrames;
            if(reset) {rates_.clear();out_.rateReady=false;lowSince_.reset();if(now-started_>=10000)recent_=now;}
            else {
                rates_.push_back({deltaBytes,now-last_});if(rates_.size()>3)rates_.pop_front();
                uint64_t bytes=0;int64_t elapsed=0;
                for(const auto& r:rates_){bytes+=r.bytes;elapsed+=r.ms;}
                out_.kbps=elapsed>0?double(bytes)*8.0/double(elapsed):0;
                out_.rateReady=true;
                if(historyLast_==0||now-historyLast_>=10000){
                    out_.bitrateHistory[out_.historyNext]=float(out_.kbps);
                    out_.historyNext=(out_.historyNext+1)%out_.bitrateHistory.size();
                    out_.historyCount=std::min<unsigned>(out_.historyCount+1,out_.bitrateHistory.size());
                    historyLast_=now;
                }
            }
            const double reference=sample.targetKbps>0?sample.targetKbps:baseline_;
            const bool warmed=now-started_>=10000;
            const bool low=warmed && out_.hasCounters && out_.rateReady && reference>0 && out_.kbps<reference*0.70;
            if(low) {if(!lowSince_)lowSince_=now;} else lowSince_.reset();
            const bool sustained=lowSince_ && now-*lowSince_>=6000;
            out_.problemNow=sample.reconnecting || deltaDropped>0 || sustained;
            if(out_.problemNow)recent_=now;
            if(out_.rateReady && !low && !sample.reconnecting && out_.kbps>0)
                baseline_=baseline_==0?out_.kbps:baseline_*0.9+out_.kbps*0.1;
            previous_=sample;last_=now;
        }
        out_.active=true;out_.hasCounters=out_.hasCounters || sample.bytes>0 || sample.frames>0;
        out_.reconnecting=sample.reconnecting;out_.ageMs=now-started_;
        if(sample.reconnecting){out_.problemNow=true;recent_=now;}
        out_.dropPercent=frames_?std::min(100.0,100.0*double(out_.dropped)/double(frames_)):0;
        ageProblem(now);return out_;
    }
private:
    void ageProblem(int64_t now) {
        out_.recentProblem=recent_ && now-*recent_<300000;
        out_.problemAgeMs=recent_?std::max<int64_t>(0,now-*recent_):0;
    }
    struct Rate {uint64_t bytes;int64_t ms;};
    bool live_=false;
    int64_t started_=0,last_=0,historyLast_=0;
    uint64_t frames_=0;
    double baseline_=0;
    std::optional<int64_t> recent_,lowSince_;
    OutputSample previous_;
    OutputStats out_;
    std::deque<Rate> rates_;
};
}

