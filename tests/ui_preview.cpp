#include "../src/dock.hpp"
#include <QApplication>
#include <QPushButton>
#include <QCheckBox>
#include <QDir>
#include <QThread>
#include <QFont>
#include <QScrollArea>
#include <QScrollBar>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QLabel>
#include <QPixmap>
#include <QLineEdit>
#include <iostream>
struct DockPreview {
    static QVector<sn::OutputView> outputViews(const StreamDock& panel){return panel.monitor_.views();}
    static bool live(const StreamDock& panel){return panel.broadcasting_;}
    static quint64 session(const StreamDock& panel){return panel.generation_;}
    static void monitorDemo(StreamDock& panel) {
        panel.telemetryTimer_.stop();panel.streamStateTimer_.stop();
        streamPreviewOutputs.clear();
        StreamPreviewOutput youtube{true,true};youtube.name="Aitum YouTube";youtube.platform="YouTube";
        StreamPreviewOutput twitch{true,true};twitch.name="Aitum Twitch";twitch.platform="Twitch";
        streamPreviewOutputs={youtube,twitch};
        for(int64_t ms=0;ms<=30000;ms+=2000) {
            if(ms) {
                streamPreviewOutputs[0].bytes+=1500000;streamPreviewOutputs[0].frames+=120;
                streamPreviewOutputs[1].bytes+=ms>=12000?650000:1500000;streamPreviewOutputs[1].frames+=120;
                if(ms==20000)streamPreviewOutputs[1].dropped=12;
            }
            panel.monitor_.sample(ms);
        }
        panel.broadcasting_=true;panel.page_=StreamDock::Page::Home;panel.phase_=StreamDock::Phase::Idle;
        panel.rebuild();
    }
    static void stopMonitorDemo(StreamDock& panel) {
        streamPreviewOutputs.clear();panel.monitor_.clear();panel.broadcasting_=false;
        panel.telemetryTimer_.start();panel.streamStateTimer_.start();panel.rebuild();
    }
    static Settings demoConnections(StreamDock& panel) {
        const Settings original=panel.store_.current;
        auto& s=panel.store_.current;
        s.telegramEnabled=true;s.discordEnabled=true;
        s.telegramToken="0:PREVIEW_ONLY_NOT_A_REAL_TOKEN";
        s.telegramChat="@preview_channel";
        s.discordWebhook="https://discord.com/api/webhooks/0/PREVIEW_ONLY_NOT_A_REAL_WEBHOOK";
        s.youtubeUrl="https://www.youtube.com/@example/live";
        s.twitchUrl="https://www.twitch.tv/example";
        panel.page_=StreamDock::Page::Home;panel.phase_=StreamDock::Phase::Idle;
        panel.rebuild();return original;
    }
    static void restoreSettings(StreamDock& panel,const Settings& settings) {
        panel.store_.current=settings;panel.rebuild();
    }
    static bool donationIdleStates(StreamDock& panel) {
        const auto original=panel.store_.current.donations;
        auto& s=panel.store_.current.donations;
        s.enabled=true;s.daEnabled=true;s.dpEnabled=true;s.onlyLive=true;s.legacyApiOptIn=true;
        s.daToken.clear();s.dpToken.clear();
        panel.donation_.setLive(false);panel.donation_.configure();
        const bool missing=panel.donation_.daStatus()=="Not configured" && panel.donation_.dpStatus()=="Not configured";
        // onlyLive blocks external service connections; these are dummy values.
        s.daToken="PREVIEW_ONLY";s.dpToken="PREVIEW_ONLY";panel.donation_.configure();
        const bool waiting=panel.donation_.daStatus()=="Waiting for stream" && panel.donation_.dpStatus()=="Waiting for stream";
        s.enabled=false;panel.donation_.configure();
        const bool disabled=panel.donation_.daStatus()=="Module disabled" && panel.donation_.dpStatus()=="Module disabled";
        panel.store_.current.donations=original;panel.donation_.configure();
        return missing && waiting && disabled;
    }
    static void countdown(StreamDock& panel,int seconds=60) {
        panel.session_.delay=seconds;panel.session_.resultSeconds=45;
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
    for(const char* asset : {":/stream-notify/brands/donationalerts.png",":/stream-notify/brands/donatepay.png"}) {
        if(QPixmap(QString::fromLatin1(asset)).isNull()) {
            std::cerr<<"Missing or invalid embedded service logo: "<<asset<<"\n";return 21;
        }
    }
    std::cout<<"Embedded DonationAlerts and DonatePay artwork decoded successfully\n";
    const QString output=argc>1?QString::fromLocal8Bit(argv[1]):QString("ui-previews");
    QDir().mkpath(output);
    const int width=argc>2?QString::fromLocal8Bit(argv[2]).toInt():290;
    const int height=argc>3?QString::fromLocal8Bit(argv[3]).toInt():900;
    StreamDock panel;panel.resize(width,height);panel.show();pump();
    auto click=[&](const QString& text) {
        for(auto* button:panel.findChildren<QPushButton*>())
            if(button->text().contains(text) && button->isEnabled()){button->click();pump();return true;}
        std::cerr<<"Missing UI navigation control: "<<text.toStdString()<<"\n";return false;
    };
    auto save=[&](const QString& name){
        pump();
        for(auto* label:panel.findChildren<QLabel*>())
            if(label->text().contains(QString::fromUtf8("Сохранено")) || label->text().contains(QString::fromUtf8("Автосохранение")))return false;
        return panel.grab().save(output+"/"+name+".png");
    };
    if(!save("01-home"))return 1;
    // Empty-state actions should open and reveal the correct settings field.
    if(!click(QString::fromUtf8("Добавить")))return 28;
    auto* urlField=panel.findChild<QLineEdit*>("YouTubeUrl");
    if(!urlField)return 29;
    bool revealed=false;
    for(auto* scroll:panel.findChildren<QScrollArea*>())
        if(scroll->widget() && scroll->widget()->isAncestorOf(urlField))
            revealed=scroll->viewport()->rect().contains(urlField->mapTo(scroll->viewport(),urlField->rect().center()));
    if(!revealed)return 30;
    if(!click(QString::fromUtf8("Основное")) || !click(QString::fromUtf8("Настроить")) || !save("02-connections"))return 31;
    if(!panel.findChild<QLineEdit*>("TelegramToken"))return 32;
    if(panel.findChild<QWidget*>("PluginResources") || panel.findChild<QWidget*>("OutputMonitor"))return 42;
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
    const QSize dockSize=panel.size();
    auto donationTab=[&](int section) {
        for(auto* button:panel.findChildren<QPushButton*>()) {
            const QVariant value=button->property("donationSection");
            if(value.isValid() && value.toInt()==section) {button->click();pump();return panel.size()==dockSize;}
        }
        return false;
    };
    if(!donationTab(1)||!save("10-donations-send"))return 10;
    if(!donationTab(2)||!save("11-donations-amounts"))return 11;
    const auto amounts=panel.findChildren<QDoubleSpinBox*>();
    if(amounts.size()!=3)return 12;
    amounts[0]->setValue(99);pump();
    if(!donationTab(3))return 13;
    bool correctPreview=false;
    for(auto* label:panel.findChildren<QLabel*>())
        if(label->text().contains("99 RUB"))correctPreview=true;
    if(!correctPreview)return 14;
    auto* editor=panel.findChild<QTextEdit*>();
    if(!editor)return 15;
    const QString original=editor->toPlainText();
    editor->setPlainText(original+" UI-PERSIST-CHECK");pump();
    if(!donationTab(0)||!donationTab(3))return 16;
    editor=panel.findChild<QTextEdit*>();
    if(!editor||!editor->toPlainText().endsWith(" UI-PERSIST-CHECK"))return 17;
    editor->setPlainText(original);
    if(!donationTab(2))return 18;
    const auto restoredAmounts=panel.findChildren<QDoubleSpinBox*>();
    if(restoredAmounts.isEmpty()||restoredAmounts[0]->value()!=99)return 19;
    restoredAmounts[0]->setValue(100);
    if(!donationTab(3)||!save("12-donations-text"))return 20;
    std::cout<<"Donation subtabs: stable dock size, edited amounts and template persistence passed\\n";
    for(auto* scroll:panel.findChildren<QScrollArea*>())scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    if(!save("07-donations-bottom"))return 1;
    if(!DockPreview::donationIdleStates(panel))return 33;
    const Settings originalSettings=DockPreview::demoConnections(panel);pump();
    QList<QPointer<QTimer>> readyTimers;
    for(auto* timer:panel.findChildren<QTimer*>())
        if(timer->objectName()=="ReadyBlinkTimer")readyTimers.append(timer);
    if(readyTimers.size()!=2)return 34;
    for(const auto& timer:readyTimers)if(!timer || !timer->isActive())return 35;
    if(!save("15-home-ready"))return 1;
    panel.hide();pump();for(const auto& timer:readyTimers)if(timer->isActive())return 36;
    panel.show();pump();for(const auto& timer:readyTimers)if(!timer->isActive())return 37;
    DockPreview::restoreSettings(panel,originalSettings);pump();
    for(const auto& timer:readyTimers)if(timer)return 38;
    std::cout<<"Setup links, donation idle reasons and visible-only ready indicators passed\\n";
    const Settings monitorSettings=DockPreview::demoConnections(panel);pump();
    DockPreview::monitorDemo(panel);pump();
    if(!panel.findChild<QWidget*>("OutputMonitor") || !panel.findChild<QWidget*>("PluginResources"))return 40;
    const auto outputs=DockPreview::outputViews(panel);
    if(outputs.size()!=2 || outputs[0].stats.recentProblem || !outputs[1].stats.recentProblem)return 41;
    if(!save("16-monitoring-home"))return 1;
    for(auto* scroll:panel.findChildren<QScrollArea*>())scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    if(!save("17-monitoring-bottom"))return 1;
    DockPreview::stopMonitorDemo(panel);pump();
    DockPreview::restoreSettings(panel,monitorSettings);pump();
    DockPreview::countdown(panel);if(!save("08-countdown"))return 1;
    QPointer<QTimer> pulse=panel.findChild<QTimer*>("CountdownPulseTimer");
    if(!pulse || pulse->isActive())return 22; // No animation outside the final ten seconds.
    DockPreview::countdown(panel,10);pump();
    pulse=panel.findChild<QTimer*>("CountdownPulseTimer");
    if(!pulse || !pulse->isActive())return 23;
    const QSize pulseSize=panel.size();
    if(!save("13-countdown-last10"))return 1;
    const QImage beforePulse=panel.grab().toImage();
    for(int i=0;i<7;++i)pump();
    const QImage afterPulse=panel.grab().toImage();
    if(beforePulse==afterPulse || panel.size()!=pulseSize)return 24;
    if(!save("14-countdown-pulse"))return 1;
    panel.hide();pump();if(pulse->isActive())return 25;
    panel.show();pump();if(!pulse->isActive())return 26;
    DockPreview::result(panel);if(!save("09-result"))return 1;
    if(pulse)return 27; // The ring and its timer must be destroyed on phase change.
    std::cout<<"Countdown pulse: final-ten-seconds only, visible frames, stable layout, hide/resume and disposal passed\\n";
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
