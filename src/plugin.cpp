#include "stats_dock.hpp"
#include "version.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <util/bmem.h>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("OTMOPOZOK")

namespace {
QPointer<stats::StatsDock> panel;
bool registered=false,exiting=false;

QString statePath(){
    char* raw=obs_module_config_path("dock-visibility.ini");
    if(!raw)return {};
    const QString path=QString::fromUtf8(raw);bfree(raw);
    QDir().mkpath(QFileInfo(path).absolutePath());
    return path;
}

void saveVisibility(){
    auto* window=static_cast<QMainWindow*>(obs_frontend_get_main_window());
    auto* dock=window?window->findChild<QDockWidget*>("stream-stats-dock"):nullptr;
    const QString path=statePath();
    if(!dock||path.isEmpty())return;
    QSettings saved(path,QSettings::IniFormat);
    saved.setValue("visible",dock->toggleViewAction()->isChecked());saved.sync();
}

void installDock(){
    if(exiting||panel)return;
    auto* window=static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if(!window)return;
    auto* created=new stats::StatsDock;
    if(!obs_frontend_add_dock_by_id("stream-stats-dock","Stream Stats",created)){
        delete created;blog(LOG_ERROR,"[Stream Stats] Could not register dock");return;
    }
    panel=created;
    auto* dock=window->findChild<QDockWidget*>("stream-stats-dock");
    if(!dock)return;
    const QString path=statePath();
    QTimer::singleShot(0,dock,[window,dock,path]{
        if(path.isEmpty())return;
        QSettings saved(path,QSettings::IniFormat);
        if(!saved.contains("visible")){
            dock->setFloating(false);
            window->addDockWidget(Qt::BottomDockWidgetArea,dock);
            dock->resize(720,260);
        }
        dock->setVisible(saved.value("visible",true).toBool());
        QObject::connect(dock->toggleViewAction(),&QAction::toggled,dock,[path,window](bool visible){
            if(!window->isVisible()||window->isMinimized())return;
            QSettings state(path,QSettings::IniFormat);state.setValue("visible",visible);state.sync();
        });
    });
}

void onFrontend(obs_frontend_event event,void*){
    if(exiting)return;
    if(event==OBS_FRONTEND_EVENT_FINISHED_LOADING)installDock();
    if(event==OBS_FRONTEND_EVENT_EXIT){saveVisibility();exiting=true;if(panel)panel->shutdown();}
}
} // namespace

MODULE_EXPORT const char* obs_module_name(void){return "Stream Stats";}
MODULE_EXPORT const char* obs_module_description(void){return "Native OBS system, stream and recording statistics";}

bool obs_module_load(void){
    exiting=false;obs_frontend_add_event_callback(onFrontend,nullptr);registered=true;
    blog(LOG_INFO,"[Stream Stats] Version %s loaded",STREAM_STATS_VERSION);
    return true;
}
void obs_module_unload(void){
    exiting=true;
    if(registered){obs_frontend_remove_event_callback(onFrontend,nullptr);registered=false;}
    if(panel){panel->shutdown();obs_frontend_remove_dock("stream-stats-dock");panel=nullptr;}
}
