#include "../src/dock.hpp"
#include <QApplication>
#include <QPushButton>
#include <QCheckBox>
#include <QDir>
#include <QThread>
#include <QFont>
#include <QScrollArea>
#include <QScrollBar>
#include <iostream>
struct DockPreview {
    static bool live(const StreamDock& panel){return panel.broadcasting_;}
    static quint64 session(const StreamDock& panel){return panel.generation_;}
    static void countdown(StreamDock& panel) {
        panel.session_.delay=60;panel.session_.resultSeconds=45;
        panel.session_.telegram=true;panel.session_.discord=true;
        panel.elapsed_.start();panel.phase_=StreamDock::Phase::Counting;panel.rebuild();
    }
    static void result(StreamDock& panel) {
        panel.telegram_={Outcome::Sent,QString("Confirmed"),1};
        panel.discord_={Outcome::Sent,QString("Confirmed"),1};
        panel.phase_=StreamDock::Phase::Result;
        panel.resultTimer_.start(45000);panel.rebuild();
    }
};
static void pump() {
    for(int i=0;i<25;++i){QApplication::processEvents();QThread::msleep(2);}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setFont(QFont("Segoe UI",9));
    const QString output=argc>1?QString::fromLocal8Bit(argv[1]):QString("ui-previews");
    QDir().mkpath(output);
    const int width=argc>2?QString::fromLocal8Bit(argv[2]).toInt():290;
    const int height=argc>3?QString::fromLocal8Bit(argv[3]).toInt():900;
    StreamDock panel;panel.resize(width,height);panel.show();pump();
    auto click=[&](const QString& text) {
        for(auto* button:panel.findChildren<QPushButton*>())
            if(button->text().contains(text) && button->isEnabled()){button->click();pump();return true;}
        std::cerr<<"Missing UI navigation control\n";return false;
    };
    auto save=[&](const QString& name){pump();return panel.grab().save(output+"/"+name+".png");};
    if(!save("01-home") || !click(QString::fromUtf8("Настройки")) || !save("02-connections"))return 1;
    if(!click(QString::fromUtf8("Сообщение")) || !save("03-message"))return 1;
    if(!click(QString::fromUtf8("Интерфейс")) || !click(QString("English")))return 1;
    if(!click(QString::fromUtf8("Русский")) || !save("04-interface"))return 1;
    // Exercise the input-event rebuild path that previously crashed OBS.
    for(int i=0;i<2;++i) {
        bool found=false;
        for(auto* toggle:panel.findChildren<QCheckBox*>())
            if(toggle->text().contains(QString::fromUtf8("Компактный"))){toggle->click();found=true;break;}
        pump();if(!found)return 1;
    }
    if(!click(QString::fromUtf8("Донаты")) || !save("05-donations"))return 1;
    if(!click(QString::fromUtf8("Связь")) || !save("06-connections-return"))return 1;
    if(!click(QString::fromUtf8("Донаты")))return 1;
    for(auto* scroll:panel.findChildren<QScrollArea*>())scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    if(!save("07-donations-bottom"))return 1;
    DockPreview::countdown(panel);if(!save("08-countdown"))return 1;
    DockPreview::result(panel);if(!save("09-result"))return 1;
    streamPreviewOutputs={{false,true}}; // Recording must not announce.
    panel.refreshStreamingState();pump();if(DockPreview::live(panel))return 2;
    const auto before=DockPreview::session(panel);
    streamPreviewOutputs.push_back({true,true}); // First Aitum destination.
    panel.refreshStreamingState();pump();
    if(!DockPreview::live(panel)||DockPreview::session(panel)!=before+1)return 3;
    streamPreviewOutputs.push_back({true,true}); // Second Aitum destination.
    panel.refreshStreamingState();pump();
    if(DockPreview::session(panel)!=before+1)return 4;
    streamPreviewOutputs[1].active=false;
    panel.refreshStreamingState();pump();if(!DockPreview::live(panel))return 5;
    streamPreviewOutputs[2].active=false;
    panel.refreshStreamingState();pump();if(DockPreview::live(panel))return 6;
    streamPreviewOutputs[1].active=true;
    panel.refreshStreamingState();pump();
    if(!DockPreview::live(panel)||DockPreview::session(panel)!=before+2)return 7;
    streamPreviewOutputs.clear();panel.refreshStreamingState();pump();
    std::cout<<"Multistream aggregate start, deduplication, last-stop and restart tests passed\\n";
    panel.shutdown();
    std::cout<<"Dock navigation, language and compact-mode smoke tests passed\n";
}
