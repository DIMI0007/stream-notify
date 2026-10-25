#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QTimer>
#include <QPointer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QSslSocket>
#include <util/bmem.h>
#include "dock.hpp"
#include "version.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Stream Notify Contributors")

namespace {
QPointer<StreamDock> g_panel;
bool g_registered = false;
bool g_exiting = false;
QString g_qtPluginPath;

void installDock()
{
    if (g_exiting || g_panel) return;
    auto* window = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if(!window) return;
    auto* panel = new StreamDock;
    if (!obs_frontend_add_dock_by_id("stream-notify-dock", "Stream Notify", panel)) {
        delete panel;
        blog(LOG_ERROR,"[Stream Notify] Could not register dock");
        return;
    }
    g_panel = panel;
    // OBS manages layout persistence. Force visibility only once upon fresh install.
    auto* dock = window->findChild<QDockWidget*>("stream-notify-dock");
    if(dock) {
        char* path = obs_module_config_path("dock-initialized.flag");
        if(path) {
            QFile flag(QString::fromUtf8(path));
            bfree(path);
            if(!flag.exists()) {
                if(QDir().mkpath(QFileInfo(flag).absolutePath()) && flag.open(QIODevice::WriteOnly)) {
                    flag.write("1"); flag.close();
                    dock->setFloating(false);
                    window->addDockWidget(Qt::RightDockWidgetArea,dock);
                    dock->show();
                }
            }
        }
    }
}
void onFrontend(enum obs_frontend_event event, void*)
{
    if(g_exiting) return;
    switch(event) {
        case OBS_FRONTEND_EVENT_FINISHED_LOADING: installDock(); break;
        case OBS_FRONTEND_EVENT_STREAMING_STARTED:
            if(g_panel) g_panel->refreshStreamingState();
            break;
        case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
        case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
            if(g_panel) g_panel->refreshStreamingState();
            break;
        case OBS_FRONTEND_EVENT_EXIT:
            g_exiting = true;
            if(g_panel) g_panel->shutdown();
            break;
        default: break;
    }
}
}

MODULE_EXPORT const char* obs_module_name(void) {return "Stream Notify";}
MODULE_EXPORT const char* obs_module_description(void)
{return "Telegram and Discord stream start notifications with a native OBS dock";}

bool obs_module_load(void)
{
    g_exiting = false;
    char* tlsDirectory=obs_module_file("qt-plugins");
    if(tlsDirectory) {
        g_qtPluginPath=QString::fromUtf8(tlsDirectory);
        bfree(tlsDirectory);
        QCoreApplication::addLibraryPath(g_qtPluginPath);
    }
    if(!QSslSocket::supportsSsl() && QSslSocket::availableBackends().contains("schannel"))
        QSslSocket::setActiveBackend("schannel");
    blog(LOG_INFO,"[Stream Notify] HTTPS support: %s; TLS backend: %s",
        QSslSocket::supportsSsl()?"available":"unavailable",
        QSslSocket::activeBackend().toUtf8().constData());
    obs_frontend_add_event_callback(onFrontend, nullptr);
    g_registered = true;
    blog(LOG_INFO, "[Stream Notify] Version %s (%s) loaded; waiting for frontend initialization",STREAM_NOTIFY_VERSION,STREAM_NOTIFY_BUILD);
    return true;
}
void obs_module_unload(void)
{
    g_exiting = true;
    if(g_registered) {
        obs_frontend_remove_event_callback(onFrontend, nullptr);
        g_registered = false;
    }
    if(g_panel) {
        g_panel->shutdown();
        obs_frontend_remove_dock("stream-notify-dock");
        g_panel = nullptr;
    }
    if(!g_qtPluginPath.isEmpty()) {
        QCoreApplication::removeLibraryPath(g_qtPluginPath);
        g_qtPluginPath.clear();
    }
}

