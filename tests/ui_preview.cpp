#include "../src/dock.hpp"
#include <QApplication>
#include <QPushButton>
#include <QCheckBox>
#include <QDir>
#include <QThread>
#include <QFont>
#include <iostream>
static void pump() {
    for(int i=0;i<25;++i){QApplication::processEvents();QThread::msleep(2);}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setFont(QFont("Segoe UI",9));
    const QString output=argc>1?QString::fromLocal8Bit(argv[1]):QString("ui-previews");
    QDir().mkpath(output);
    StreamDock panel;panel.resize(290,900);panel.show();pump();
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
    panel.shutdown();
    std::cout<<"Dock navigation, language and compact-mode smoke tests passed\n";
}
