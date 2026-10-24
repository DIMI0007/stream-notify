#pragma once
#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/bmem.h>
#include <QDir>
#include <QByteArray>
#include <cstdlib>
#include <cstring>
#include <vector>
// Isolate the real Qt dock from OBS host globals during UI smoke testing.
inline const char* streamPreviewLocale(){return "ru-RU";}
inline bool streamPreviewBuiltinLive=false;
struct StreamPreviewOutput {
    bool network;bool active;
    bool reconnecting=false;uint64_t bytes=0,frames=0,dropped=0;double targetKbps=6000;
    QString name,platform;
};
inline std::vector<StreamPreviewOutput> streamPreviewOutputs;
inline bool streamPreviewLive(){return streamPreviewBuiltinLive;}
inline void streamPreviewEnum(bool (*callback)(void*,obs_output_t*),void* context) {
    for(auto& output:streamPreviewOutputs)
        if(!callback(context,reinterpret_cast<obs_output_t*>(&output)))break;
}
inline uint32_t streamPreviewFlags(const obs_output_t* output) {
    return reinterpret_cast<const StreamPreviewOutput*>(output)->network?OBS_OUTPUT_SERVICE:0;
}
inline bool streamPreviewActive(const obs_output_t* output) {
    return reinterpret_cast<const StreamPreviewOutput*>(output)->active;
}
inline char* streamPreviewConfig(const char* name) {
    const QString root=qEnvironmentVariable("STREAM_NOTIFY_PREVIEW_DIR",QDir::tempPath()+"/stream-notify-ui-preview");
    QDir().mkpath(root);
    const QByteArray path=(root+"/"+QString::fromUtf8(name)).toUtf8();
    auto* copy=static_cast<char*>(std::malloc(size_t(path.size())+1));
    if(copy)std::memcpy(copy,path.constData(),size_t(path.size())+1);
    return copy;
}
inline void streamPreviewFree(void* pointer){std::free(pointer);}
#define obs_get_locale streamPreviewLocale
#define obs_frontend_streaming_active streamPreviewLive
#undef obs_module_config_path
#define obs_module_config_path(name) streamPreviewConfig(name)
#define bfree streamPreviewFree

#define obs_enum_outputs streamPreviewEnum
#define obs_output_get_flags streamPreviewFlags
#define obs_output_active streamPreviewActive
