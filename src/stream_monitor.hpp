#pragma once
#include "output_health.hpp"
#include "resources.hpp"
#include <obs.h>
#include <QMap>
#include <QVector>
#include <QUrl>
#include <QSet>

namespace sn {
struct OutputView {QString key,platform,name;OutputStats stats;};
inline QString platformFor(const QString& name,const QString& service,const QString& server) {
    const auto host=QUrl(server).host().toLower();
    const auto title=(service+" "+name).toLower();
    if(host=="twitch.tv" || host.endsWith(".twitch.tv") || title.contains("twitch"))return "Twitch";
    if(host=="youtube.com" || host.endsWith(".youtube.com") || host=="youtube" ||
       title.contains("youtube"))return "YouTube";
    return "RTMP";
}
class StreamMonitor {
public:
    StreamMonitor()=default;
    StreamMonitor(const StreamMonitor&)=delete;StreamMonitor& operator=(const StreamMonitor&)=delete;
    ~StreamMonitor(){clear();}
    void clear(){
#ifndef STREAM_NOTIFY_UI_PREVIEW
        for(auto it=entries_.begin();it!=entries_.end();++it)
            obs_weak_output_release(reinterpret_cast<obs_weak_output_t*>(it.key()));
#endif
        entries_.clear();seen_.clear();views_.clear();
    }
    QVector<OutputView> sample(int64_t now) {
        CpuScope scope;now_=now;seen_.clear();views_.clear();
#ifdef STREAM_NOTIFY_UI_PREVIEW
        int index=0;
        for(const auto& item:streamPreviewOutputs) {
            ++index;if(!item.network)continue;
            const auto key=quintptr(index);
            seen_.insert(key);
            auto& entry=entries_[key];
            const OutputSample sample{item.active,item.reconnecting,item.bytes,item.frames,item.dropped,item.targetKbps};
            const auto name=item.name.isEmpty()?QString("Preview output %1").arg(index):item.name;
            views_.push_back({QString::number(key),platformFor(name,item.platform,QString()),name,entry.health.update(sample,now)});
        }
#else
        obs_enum_outputs([](void* opaque,obs_output_t* output){
            return static_cast<StreamMonitor*>(opaque)->observe(output);
        },this);
#endif
        for(auto it=entries_.begin();it!=entries_.end();) {
            if(!seen_.contains(it.key())){
#ifndef STREAM_NOTIFY_UI_PREVIEW
                obs_weak_output_release(reinterpret_cast<obs_weak_output_t*>(it.key()));
#endif
                it=entries_.erase(it);
            } else ++it;
        }
        return views_;
    }
    QVector<OutputView> views() const{return views_;}
    void account(BufferCensus& census)const{
        for(const auto& v:views_){census.add(v.key);census.add(v.name);census.add(v.platform);}
    }
private:
    struct Entry {OutputHealth health;};
#ifndef STREAM_NOTIFY_UI_PREVIEW
    bool observe(obs_output_t* output) {
        if(!(obs_output_get_flags(output)&OBS_OUTPUT_SERVICE))return true;
        auto* weak=obs_output_get_weak_output(output);if(!weak)return true;
        const auto key=reinterpret_cast<quintptr>(weak);
        if(entries_.contains(key))obs_weak_output_release(weak);
        else entries_.insert(key,Entry{});
        seen_.insert(key);
        QString name=QString::fromUtf8(obs_output_get_name(output));
        QString service,server;
        if(auto* provider=obs_output_get_service(output)) {
            if(auto* settings=obs_service_get_settings(provider)) {
                // Read only the public service name/server, never stream keys.
                service=QString::fromUtf8(obs_data_get_string(settings,"service"));
                server=QString::fromUtf8(obs_data_get_string(settings,"server"));
                obs_data_release(settings);
            }
        }
        double target=0;
        if(auto* encoder=obs_output_get_video_encoder(output)) {
            if(auto* settings=obs_encoder_get_settings(encoder)) {
                target=double(obs_data_get_int(settings,"bitrate"));obs_data_release(settings);
            }
        }
        OutputSample sample;
        sample.active=obs_output_active(output);sample.reconnecting=obs_output_reconnecting(output);
        sample.bytes=obs_output_get_total_bytes(output);sample.frames=uint64_t(std::max(0,obs_output_get_total_frames(output)));
        sample.dropped=uint64_t(std::max(0,obs_output_get_frames_dropped(output)));sample.targetKbps=target;
        views_.push_back({QString::number(key),platformFor(name,service,server),name,entries_[key].health.update(sample,now_)});
        return true;
    }
#endif
    QMap<quintptr,Entry> entries_;QSet<quintptr> seen_;QVector<OutputView> views_;int64_t now_=0;
};
}
