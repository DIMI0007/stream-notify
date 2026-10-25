#pragma once
#include <atomic>
#include <algorithm>
#include "settings.hpp"
#include <cstdint>
#include <thread>
#include <QString>
#include <QByteArray>
#include <QSet>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QVariant>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sn {
// All counters belong to this DLL. No process-wide CPU/RAM figures are used.
struct MeterTotals {uint64_t cpuNs=0,tx=0,rx=0,scopes=0;};
class Meter {
public:
    static Meter& instance(){static Meter value;return value;}
    MeterTotals totals() const {return {cpu_.load(),tx_.load(),rx_.load(),scopes_.load()};}
    void cpu(uint64_t ns){cpu_.fetch_add(ns);scopes_.fetch_add(1);}
    void sent(uint64_t n){tx_.fetch_add(n);}
    void received(uint64_t n){rx_.fetch_add(n);}
    static bool threadTime(uint64_t& ns) {
#ifdef Q_OS_WIN
        FILETIME created,ended,kernel,user;
        if(!GetThreadTimes(GetCurrentThread(),&created,&ended,&kernel,&user))return false;
        ULARGE_INTEGER k,u;k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
        u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
        ns=(k.QuadPart+u.QuadPart)*100;return true;
#else
        Q_UNUSED(ns);return false;
#endif
    }
private:
    std::atomic<uint64_t> cpu_{0},tx_{0},rx_{0},scopes_{0};
};
class CpuScope {
public:
    CpuScope():outer_(depth_++==0){valid_=outer_ && Meter::threadTime(start_);}
    ~CpuScope(){
        --depth_;uint64_t end=0;
        if(valid_ && Meter::threadTime(end) && end>=start_)Meter::instance().cpu(end-start_);
    }
    CpuScope(const CpuScope&)=delete;CpuScope& operator=(const CpuScope&)=delete;
private:
    inline static thread_local unsigned depth_=0;
    bool outer_=false,valid_=false;uint64_t start_=0;
};
// Account only actual retained text/byte storage, deduplicating Qt shared buffers.
// This is deliberately NOT a claimed complete allocator/RAM footprint.
class BufferCensus {
public:
    void add(const QString& value){add(value.constData(),uint64_t(value.capacity())*sizeof(QChar));}
    void add(const QByteArray& value){add(value.constData(),uint64_t(value.capacity()));}
    void add(const void* key,uint64_t bytes){if(bytes && !seen_.contains(key)){seen_.insert(key);bytes_+=bytes;}}
    uint64_t bytes() const{return bytes_;}
private:
    QSet<const void*> seen_;uint64_t bytes_=0;
};

inline void account(BufferCensus& c,const Snapshot& s) {
    c.add(s.token);c.add(s.chat);c.add(s.webhook);c.add(s.message);
}
inline void account(BufferCensus& c,const Settings& s) {
    for(const auto* value:{&s.telegramToken,&s.telegramChat,&s.discordWebhook,&s.youtubeUrl,&s.twitchUrl,
        &s.message,&s.locale,&s.lastTelegram,&s.lastDiscord,&s.lastTime})c.add(*value);
    for(const auto& event:s.events)c.add(event);
    const auto& d=s.donations;
    c.add(d.daToken);c.add(d.dpToken);for(const auto& value:d.templates)c.add(value);
    for(auto it=d.thresholds.begin();it!=d.thresholds.end();++it)c.add(it.key());
}
inline void accountReply(BufferCensus& c,QNetworkReply* reply) {
    if(reply){c.add(reply,uint64_t(std::max<qint64>(0,reply->bytesAvailable())));c.add(reply->property("snUpload").toByteArray());}
}

struct Usage {
    bool ready=false,cpuAvailable=false;double cpuPercent=0,txPerSec=0,rxPerSec=0;
    uint64_t buffers=0,txTotal=0,rxTotal=0;
};
class UsageSampler {
public:
    Usage sample(uint64_t buffers) {
        const auto next=Meter::instance().totals();uint64_t ignored=0;
        Usage result;result.buffers=buffers;result.txTotal=next.tx;result.rxTotal=next.rx;
        result.cpuAvailable=Meter::threadTime(ignored);
        if(clock_.isValid()) {
            const auto ns=clock_.nsecsElapsed();
            if(ns>0) {
                result.ready=true;
                const auto cores=std::max(1u,std::thread::hardware_concurrency());
                result.cpuPercent=100.0*double(next.cpuNs-previous_.cpuNs)/double(ns)/cores;
                result.txPerSec=double(next.tx-previous_.tx)*1e9/double(ns);
                result.rxPerSec=double(next.rx-previous_.rx)*1e9/double(ns);
            }
        }
        previous_=next;clock_.restart();return result;
    }
private:MeterTotals previous_;QElapsedTimer clock_;
};
inline QNetworkReply* watchReply(QNetworkReply* reply,const QByteArray& upload={}) {
    if(!upload.isEmpty())reply->setProperty("snUpload",upload);
    struct Progress {qint64 up=0,down=0;};
    auto progress=std::make_shared<Progress>();
    QObject::connect(reply,&QNetworkReply::uploadProgress,reply,[progress](qint64 bytes,qint64){
        CpuScope scope;
        if(bytes>progress->up)Meter::instance().sent(uint64_t(bytes-progress->up));
        progress->up=std::max(progress->up,bytes);
    });
    QObject::connect(reply,&QNetworkReply::downloadProgress,reply,[progress](qint64 bytes,qint64){
        CpuScope scope;
        if(bytes>progress->down)Meter::instance().received(uint64_t(bytes-progress->down));
        progress->down=std::max(progress->down,bytes);
    });
    return reply;
}
}

