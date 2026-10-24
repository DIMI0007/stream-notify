#pragma once
#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/bmem.h>
#include <QDir>
#include <QByteArray>
#include <cstdlib>
#include <cstring>
// Isolate the real Qt dock from OBS host globals during UI smoke testing.
inline const char* streamPreviewLocale(){return "ru-RU";}
inline bool streamPreviewLive(){return false;}
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
