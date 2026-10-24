#pragma once
#include "stream_monitor.hpp"
#include "ui_widgets.hpp"
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QLocale>
#include <QToolButton>

namespace sn {
inline QString phrase(bool ru,const char* russian,const char* english){return QString::fromUtf8(ru?russian:english);}
inline QString volume(double bytes,bool ru) {
    const auto locale=ru?QLocale(QLocale::Russian):QLocale(QLocale::English);
    if(bytes>=1024.0*1024*1024)return locale.toString(bytes/(1024.0*1024*1024),'f',2)+phrase(ru," ГБ"," GiB");
    if(bytes>=1024.0*1024)return locale.toString(bytes/(1024.0*1024),'f',1)+phrase(ru," МБ"," MiB");
    if(bytes>=1024)return locale.toString(bytes/1024.0,'f',1)+phrase(ru," КБ"," KiB");
    return locale.toString(qlonglong(bytes))+phrase(ru," Б"," B");
}
inline QLabel* text(const QString& value,const QString& role="Hint"){
    auto* label=new QLabel(value);label->setObjectName(role);label->setWordWrap(true);
    label->setMinimumWidth(0);return label;
}
class OutputPanel final : public QWidget {
public:
    explicit OutputPanel(bool russian,QWidget* parent=nullptr):QWidget(parent),ru_(russian) {
        setObjectName("Card");setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
        layout_=new QVBoxLayout(this);layout_->setContentsMargins(7,7,7,7);layout_->setSpacing(5);
        layout_->addWidget(text(phrase(ru_,"Потоки OBS / Aitum","OBS / Aitum outputs"),"Section"));
        auto* hint=text(phrase(ru_,"Каждый выход отслеживается отдельно","Each output is monitored independently"));
        hint->setStyleSheet("font-size:10px;color:#8ea1c0;");layout_->addWidget(hint);
        rows_=new QWidget;rowsLayout_=new QVBoxLayout(rows_);
        rowsLayout_->setContentsMargins(0,0,0,0);rowsLayout_->setSpacing(5);layout_->addWidget(rows_);
        auto* note=text(phrase(ru_,"Красный — просадка сейчас или за последние 5 мин","Red: a problem now or within the last 5 min"));
        note->setStyleSheet("font-size:10px;color:#8ea1c0;");
        note->setToolTip(phrase(ru_,"Просадка: битрейт ниже 70% настройки в течение 6 с после 10 с прогрева, новые потери кадров или переподключение. Без заданного битрейта используется недавний стабильный уровень. Статистика записи не учитывается.",
            "Problem: bitrate below 70% of target for 6 s after 10 s warm-up, new dropped frames or reconnecting. Without a configured target, the recent stable bitrate is used. Recording is excluded."));
        layout_->addWidget(note);
    }
    void setOutputs(QVector<OutputView> outputs) {
        CpuScope scope;
        if(outputs.isEmpty()) {
            outputs={{"placeholder-youtube","YouTube","",{}},{"placeholder-twitch","Twitch","",{}}};
        }
        QStringList keys;for(const auto& v:outputs)keys<<v.key;
        if(keys!=keys_) {
            while(auto* item=rowsLayout_->takeAt(0)){delete item->widget();delete item;}
            cards_.clear();keys_=keys;
            for(const auto& v:outputs) {
                auto* box=new QFrame;box->setObjectName("OutputTile");
                box->setStyleSheet("QFrame#OutputTile{background:#0b1520;border:1px solid #2d4056;border-radius:7px;}");
                auto* grid=new QGridLayout(box);grid->setContentsMargins(6,5,6,5);grid->setHorizontalSpacing(7);grid->setVerticalSpacing(3);
                grid->addWidget(new ServiceBadge(v.platform=="RTMP"?"Stream Notify":v.platform),0,0,2,1);
                Row row;row.title=text(v.platform,"Section");row.state=text("","Hint");
                row.state->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
                grid->addWidget(row.title,0,1);grid->addWidget(row.state,0,2);
                grid->addWidget(text(phrase(ru_,"Битрейт","Bitrate")),1,1);grid->addWidget(text(phrase(ru_,"Передано","Transferred")),1,2);
                row.rate=text("—");row.bytes=text("—");
                grid->addWidget(row.rate,2,1);grid->addWidget(row.bytes,2,2);
                grid->addWidget(text(phrase(ru_,"Потери кадров","Dropped frames")),3,1);
                row.drops=text("—");row.drops->setAlignment(Qt::AlignRight);grid->addWidget(row.drops,3,2);
                row.history=text("");row.history->setStyleSheet("font-size:10px;");
                grid->addWidget(row.history,4,0,1,3);
                grid->setColumnStretch(1,1);grid->setColumnStretch(2,1);
                rowsLayout_->addWidget(box);cards_.push_back(row);
            }
        }
        const QLocale locale=ru_?QLocale(QLocale::Russian):QLocale(QLocale::English);
        for(qsizetype i=0;i<outputs.size();++i) {
            const auto& v=outputs[i];const auto& s=v.stats;auto& row=cards_[i];
            row.title->setToolTip(v.name);
            const bool placeholder=v.key.startsWith("placeholder");
            const QString state=!s.active?phrase(ru_,"Ожидание","Idle"):s.reconnecting?phrase(ru_,"Переподключение","Reconnecting"):
                s.problemNow?phrase(ru_,"Нестабильно","Unstable"):s.rateReady&&s.hasCounters?phrase(ru_,"Стабильно","Stable"):phrase(ru_,"Нет данных","No data");
            row.state->setText(state);row.state->setStyleSheet(QString("font-size:10px;color:%1;").arg(s.problemNow?"#ff737d":s.active?"#00df8c":"#8ea1c0"));
            row.rate->setText(s.active&&s.rateReady&&s.hasCounters?locale.toString(s.kbps,'f',0)+phrase(ru_," кбит/с"," kbps"):"—");
            row.rate->setStyleSheet(QString("font-weight:600;color:%1;").arg(s.recentProblem?"#ff737d":"#72e9f4"));
            row.bytes->setText(placeholder?"—":volume(double(s.bytes),ru_));
            row.bytes->setStyleSheet("font-weight:600;color:#dfe8ff;");
            row.drops->setText(placeholder?"—":locale.toString(s.dropPercent,'f',1)+" %");
            row.drops->setStyleSheet(QString("color:%1;").arg(s.recentProblem?"#ff737d":"#00df8c"));
            const QString history=placeholder?phrase(ru_,"Выход пока не обнаружен","Output not detected"):
                !s.active?phrase(ru_,"Выход не запущен","Output is inactive"):s.problemNow?phrase(ru_,"Просадка сейчас","Problem now"):
                s.recentProblem?phrase(ru_,"Просадка %1 с назад","Problem %1 s ago").arg(s.problemAgeMs/1000):
                phrase(ru_,"Наблюдение: %1 с без просадок","Observed: %1 s without problems").arg(std::min<int64_t>(300,s.ageMs/1000));
            row.history->setText(history);row.history->setStyleSheet(QString("font-size:10px;color:%1;").arg(s.recentProblem?"#d68690":"#8ea1c0"));
        }
    }
private:
    struct Row {QLabel *title,*state,*rate,*bytes,*drops,*history;};
    bool ru_;QVBoxLayout *layout_,*rowsLayout_;QWidget* rows_;QStringList keys_;QVector<Row> cards_;
};
class ResourcePanel final : public QWidget {
public:
    explicit ResourcePanel(bool russian,QWidget* parent=nullptr):QWidget(parent),ru_(russian) {
        setObjectName("Card");setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
        auto* grid=new QGridLayout(this);grid->setContentsMargins(7,6,7,6);grid->setSpacing(4);
        auto* heading=new QWidget;auto* headingRow=new QHBoxLayout(heading);headingRow->setContentsMargins(0,0,0,0);headingRow->setSpacing(5);
        auto* chip=new QLabel;chip->setPixmap(streamNavIcon("cpu").pixmap(18,18));headingRow->addWidget(chip);
        headingRow->addWidget(text(phrase(ru_,"Ресурсы плагина","Plugin resources"),"Section"),1);
        grid->addWidget(heading,0,0);
        auto* info=new QToolButton;info->setIcon(streamNavIcon("info"));info->setIconSize(QSize(16,16));info->setFixedSize(18,18);
        info->setStyleSheet("QToolButton{border:0;background:transparent;}");info->setCursor(Qt::WhatsThisCursor);
        info->setToolTip(phrase(ru_,
            "CPU — время отмеченных синхронных участков кода плагина по GetThreadTimes, с учётом числа ядер. Фоновые задачи Qt/ОС отдельно не атрибутируются. Память — ёмкость учтённых строк и буферов с устранением общих копий; это не полная RAM DLL, Qt или OBS. Сеть — данные HTTP/WS плагина, без TLS/TCP-заголовков и трафика стрима.",
            "CPU: instrumented synchronous plugin code via GetThreadTimes, normalized by CPU cores; shared Qt/OS background work is not attributed. Memory: deduplicated retained text/buffer capacity, not total DLL/Qt/OBS RAM. Network: plugin HTTP/WS data, excluding TLS/TCP overhead and stream traffic."));
        grid->addWidget(info,0,1,Qt::AlignRight);
        grid->addWidget(text(phrase(ru_,"CPU кода","Code CPU")),1,0);grid->addWidget(text(phrase(ru_,"Учтённая память","Accounted buffers")),1,1);
        cpu_=text("—");memory_=text("—");cpu_->setStyleSheet("font-weight:600;color:#dfe8ff;");memory_->setStyleSheet(cpu_->styleSheet());
        grid->addWidget(cpu_,2,0);grid->addWidget(memory_,2,1);
        grid->addWidget(text(phrase(ru_,"Сеть плагина","Plugin data")),3,0);
        network_=text("—");network_->setAlignment(Qt::AlignRight);network_->setStyleSheet("color:#72e9f4;font-size:10px;");grid->addWidget(network_,3,1);
        auto* note=text(phrase(ru_,"Наши задачи и буферы · обновление 2 с","Our tasks and buffers · refresh every 2 s"));
        note->setStyleSheet("font-size:10px;color:#8ea1c0;");grid->addWidget(note,4,0,1,2);
        grid->setColumnStretch(0,1);grid->setColumnStretch(1,1);
    }
    void setUsage(const Usage& usage) {
        const QLocale locale=ru_?QLocale(QLocale::Russian):QLocale(QLocale::English);
        cpu_->setText(!usage.cpuAvailable?"—":!usage.ready?phrase(ru_,"Измерение…","Measuring…"):
            usage.cpuPercent<0.01?phrase(ru_,"<0,01 %","<0.01 %"):locale.toString(usage.cpuPercent,'f',2)+" %");
        memory_->setText(volume(double(usage.buffers),ru_));
        network_->setText(usage.ready?QString::fromUtf8("↑ ")+volume(usage.txPerSec,ru_)+phrase(ru_,"/с","/s")+
            QString::fromUtf8("  ↓ ")+volume(usage.rxPerSec,ru_)+phrase(ru_,"/с","/s"):"—");
    }
private:bool ru_;QLabel *cpu_,*memory_,*network_;
};
}
