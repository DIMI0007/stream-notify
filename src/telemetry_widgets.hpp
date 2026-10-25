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
        layout_->addWidget(text(phrase(ru_,"Потоки","Streams"),"Section"));
        rows_=new QWidget;rowsLayout_=new QVBoxLayout(rows_);
        rowsLayout_->setContentsMargins(0,0,0,0);rowsLayout_->setSpacing(5);layout_->addWidget(rows_);
        setToolTip(phrase(ru_,"Красный битрейт — просадка сейчас или за последние 5 минут. Учитываются устойчивое падение ниже 70% цели, сетевые потери и переподключение. Каждая платформа отслеживается отдельно.",
            "Red bitrate: a problem now or in the last 5 minutes. Sustained bitrate below 70% of target, dropped network frames and reconnecting are tracked independently for each output."));
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
                row.rate=text("—");row.bytes=text("—");row.bytes->setAlignment(Qt::AlignRight);
                grid->addWidget(row.rate,1,1);grid->addWidget(row.bytes,1,2);
                grid->setColumnStretch(1,1);grid->setColumnStretch(2,1);
                rowsLayout_->addWidget(box);cards_.push_back(row);
            }
        }
        const QLocale locale=ru_?QLocale(QLocale::Russian):QLocale(QLocale::English);
        for(qsizetype i=0;i<outputs.size();++i) {
            const auto& v=outputs[i];const auto& s=v.stats;auto& row=cards_[i];
            row.title->setToolTip(v.name);
            const bool placeholder=v.key.startsWith("placeholder");
            const QString state=!s.active?phrase(ru_,"Не идёт","Offline"):s.reconnecting?phrase(ru_,"Переподключение","Reconnecting"):
                s.problemNow?phrase(ru_,"Нестабильно","Unstable"):s.rateReady&&s.hasCounters?phrase(ru_,"В эфире","Live"):phrase(ru_,"Нет данных","No data");
            row.state->setText(state);row.state->setStyleSheet(QString("font-size:10px;color:%1;").arg(s.problemNow?"#ff737d":s.active?"#00df8c":"#8ea1c0"));
            row.rate->setText(s.active&&s.rateReady&&s.hasCounters?locale.toString(s.kbps,'f',0)+phrase(ru_," кбит/с"," kbps"):"—");
            row.rate->setStyleSheet(QString("font-weight:600;color:%1;").arg(s.recentProblem?"#ff737d":"#72e9f4"));
            row.bytes->setText(placeholder?"—":locale.toString(double(s.bytes)/(1024.0*1024*1024),'f',2)+phrase(ru_," ГБ"," GiB"));
            row.bytes->setStyleSheet("font-weight:600;color:#dfe8ff;");
            const QString details=phrase(ru_,"Передано за запуск выхода. Потери кадров: %1%.","Transferred during this output session. Dropped frames: %1%.").arg(locale.toString(s.dropPercent,'f',1))+
                (s.recentProblem?phrase(ru_," Просадка %1 с назад."," Problem %1 s ago.").arg(s.problemAgeMs/1000):QString());
            row.bytes->setToolTip(details);row.rate->setToolTip(details);

        }
    }
private:
    struct Row {QLabel *title,*state,*rate,*bytes;};
    bool ru_;QVBoxLayout *layout_,*rowsLayout_;QWidget* rows_;QStringList keys_;QVector<Row> cards_;
};
class ResourcePanel final : public QWidget {
public:
    explicit ResourcePanel(bool russian,QWidget* parent=nullptr):QWidget(parent),ru_(russian) {
        setObjectName("Card");setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
        auto* grid=new QGridLayout(this);grid->setContentsMargins(7,6,7,6);grid->setSpacing(4);
        auto* heading=new QWidget;auto* headingRow=new QHBoxLayout(heading);headingRow->setContentsMargins(0,0,0,0);headingRow->setSpacing(5);
        auto* chip=new QLabel;chip->setPixmap(streamNavIcon("cpu").pixmap(18,18));headingRow->addWidget(chip);
        headingRow->addWidget(text(phrase(ru_,"Нагрузка плагина","Plugin usage"),"Section"),1);
        grid->addWidget(heading,0,0,1,2);
        auto* info=new QToolButton;info->setIcon(streamNavIcon("info"));info->setIconSize(QSize(16,16));info->setFixedSize(18,18);
        info->setStyleSheet("QToolButton{border:0;background:transparent;}");info->setCursor(Qt::WhatsThisCursor);
        info->setToolTip(phrase(ru_,
            "CPU — время отмеченных синхронных участков кода плагина по GetThreadTimes, с учётом числа ядер. Фоновые задачи Qt/ОС отдельно не атрибутируются. Память — ёмкость учтённых строк и буферов с устранением общих копий; это не полная RAM DLL, Qt или OBS. Трафик за сеанс — данные HTTP/WS плагина с момента запуска OBS, без TLS/TCP-заголовков и трафика стрима.",
            "CPU: instrumented synchronous plugin code via GetThreadTimes, normalized by CPU cores; shared Qt/OS background work is not attributed. Memory: deduplicated retained text/buffer capacity, not total DLL/Qt/OBS RAM. Session traffic: plugin HTTP/WS data since OBS started, excluding TLS/TCP overhead and stream traffic."));
        info->setAccessibleName(phrase(ru_,"Что измеряется","What is measured"));headingRow->addWidget(info);
        cpu_=text("—");memory_=text("—");cpu_->setStyleSheet("font-weight:600;color:#dfe8ff;");memory_->setStyleSheet(cpu_->styleSheet());
        auto makeMetric=[&](const QString& caption,QLabel* value){
            auto* cell=new QWidget;auto* row=new QHBoxLayout(cell);row->setContentsMargins(0,0,0,0);row->setSpacing(4);
            auto* name=text(caption);name->setStyleSheet("font-size:10px;color:#8ea1c0;");
            row->addWidget(name);row->addWidget(value,1);return cell;
        };
        grid->addWidget(makeMetric(phrase(ru_,"CPU","CPU"),cpu_),1,0);
        grid->addWidget(makeMetric(phrase(ru_,"Память данных","Data memory"),memory_),1,1);
        auto* trafficCaption=text(phrase(ru_,"Трафик за сеанс","Session traffic"));
        trafficCaption->setStyleSheet("font-size:10px;color:#8ea1c0;");
        grid->addWidget(trafficCaption,2,0);
        network_=text("—");network_->setAlignment(Qt::AlignRight);network_->setStyleSheet("color:#72e9f4;font-size:10px;");grid->addWidget(network_,2,1);
        grid->setColumnStretch(0,1);grid->setColumnStretch(1,1);
    }
    void setUsage(const Usage& usage) {
        const QLocale locale=ru_?QLocale(QLocale::Russian):QLocale(QLocale::English);
        cpu_->setText(!usage.cpuAvailable?"—":!usage.ready?phrase(ru_,"Измерение…","Measuring…"):
            usage.cpuPercent<0.01?phrase(ru_,"<0,01 %","<0.01 %"):locale.toString(usage.cpuPercent,'f',2)+" %");
        memory_->setText(usage.buffers?volume(double(usage.buffers),ru_):"—");
        network_->setText(QString::fromUtf8("↑ ")+volume(double(usage.txTotal),ru_)+
            QString::fromUtf8("  ↓ ")+volume(double(usage.rxTotal),ru_));
    }
private:bool ru_;QLabel *cpu_,*memory_,*network_;
};
}

