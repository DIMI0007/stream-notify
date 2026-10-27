#include "stats_dock.hpp"
#ifndef STREAM_STATS_PREVIEW
#include <obs.h>
#include <obs-module.h>
#include <util/bmem.h>
#endif
#include <QFrame>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSettings>
#include <QStyle>
#include <QVBoxLayout>
#include <QLocale>
#include <algorithm>

namespace stats {
namespace {

QString configFile() {
#ifdef STREAM_STATS_PREVIEW
    return QDir::tempPath()+"/stream-stats-preview-ui.ini";
#else
    char* raw=obs_module_config_path("ui.ini");
    if(!raw)return {};
    const QString path=QString::fromUtf8(raw);bfree(raw);
    QDir().mkpath(QFileInfo(path).absolutePath());
    return path;
#endif
}

QString bytesText(quint64 bytes,const QLocale& locale) {
    const double value=double(bytes);
    if(value>=1024.0*1024*1024)return locale.toString(value/(1024.0*1024*1024),'f',1)+" GB";
    if(value>=1024.0*1024)return locale.toString(value/(1024.0*1024),'f',0)+" MB";
    return locale.toString(value/1024.0,'f',0)+" KB";
}
QString duration(qint64 seconds) {
    if(seconds<0)return "—";
    const qint64 hours=seconds/3600,minutes=(seconds/60)%60,secs=seconds%60;
    return QString("%1:%2:%3").arg(hours,2,10,QChar('0')).arg(minutes,2,10,QChar('0')).arg(secs,2,10,QChar('0'));
}

class StatsMark final : public QWidget {
public:
    StatsMark(){setObjectName("StatsMark");setFixedSize(28,28);}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.scale(width()/28.0,height()/28.0);
        p.setPen(QPen(QColor("#00df8c"),2));p.setBrush(QColor("#073326"));
        p.drawEllipse(QRectF(2,2,24,24));
        p.setPen(Qt::NoPen);p.setBrush(QColor("#00df8c"));
        p.drawRoundedRect(QRectF(7,14,3,7),1,1);
        p.drawRoundedRect(QRectF(12,9,3,12),1,1);
        p.drawRoundedRect(QRectF(17,12,3,9),1,1);
    }
};

QFrame* card(const QString& title,const QColor& accent,QVBoxLayout*& body) {
    auto* frame=new QFrame;frame->setObjectName("StatsCard");frame->setMinimumWidth(0);
    auto* layout=new QVBoxLayout(frame);layout->setContentsMargins(7,5,7,5);layout->setSpacing(2);
    auto* label=new QLabel(title);label->setObjectName("CardTitle");
    label->setProperty("accent",accent.name());
    label->setStyleSheet(QString("color:%1;").arg(accent.name()));layout->addWidget(label);
    body=layout;return frame;
}

QLabel* addMetric(QVBoxLayout* layout,const QString& name) {
    auto* wrapper=new QWidget;wrapper->setObjectName("MetricRow");auto* row=new QHBoxLayout(wrapper);
    row->setContentsMargins(0,0,0,0);row->setSpacing(3);
    auto* left=new QLabel(name);left->setObjectName("MetricName");left->setMinimumWidth(0);
    auto* right=new QLabel("—");right->setObjectName("MetricValue");right->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    right->setMinimumWidth(0);
    row->addWidget(left,1);row->addWidget(right,0);
    wrapper->setFixedHeight(14);layout->addWidget(wrapper);
    return right;
}

} // namespace

class Chart final : public QWidget {
public:
    explicit Chart(QVector<QPair<const History*,QColor>> series,QWidget* parent=nullptr)
        :QWidget(parent),series_(std::move(series)) {setMinimumWidth(0);setFixedHeight(38);}
    void setRange(int seconds){seconds_=seconds;update();}
    void setDisplayHeight(int height){setFixedHeight(height);update();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QRectF plot=rect().adjusted(1,2,-1,-2);
        p.setBrush(QColor("#08131f"));p.setPen(QPen(QColor("#263b50"),1));
        p.drawRoundedRect(plot,4,4);
        p.setPen(QPen(QColor("#234056"),1,Qt::DashLine));
        for(int i=1;i<3;++i){const qreal y=plot.top()+plot.height()*i/3.0;p.drawLine(QPointF(plot.left()+2,y),QPointF(plot.right()-2,y));}
        QVector<QVector<double>> values;values.reserve(series_.size());
        double maximum=1;
        for(const auto& series:series_){
            values.push_back(series.first->recent(seconds_));
            for(double value:values.back())maximum=std::max(maximum,value);
        }
        maximum=std::ceil(maximum*1.12);
        for(int s=0;s<series_.size();++s){
            const auto& points=values[s];if(points.size()<2)continue;
            QPainterPath line;
            for(int i=0;i<points.size();++i){
                const qreal x=plot.left()+2+qreal(i)*qreal(plot.width()-4)/qreal(points.size()-1);
                const qreal y=plot.bottom()-2-qreal(points[i]/maximum)*qreal(plot.height()-5);
                if(i==0)line.moveTo(x,y);else line.lineTo(x,y);
            }
            if(s==0){
                QPainterPath area=line;area.lineTo(plot.right()-2,plot.bottom()-2);
                area.lineTo(plot.left()+2,plot.bottom()-2);area.closeSubpath();
                QColor fill=series_[s].second;fill.setAlpha(35);p.fillPath(area,fill);
            }
            p.setPen(QPen(series_[s].second,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p.drawPath(line);
        }
    }
private:
    QVector<QPair<const History*,QColor>> series_;
    int seconds_=60;
};

StatsDock::StatsDock(QWidget* parent):QWidget(parent) {
    setObjectName("StreamStatsDock");setMinimumSize(310,200);
    setStyleSheet(R"(
      QWidget#StreamStatsDock{background:#07111b;color:#dbe7fa;font-family:'Segoe UI';font-size:10px;}
      QFrame#StatsCard{background:#0c1725;border:1px solid #29415a;border-radius:7px;}
      QLabel#CardTitle{font-size:12px;font-weight:700;}
      QLabel#MetricName{color:#9fb5d5;font-size:10px;}
      QLabel#MetricValue{color:#edf5ff;font-size:10px;font-weight:700;}
      QLabel#DockTitle{font-size:17px;font-weight:700;color:#e8f5ff;}
      QLabel#Footer{font-size:10px;color:#9ab2cc;}
      QPushButton{color:#dbe7fa;background:#142238;border:1px solid #344b6a;border-radius:6px;padding:2px 6px;}
      QPushButton:hover{background:#1b3050;border-color:#5983b5;}
      QPushButton#ActiveRange{background:#193d4b;border-color:#00bfa2;color:#e8ffff;}
      QScrollArea{border:0;background:transparent;}
      QScrollArea QWidget#CardHost{background:transparent;}
    )");
    QSettings saved(configFile(),QSettings::IniFormat);
    language_=saved.value("language","auto").toString();
    rangeMinutes_=saved.value("rangeMinutes",1).toInt();
    if(rangeMinutes_!=1&&rangeMinutes_!=5&&rangeMinutes_!=15)rangeMinutes_=1;
    auto* root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);
    pages_=new QStackedWidget;root->addWidget(pages_);
    buildPages();
    tick_.setTimerType(Qt::CoarseTimer);
    QObject::connect(&tick_,&QTimer::timeout,this,[this]{if(!closing_)updateSnapshot(collector_.sample());});
    tick_.start(1000);
    updateSnapshot(collector_.sample());
}

void StatsDock::shutdown(){closing_=true;tick_.stop();}
#ifdef STREAM_STATS_PREVIEW
void StatsDock::previewSamples(int count){for(int i=0;i<count;++i)updateSnapshot(collector_.sample());}
#endif
bool StatsDock::russian() const {
    if(language_=="ru")return true;if(language_=="en")return false;
#ifdef STREAM_STATS_PREVIEW
    return true;
#else
    const char* locale=obs_get_locale();return locale&&QString::fromUtf8(locale).startsWith("ru",Qt::CaseInsensitive);
#endif
}
QString StatsDock::phrase(const char* ru,const char* en) const {return QString::fromUtf8(russian()?ru:en);}

void StatsDock::buildPages() {
    if(home_){pages_->removeWidget(home_);home_->deleteLater();home_=nullptr;}
    if(settings_){pages_->removeWidget(settings_);settings_->deleteLater();settings_=nullptr;}
    layoutMode_=-1;home_=buildHome();settings_=buildSettings();
    pages_->addWidget(home_);pages_->addWidget(settings_);pages_->setCurrentWidget(home_);
    arrangeCards();updateSnapshot(last_);
}

QWidget* StatsDock::buildHome() {
    auto* page=new QWidget;auto* outer=new QVBoxLayout(page);
    outer->setContentsMargins(6,4,6,3);outer->setSpacing(4);
    auto* top=new QHBoxLayout;top->setContentsMargins(1,0,1,0);top->setSpacing(6);
    top->addWidget(new StatsMark);
    auto* heading=new QLabel("Stream Stats");heading->setObjectName("DockTitle");top->addWidget(heading,1);
    for(int minutes:{1,5,15}){
        auto* button=new QPushButton(QString::number(minutes)+phrase(" м"," m"));
        button->setFixedSize(34,23);button->setToolTip(phrase("История графиков","Chart history"));
        QObject::connect(button,&QPushButton::clicked,this,[this,minutes]{setRange(minutes);});
        top->addWidget(button);rangeButtons_.append(button);
    }
    auto* gear=new QPushButton(QString::fromUtf8("⚙"));gear->setObjectName("SettingsButton");gear->setFixedSize(28,27);
    gear->setStyleSheet("font-size:18px;padding:0;");
    gear->setToolTip(phrase("Настройки","Settings"));
    QObject::connect(gear,&QPushButton::clicked,this,[this]{pages_->setCurrentWidget(settings_);});
    top->addWidget(gear);outer->addLayout(top);
    auto* scroller=new QScrollArea;scroller->setWidgetResizable(true);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* host=new QWidget;host->setObjectName("CardHost");cards_=new QGridLayout(host);
    cards_->setContentsMargins(0,0,0,0);cards_->setSpacing(5);
    QVBoxLayout* body=nullptr;
    systemCard_=card(phrase("Система","System"),QColor("#4addd3"),body);
    cpuObs_=addMetric(body,phrase("CPU OBS","OBS CPU"));
    cpuPc_=addMetric(body,phrase("CPU ПК","PC CPU"));
    gpu_=addMetric(body,"GPU");
    ramObs_=addMetric(body,phrase("RAM OBS","OBS RAM"));
    ramFree_=addMetric(body,phrase("Своб. RAM","Free RAM"));
    fps_=addMetric(body,"FPS");
    render_=addMetric(body,phrase("Рендер","Render"));
    renderDrop_=addMetric(body,phrase("Пропуск ренд.","Render missed"));
    encodeDrop_=addMetric(body,phrase("Пропуск код.","Encode skipped"));
    systemChart_=new Chart({{&cpuHistory_,QColor("#00dfa4")},{&obsCpuHistory_,QColor("#43b7ff")},{&gpuHistory_,QColor("#aa79ff")}});
    systemChart_->setToolTip(phrase("CPU ПК — зелёный, CPU OBS — синий, GPU — фиолетовый","PC CPU green, OBS CPU blue, GPU purple"));
    body->addStretch(1);
    body->addWidget(systemChart_);
    fpsChart_=new Chart({{&fpsHistory_,QColor("#ffcc49")}});
    fpsChart_->setToolTip(phrase("История FPS","FPS history"));body->addWidget(fpsChart_);

    streamCard_=card(phrase("Трансляция","Stream"),QColor("#ffce43"),body);
    streamStatus_=addMetric(body,phrase("Статус","Status"));
    streamBitrate_=addMetric(body,phrase("Битрейт Σ","Bitrate Σ"));
    streamCount_=addMetric(body,phrase("Потоков","Outputs"));
    streamBytes_=addMetric(body,phrase("Отправлено","Sent"));
    streamDrop_=addMetric(body,phrase("Потери сети","Network loss"));
    body->addStretch(1);
    streamChart_=new Chart({{&streamHistory_,QColor("#ffbf37")}});
    streamChart_->setToolTip(phrase("Суммарный фактический битрейт активных выходов","Actual aggregate bitrate of active outputs"));
    body->addWidget(streamChart_);

    recordCard_=card(phrase("Запись","Recording"),QColor("#ff556e"),body);
    recordStatus_=addMetric(body,phrase("Статус","Status"));
    recordBitrate_=addMetric(body,phrase("Битрейт","Bitrate"));
    recordFile_=addMetric(body,phrase("Размер файла","File size"));
    recordSpeed_=addMetric(body,phrase("Скорость","Write speed"));
    recordTime_=addMetric(body,phrase("Время","Duration"));
    recordDisk_=addMetric(body,phrase("Диск свободен","Disk free"));
    recordEta_=addMetric(body,phrase("До заполн.","Until full"));
    body->addStretch(1);
    recordChart_=new Chart({{&recordHistory_,QColor("#23cdec")}});
    recordChart_->setToolTip(phrase("Фактический битрейт записи","Actual recording bitrate"));
    body->addWidget(recordChart_);
    scroller->setWidget(host);outer->addWidget(scroller,1);
    footer_=new QLabel("developed by OTMOPOZOK · v1.0.0");
    footer_->setObjectName("Footer");footer_->setAlignment(Qt::AlignCenter);
    footer_->setFixedHeight(15);outer->addWidget(footer_);
    setRange(rangeMinutes_);
    return page;
}

QWidget* StatsDock::buildSettings() {
    auto* page=new QWidget;auto* outer=new QVBoxLayout(page);outer->setContentsMargins(9,8,9,8);outer->setSpacing(8);
    auto* top=new QHBoxLayout;
    auto* back=new QPushButton(phrase("← Назад","← Back"));back->setFixedSize(70,29);
    QObject::connect(back,&QPushButton::clicked,this,[this]{pages_->setCurrentWidget(home_);});
    top->addWidget(back);auto* title=new QLabel(phrase("Настройки","Settings"));title->setObjectName("DockTitle");
    top->addWidget(title,1);outer->addLayout(top);
    auto* label=new QLabel(phrase("Язык интерфейса","Interface language"));outer->addWidget(label);
    auto* choices=new QHBoxLayout;choices->setSpacing(5);
    const struct {const char* code;const char* ru;const char* en;} items[]={
        {"auto","Авто","Auto"},{"ru","Русский","Russian"},{"en","English","English"}};
    for(const auto& item:items){
        auto* button=new QPushButton(phrase(item.ru,item.en));button->setCheckable(true);
        button->setChecked(language_==item.code);
        QObject::connect(button,&QPushButton::clicked,this,[this,code=QString::fromLatin1(item.code)]{setLanguage(code);});
        choices->addWidget(button,1);
    }
    outer->addLayout(choices);
    auto* hint=new QLabel(phrase("Графики обновляются раз в секунду. История хранится только в памяти OBS и очищается при закрытии программы.",
        "Charts update once per second. History stays in OBS memory and clears when OBS exits."));
    hint->setWordWrap(true);hint->setObjectName("MetricName");outer->addWidget(hint);
    outer->addStretch(1);return page;
}

void StatsDock::setRange(int minutes){
    rangeMinutes_=minutes;
    if(!configFile().isEmpty()){QSettings saved(configFile(),QSettings::IniFormat);saved.setValue("rangeMinutes",minutes);}
    for(auto* button:rangeButtons_){
        if(!button)continue;
        const bool active=button->text().startsWith(QString::number(minutes)+" ");
        button->setObjectName(active?"ActiveRange":"");
        button->style()->unpolish(button);button->style()->polish(button);
    }
    for(auto* chart:{systemChart_,fpsChart_,streamChart_,recordChart_})if(chart)chart->setRange(minutes*60);
}

void StatsDock::setLanguage(const QString& code){
    language_=code;
    QSettings saved(configFile(),QSettings::IniFormat);saved.setValue("language",code);saved.sync();
    rangeButtons_.clear();buildPages();pages_->setCurrentWidget(settings_);
}

void StatsDock::arrangeCards(){
    if(!cards_||!home_)return;
    const bool wide=width()>=680 && width()>height()*1.3;
    const bool roomy=wide&&width()>=1100&&height()>=600;
    const int mode=roomy?2:wide?1:0;
    if(layoutMode_==mode)return;
    layoutMode_=mode;
    horizontal_=wide;
    for(auto* widget:{systemCard_,streamCard_,recordCard_})cards_->removeWidget(widget);
    if(wide){
        cards_->addWidget(systemCard_,0,0);cards_->addWidget(streamCard_,0,1);cards_->addWidget(recordCard_,0,2);
        for(int i=0;i<3;++i)cards_->setColumnStretch(i,1);
        fpsChart_->hide();
    }else{
        cards_->addWidget(systemCard_,0,0);cards_->addWidget(streamCard_,1,0);cards_->addWidget(recordCard_,2,0);
        cards_->setColumnStretch(0,1);cards_->setColumnStretch(1,0);cards_->setColumnStretch(2,0);
        fpsChart_->show();
    }
    const int chartHeight=roomy?155:wide?35:74;
    for(auto* chart:{systemChart_,fpsChart_,streamChart_,recordChart_})chart->setDisplayHeight(chartHeight);
    const int metricHeight=roomy?44:wide?14:20;
    const int metricFont=roomy?15:wide?10:11;
    for(auto* row:home_->findChildren<QWidget*>("MetricRow"))row->setFixedHeight(metricHeight);
    for(auto* label:home_->findChildren<QLabel*>("MetricName"))
        label->setStyleSheet(QString("color:#9fb5d5;font-size:%1px;").arg(metricFont));
    for(auto* label:home_->findChildren<QLabel*>("MetricValue"))
        label->setStyleSheet(QString("color:#edf5ff;font-size:%1px;font-weight:700;").arg(metricFont));
    for(auto* label:home_->findChildren<QLabel*>("CardTitle"))
        label->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;")
            .arg(label->property("accent").toString()).arg(roomy?22:wide?12:15));
    if(auto* title=home_->findChild<QLabel*>("DockTitle"))
        title->setStyleSheet(QString("font-size:%1px;font-weight:700;color:#e8f5ff;").arg(roomy?30:17));
    if(auto* mark=home_->findChild<QWidget*>("StatsMark"))mark->setFixedSize(roomy?45:28,roomy?45:28);
    if(auto* gear=home_->findChild<QPushButton*>("SettingsButton"))gear->setFixedSize(roomy?42:28,roomy?40:27);
    if(footer_)footer_->setStyleSheet(QString("font-size:%1px;color:#9ab2cc;").arg(roomy?14:10));
    if(footer_)footer_->setFixedHeight(roomy?25:15);
    for(auto* button:rangeButtons_)button->setFixedSize(roomy?48:34,roomy?32:23);
    for(auto* frame:{systemCard_,streamCard_,recordCard_}){
        auto* layout=static_cast<QVBoxLayout*>(frame->layout());
        layout->setContentsMargins(roomy?13:7,roomy?10:5,roomy?13:7,roomy?10:5);
    }
}

void StatsDock::resizeEvent(QResizeEvent* event){QWidget::resizeEvent(event);if(cards_)arrangeCards();}

void StatsDock::updateSnapshot(const Snapshot& value){
    last_=value;
    cpuHistory_.add(std::max(0.0,value.system.cpuPc));
    obsCpuHistory_.add(std::max(0.0,value.system.cpuObs));
    gpuHistory_.add(std::max(0.0,value.system.gpu));
    fpsHistory_.add(value.system.fps);
    streamHistory_.add(value.stream.kbps);
    recordHistory_.add(value.record.kbps);
    if(!cpuObs_)return;
    const QLocale locale=russian()?QLocale(QLocale::Russian):QLocale(QLocale::English);
    const auto percentage=[&](double n){return n<0?QString("—"):locale.toString(n,'f',1)+" %";};
    const auto rate=[&](double n){return locale.toString(n,'f',0)+" kb/s";};
    cpuObs_->setText(percentage(value.system.cpuObs));cpuPc_->setText(percentage(value.system.cpuPc));
    gpu_->setText(percentage(value.system.gpu));
    gpu_->setToolTip(phrase("Загрузка самого занятого GPU-движка; «—» означает, что счётчик Windows недоступен.",
        "Busiest GPU engine; — means the Windows counter is unavailable."));
    ramObs_->setText(bytesText(value.system.ramObs,locale));
    ramFree_->setText(bytesText(value.system.ramFree,locale));
    fps_->setText(locale.toString(value.system.fps,'f',1));
    render_->setText(locale.toString(value.system.renderMs,'f',1)+" ms");
    renderDrop_->setText(locale.toString(value.system.renderMissed)+" / "+locale.toString(value.system.renderFrames));
    encodeDrop_->setText(locale.toString(value.system.encodeSkipped)+" / "+locale.toString(value.system.encodeFrames));
    streamStatus_->setText(value.stream.reconnecting?phrase("ПЕРЕПОДКЛ.","RECONNECTING"):
        value.stream.active?phrase("● В ЭФИРЕ","● LIVE"):phrase("Неактивна","Offline"));
    streamStatus_->setStyleSheet(QString("color:%1;font-weight:700;").arg(value.stream.reconnecting?"#ffca56":value.stream.active?"#00df8c":"#8fa5c2"));
    streamBitrate_->setText((value.stream.incomplete?QString::fromUtf8("≥ "):QString())+rate(value.stream.kbps));
    streamBitrate_->setToolTip(value.stream.incomplete?phrase("Сумма неполная: не все активные выходы отдают счётчики.",
        "Incomplete total: some active outputs do not expose measurable counters."):QString());
    streamCount_->setText(locale.toString(value.stream.activeOutputs));
    streamBytes_->setText(bytesText(value.stream.totalBytes,locale));
    const double loss=value.stream.frames?100.0*double(value.stream.dropped)/double(value.stream.frames):0;
    streamDrop_->setText(locale.toString(value.stream.dropped)+" ("+locale.toString(loss,'f',1)+" %)");
    recordStatus_->setText(value.record.paused?"PAUSED":value.record.active?"● REC":"OFFLINE");
    recordStatus_->setStyleSheet(QString("color:%1;font-weight:700;").arg(value.record.paused?"#ffca56":value.record.active?"#ff5570":"#8fa5c2"));
    recordBitrate_->setText(value.record.active?rate(value.record.kbps):"—");
    recordFile_->setText(value.record.pathAvailable?bytesText(value.record.fileBytes,locale):"—");
    recordSpeed_->setText(value.record.active?locale.toString(value.record.megabytesPerSecond,'f',1)+" MB/s":"—");
    recordTime_->setText(value.record.active?duration(value.record.elapsedSeconds):"—");
    recordTime_->setToolTip(phrase("Оценка по времени создания текущего файла; паузы могут входить во время.",
        "Estimated from current file creation time; pauses may be included."));
    recordDisk_->setText(value.record.pathAvailable?bytesText(value.record.freeBytes,locale):"—");
    recordEta_->setText(value.record.secondsUntilFull>=0?duration(value.record.secondsUntilFull):"—");
    for(auto* chart:{systemChart_,fpsChart_,streamChart_,recordChart_})chart->update();
}

} // namespace stats

