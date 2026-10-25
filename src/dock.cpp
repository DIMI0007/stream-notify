#include "dock.hpp"
#include "stream_activity.hpp"
#include "ui_widgets.hpp"
#include "telemetry_widgets.hpp"
#include "resources.hpp"
#include "telegram_channel.hpp"
#include <obs.h>
#include <obs-frontend-api.h>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QScrollArea>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QUrl>
#include <QDesktopServices>
#include <QDateTime>
#include <QRegularExpression>
#include <QFont>
#include <QMap>
#include <QProgressBar>
#include <QTextBrowser>
#include <cmath>
#include <algorithm>

namespace {
const char* css = R"(
QWidget { font-size: 12px; }
QWidget#PageBody, QWidget#ScrollViewport { background:#080f18; }
QPushButton:disabled { color:#6c7890;border-color:#273246; }
QWidget#StreamRoot { background: #080f18; color: #dfe8ff; }
QWidget#WaitingCard { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #211e16,stop:1 #0c151e);border:1px solid #51452b;border-radius:8px; }
QWidget#Card { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #101925,stop:1 #0b131e); border: 1px solid #263244; border-radius: 8px; }
QLabel { color: #dfe8ff; background: transparent; }
QLabel#Hint { color: #9baccc; }
QLabel#Title { font-weight: 750; font-size: 17px; }
QLabel#Section { color: #d6e5fa; font-size: 13px; font-weight: 700; }
QLabel#Waiting { color:#ffdc45; font-weight:700; }
QLabel#Good { color: #07df91; font-weight: 700; }
QLabel#Bad { color: #ff8484; font-weight: 700; }
QLabel#Count { color: #59dfff; font-weight: 750; font-size: 57px; }
QPushButton { color: #e1ebff; background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #202a3b,stop:1 #141d2b); border: 1px solid #39465d; border-radius: 8px; min-height: 20px; padding: 3px 7px; }
QPushButton:hover { background:#26344d;border-color:#6476a0; }
QPushButton:focus { border-color:#8a79ff; }
QPushButton:pressed { background:#111b30; }
QPushButton#ActiveNav { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #18203c,stop:1 #111e50); border: 1px solid #303e6c; border-bottom: 2px solid #6547ff; font-weight: 700; }
QPushButton#DonationTab {font-size:11px;min-height:18px;padding:3px 4px;background:#0d1824;border:0;border-bottom:2px solid #243246;border-radius:4px;}
QPushButton#DonationTab[selected="true"] {color:#b8ffe1;background:#102c29;border-bottom-color:#00d58a;}
QPushButton#Cancel { background: #16395b; border-color: #40b7ff; font-weight: 700; min-height: 28px; }
QLineEdit,QTextEdit,QComboBox,QDoubleSpinBox { color: #e7eeff; background: #0c1420; border: 1px solid #344865; border-radius: 6px; padding: 4px; selection-background-color: #316ac4; }
QComboBox QAbstractItemView { background: #101a2b; color: #e7eeff; }
QCheckBox { color: #dce7fb; spacing: 6px; min-height: 20px; }
QCheckBox::indicator { width: 16px; height: 16px; }
QCheckBox::indicator:unchecked { border-radius: 4px; background: #344357; border: 1px solid #65748d; }
QCheckBox::indicator:checked { border-radius: 4px; background: #00d37d; border: 1px solid #2ff9a0; }
QScrollArea { border: 0; background: transparent; }
QScrollBar:vertical { border: none; background: #101929; width: 9px; }
QScrollBar::handle:vertical { background: #384d6c; border-radius: 4px;min-height:24px; }
QScrollBar::handle:vertical:hover { background:#52698d; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0px;background:transparent;border:0;}
QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical {background:transparent;}
QLineEdit:focus,QTextEdit:focus,QComboBox:focus,QDoubleSpinBox:focus {border-color:#776af5;background:#111c2e;}

)";

QString serviceStatus(const QString& value,bool russian) {
    if(!russian)return value;
    static const QMap<QString,QString> names={
        {"Disabled",QString::fromUtf8("Сервис выключен")},
        {"Module disabled",QString::fromUtf8("Модуль выключен")},
        {"Connecting",QString::fromUtf8("Подключение…")},
        {"Connected",QString::fromUtf8("Подключён")},
        {"Reconnecting",QString::fromUtf8("Переподключение…")},
        {"Waiting for stream",QString::fromUtf8("Ожидание трансляции")},
        {"Not configured",QString::fromUtf8("Введите токен")},
        {"API verification required",QString::fromUtf8("API требует проверки")},
        {"API responded (legacy, unverified)",QString::fromUtf8("API ответил — экспериментальный режим")},
        {"Invalid or expired token",QString::fromUtf8("Токен недействителен или истёк")},
        {"Rate limited",QString::fromUtf8("Лимит запросов — подождите")},
        {"Network error",QString::fromUtf8("Ошибка соединения")},
        {"Journal error",QString::fromUtf8("Ошибка журнала")},
        {"No rates file: RUB only",QString::fromUtf8("Курсы не загружены — доступен RUB")},
        {"Loading exchange rates",QString::fromUtf8("Загрузка курсов…")}
    };
    if(value.startsWith("CBR /"))return QString(value).replace("currencies",QString::fromUtf8("валют"));
    return names.value(value,value);
}

QVBoxLayout* column(QWidget* w, int gap = 9, int padding = 12)
{
    auto* v = new QVBoxLayout(w);
    v->setSpacing(std::min(gap,5));
    const int inset=std::min(padding,8);
    v->setContentsMargins(inset,inset,inset,inset);
    return v;
}
void rowWith(QVBoxLayout* v, QWidget* first, QWidget* last)
{
    auto* h = new QHBoxLayout();
    h->setSpacing(4);
    h->addWidget(first, 1);
    if(last) h->addWidget(last);
    v->addLayout(h);
}
QPushButton* btn(const QString& text, QWidget* parent = nullptr)
{
    auto* p = new QPushButton(text, parent);
    p->setCursor(Qt::PointingHandCursor);
    if(text==QString::fromUtf8("Копировать") || text=="Copy") {
        p->setText(QString());p->setIcon(streamNavIcon("copy"));p->setIconSize(QSize(18,18));
        p->setFixedWidth(30);p->setToolTip(text);p->setAccessibleName(text);
    }
    if(text==QString::fromUtf8("Показать") || text=="Show") {
        p->setText(QString());
        p->setIcon(streamEyeIcon());
        p->setIconSize(QSize(20,20));
        p->setFixedWidth(28);
        p->setToolTip(text);
        p->setAccessibleName(text);
    }
    return p;
}
QLabel* paragraph(const QString& text, QWidget* parent = nullptr)
{
    auto* l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}
}


class CreditFooter final : public QWidget {
public:
    explicit CreditFooter(QWidget* parent=nullptr):QWidget(parent) {
        setFixedHeight(20);setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);
        setAccessibleName("developed by OTMOPOZOK — YouTube");
        setToolTip("https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/");
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        QFont f=font();f.setPixelSize(11);p.setFont(f);
        const QFontMetricsF m(f);
        const QString prefix="developed by ",nick="OTMOPOZOK";
        const qreal x=std::max(qreal(2),(width()-m.horizontalAdvance(prefix+nick))/2);
        const qreal baseline=(height()-m.height())/2+m.ascent();
        const qreal nickX=x+m.horizontalAdvance(prefix);
        nickRect_=QRectF(nickX-2,baseline-m.ascent()-2,m.horizontalAdvance(nick)+4,m.height()+4);
        auto draw=[&](qreal left,const QString& text,const QColor& fill) {
            QPainterPath path;path.addText(QPointF(left,baseline),f,text);
            p.setPen(QPen(QColor(255,255,255,70),1.0,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p.setBrush(fill);p.drawPath(path);
        };
        draw(x,prefix,QColor("#9299a5"));
        draw(nickX,nick,hover_?QColor("#59dfff"):QColor("#9299a5"));
        if(hover_||hasFocus()) {
            p.setPen(QPen(QColor("#59dfff"),1));
            p.drawLine(QPointF(nickX,baseline+2),QPointF(nickX+m.horizontalAdvance(nick),baseline+2));
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        const bool next=nickRect_.contains(event->position());
        if(next!=hover_){hover_=next;setCursor(next?Qt::PointingHandCursor:Qt::ArrowCursor);update();}
    }
    void leaveEvent(QEvent*) override {hover_=false;setCursor(Qt::ArrowCursor);update();}
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button()==Qt::LeftButton && nickRect_.contains(event->position()))openChannel();
    }
    void keyPressEvent(QKeyEvent* event) override {
        if(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter||event->key()==Qt::Key_Space)openChannel();
        else QWidget::keyPressEvent(event);
    }
    void focusInEvent(QFocusEvent* event) override {QWidget::focusInEvent(event);update();}
    void focusOutEvent(QFocusEvent* event) override {QWidget::focusOutEvent(event);update();}
private:
    void openChannel(){QDesktopServices::openUrl(QUrl("https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/"));}
    QRectF nickRect_;
    bool hover_=false;
};

StreamDock::StreamDock(QWidget* parent) : QWidget(parent), store_(this), sender_(this), testSender_(this), donation_(&store_,this)
{
    setObjectName("StreamRoot");
    setAttribute(Qt::WA_StyledBackground,true);
    setMinimumWidth(275);
    setStyleSheet(css);
    loadedCleanly_ = store_.load();
    if (!loadedCleanly_) statusText_ = store_.error();
    connect(&store_, &SettingsStore::persisted, this, &StreamDock::updateSaved);
    countdown_.setInterval(1000);
    connect(&countdown_, &QTimer::timeout, this, &StreamDock::updateCountdown);
    resultTimer_.setSingleShot(true);
    connect(&resultTimer_, &QTimer::timeout, this, [this] { showPhase(Phase::Idle); });
    root_ = column(this, 4, 5);
    donationConfigure_.setSingleShot(true);
    donationConfigure_.setInterval(1400);
    connect(&donationConfigure_, &QTimer::timeout, &donation_, &DonationEngine::configure);
    connect(&donation_, &DonationEngine::updated, this, [this] {
        if(daStatusLabel_) daStatusLabel_->setText(serviceStatus(donation_.daStatus(),isRu()));
        if(dpStatusLabel_) dpStatusLabel_->setText(serviceStatus(donation_.dpStatus(),isRu()));
        if(donationSummaryLabel_)donationSummaryLabel_->setText(donationSummaryText());
        if(donationJournalLabel_) {
            const auto entries=donation_.journal();
            donationJournalLabel_->setText(entries.isEmpty()?uiText("Пока нет донатов","No donations yet"):entries.mid(0,6).join("\n"));
        }
    });
    broadcasting_ = anyStreamingOutputActive();
    donation_.setLive(broadcasting_);
    donation_.configure();
    if (broadcasting_) alreadyHandled_ = true; // No retrospective notifications.
    // Plugin outputs can be created after frontend initialization.
    streamStateTimer_.setInterval(1000);
    connect(&streamStateTimer_,&QTimer::timeout,this,&StreamDock::refreshStreamingState);
    streamStateTimer_.start();
    telemetryClock_.start();usage_=usageSampler_.sample(0);
    telemetryTimer_.setInterval(2000);
    connect(&telemetryTimer_,&QTimer::timeout,this,&StreamDock::refreshTelemetry);
    telemetryTimer_.start();refreshTelemetry();
    rebuild();
}
StreamDock::~StreamDock() { shutdown(); }

bool StreamDock::isRu() const
{
    sn::CpuScope snCpuScope;
    if (store_.current.locale == "ru") return true;
    if (store_.current.locale == "en") return false;
    const char* locale = obs_get_locale();
    return locale && QString::fromUtf8(locale).startsWith("ru", Qt::CaseInsensitive);
}
QString StreamDock::uiText(const char* ru, const char* en) const
{
    sn::CpuScope snCpuScope; return QString::fromUtf8(isRu() ? ru : en); }
QLabel* StreamDock::label(const QString& text, const QString& name)
{
    sn::CpuScope snCpuScope;
    auto* w = paragraph(text);
    if(!name.isEmpty()) w->setObjectName(name);
    return w;
}
void StreamDock::section(QVBoxLayout* l, const QString& text)
{
    sn::CpuScope snCpuScope; l->addWidget(label(text, "Section")); }
QWidget* StreamDock::card(QVBoxLayout** l)
{
    sn::CpuScope snCpuScope;
    QWidget* w = new QWidget;
    w->setObjectName("Card");
    *l = column(w, store_.current.compact ? 3 : 5, store_.current.compact ? 5 : 8);
    w->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    (*l)->setAlignment(Qt::AlignTop);
    return w;
}
void StreamDock::changeSettings() {
    sn::CpuScope snCpuScope; store_.scheduleSave(); donationConfigure_.start(); }
void StreamDock::showPage(Page page) {
    sn::CpuScope snCpuScope; page_ = page; rebuild(); }
void StreamDock::openConnections(const QString& field) {
    sn::CpuScope snCpuScope;
    pendingFocus_=field;showPage(Page::Connections);
}
void StreamDock::showPhase(Phase phase) {
    sn::CpuScope snCpuScope; phase_ = phase; rebuild(); }
void StreamDock::updateSaved(bool success)
{
    sn::CpuScope snCpuScope;
    if(!success) {
        const QString error=uiText("Не удалось сохранить настройки. Проверьте доступ к папке OBS.",
            "Settings could not be saved. Check access to the OBS configuration folder.");
        if(statusText_!=error){statusText_=error;rebuild();}
    }
}

void StreamDock::rebuild()
{
    sn::CpuScope snCpuScope;
    // Never destroy a control while Qt is still delivering its input signal.
    // Coalesce changes and let the current event return before replacing widgets.
    if(shuttingDown_ || rebuildPending_)return;
    rebuildPending_=true;
    QTimer::singleShot(0,this,[this] {
        rebuildPending_=false;
        if(!shuttingDown_)rebuildNow();
    });
}

void StreamDock::rebuildNow()
{
    sn::CpuScope snCpuScope;
    // All text inputs continuously update SettingsStore::current, so rebuilding
    // for a language switch never discards uncommitted typed text.
    while (QLayoutItem* item = root_->takeAt(0)) {
        if (QWidget* w = item->widget()) delete w;
        delete item;
    }
    root_->setSpacing(store_.current.compact ? 3 : 5);
    title_ = nullptr;
    daStatusLabel_ = nullptr; dpStatusLabel_ = nullptr; donationJournalLabel_ = nullptr;
    central_ = nullptr;
    auto* header = new QWidget;
    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(2,2,2,2);
    h->addWidget(new ServiceBadge("Stream Notify"));
    auto* t = label("Stream Notify", "Title");
    title_ = t;
    h->addWidget(t, 1);
    if(store_.current.showStatus) {
        auto* status=new ActivityBadge(store_.current.enabled?uiText("Активен","Active"):uiText("Выключен","Disabled"),store_.current.enabled);
        status->setToolTip(store_.current.enabled
            ? uiText("Автоанонсы включены. Подключения Telegram и Discord настраиваются отдельно.",
                     "Automatic announcements are enabled. Telegram and Discord connections are configured separately.")
            : uiText("Автоанонсы выключены","Automatic announcements are disabled"));
        h->addWidget(status);
    }
    root_->addWidget(header);

    if (phase_ == Phase::Idle) central_ = buildRegular();
    else if (phase_ == Phase::Counting) central_ = buildCountdown();
    else if (phase_ == Phase::Sending) central_ = buildSending();
    else if (phase_ == Phase::Result) central_ = buildResult();
    else {
        QVBoxLayout* v; central_ = card(&v);
        v->addStretch();
        v->addWidget(label(uiText("Отправка отменена", "Notification cancelled"), "Section"), 0, Qt::AlignHCenter);
        v->addWidget(label(uiText("Текущий эфир больше не будет объявлен", "No notification will be sent for this stream")), 0, Qt::AlignHCenter);
        v->addStretch();
    }
    if(phase_!=Phase::Idle) {
        auto* scroll=new QScrollArea;
        scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->viewport()->setObjectName("ScrollViewport");
        if(central_->objectName().isEmpty())central_->setObjectName("PageBody");
        scroll->setWidget(central_);central_=scroll;
    }
    root_->addWidget(central_, 1);
    root_->addWidget(new CreditFooter(this));
    if(!pendingFocus_.isEmpty()) {
        QPointer<QLineEdit> field=findChild<QLineEdit*>(pendingFocus_);
        pendingFocus_.clear();
        QTimer::singleShot(0,this,[this,field] {
            if(!field)return;
            field->setFocus(Qt::OtherFocusReason);
            for(auto* scroll:findChildren<QScrollArea*>())
                if(scroll->widget() && scroll->widget()->isAncestorOf(field))
                    scroll->ensureWidgetVisible(field,0,12);
        });
    }
}

QWidget* StreamDock::buildRegular()
{
    sn::CpuScope snCpuScope;
    auto* wrap = new QWidget;
    auto* vertical = column(wrap, 6, 0);
    auto* tabs = new QWidget;
    auto* nav = new QHBoxLayout(tabs);
    nav->setContentsMargins(0,0,0,0);
    nav->setSpacing(4);
    auto* home = btn(uiText("Основное", "Home"));
    home->setIcon(streamNavIcon("home",page_==Page::Home));
    home->setIconSize(QSize(18,18));
    auto* settings = btn(uiText("Настройки", "Settings"));
    settings->setIcon(streamNavIcon("settings",page_!=Page::Home));
    settings->setIconSize(QSize(18,18));
    home->setObjectName(page_ == Page::Home ? "ActiveNav" : "");
    settings->setObjectName(page_ != Page::Home ? "ActiveNav" : "");
    connect(home, &QPushButton::clicked, this, [this] {showPage(Page::Home);});
    connect(settings, &QPushButton::clicked, this, [this] {showPage(Page::Connections);});
    nav->addWidget(home); nav->addWidget(settings);
    vertical->addWidget(tabs);
    if (page_ != Page::Home) {
        auto* sub = new QWidget;
        auto* subnav = new QGridLayout(sub);
        subnav->setContentsMargins(0,0,0,0);
        subnav->setSpacing(4);
        const struct {Page page; QString name;} pages[] = {
            {Page::Connections, uiText("Связь", "Connect")},
            {Page::Message, uiText("Сообщение", "Message")},
            {Page::Interface, uiText("Интерфейс", "Interface")},
            {Page::Donations, uiText("Донаты", "Donations")}
        };
        int subIndex = 0;
        for (const auto& p : pages) {
            auto* b = btn(p.name);
            const QString icon=p.page==Page::Connections?"connections":p.page==Page::Message?"message":
                p.page==Page::Interface?"interface":"donations";
            b->setIcon(streamNavIcon(icon,p.page==page_));
            b->setIconSize(QSize(16,16));
            if(p.page == page_) b->setObjectName("ActiveNav");
            connect(b, &QPushButton::clicked, this, [this,p] {showPage(p.page);});
            subnav->addWidget(b, subIndex / 2, subIndex % 2);
            ++subIndex;
        }
        vertical->addWidget(sub);
    }
    if(page_==Page::Donations) {
        auto* group=new QWidget;
        auto* row=new QHBoxLayout(group);row->setContentsMargins(0,0,0,0);row->setSpacing(3);
        const struct {DonationPage page;QString title;QString hint;} sections[]={
            {DonationPage::Services,uiText("Сервисы","Services"),uiText("DonationAlerts и DonatePay","DonationAlerts and DonatePay")},
            {DonationPage::Delivery,uiText("Отправка","Delivery"),uiText("Получатели и правила отправки","Recipients and delivery rules")},
            {DonationPage::Amounts,uiText("Суммы","Amounts"),uiText("Рублёвые пороги и курсы валют","RUB thresholds and exchange rates")},
            {DonationPage::Templates,uiText("Текст","Text"),uiText("Шаблоны, предпросмотр и журнал","Templates, preview and delivery log")}
        };
        for(const auto& item:sections) {
            auto* choice=btn(item.title);choice->setObjectName("DonationTab");
            choice->setMinimumWidth(0);choice->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
            choice->setProperty("selected",item.page==donationPage_);
            choice->setProperty("donationSection",int(item.page));choice->setToolTip(item.hint);
            connect(choice,&QPushButton::clicked,this,[this,page=item.page]{donationPage_=page;rebuild();});
            row->addWidget(choice,1);
        }
        vertical->addWidget(group);
    }
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->viewport()->setObjectName("ScrollViewport");
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget* body = nullptr;
    switch(page_) {
        case Page::Home: body = buildHome(); break;
        case Page::Connections: body = buildConnections(); break;
        case Page::Message: body = buildMessage(); break;
        case Page::Interface: body = buildInterface(); break;
        case Page::Donations: body = buildDonations(); break;
    }
    body->setObjectName("PageBody");
    scroll->setWidget(body);
    vertical->addWidget(scroll,1);
    return wrap;
}

QWidget* StreamDock::buildHome()
{
    sn::CpuScope snCpuScope;
    auto* body=new QWidget;
    auto* outer=column(body,5,3);QVBoxLayout* v;
    auto* status=card(&v);
    if(!broadcasting_)status->setObjectName("WaitingCard");
    auto* statusRow=new QHBoxLayout;statusRow->setSpacing(7);
    statusRow->addWidget(new ServiceBadge(broadcasting_?"Stream Notify":"Waiting"));
    auto* statusText=new QVBoxLayout;statusText->setSpacing(2);
    statusText->addWidget(label(broadcasting_?uiText("В эфире","Live"):uiText("Ожидание трансляции","Waiting for stream"),broadcasting_?"Good":"Waiting"));
    statusText->addWidget(label(store_.current.enabled
        ?uiText("Автоанонс · через %1 с","Auto announcement · after %1 s").arg(store_.current.sendDelay)
        :uiText("Автоанонс выключен","Auto announcement off"),"Hint"));
    statusRow->addLayout(statusText,1);v->addLayout(statusRow);
    status->setToolTip(statusText_);outer->addWidget(status);

    auto* channels=card(&v);section(v,uiText("Отправка в мессенджеры","Notification channels"));
    auto* channelRow=new QHBoxLayout;channelRow->setSpacing(8);
    auto addChannel=[&](bool telegram) {
        auto* cell=new QWidget;auto* row=new QHBoxLayout(cell);row->setContentsMargins(0,0,0,0);row->setSpacing(6);
        row->addWidget(new ServiceBadge(telegram?"Telegram":"Discord"));
        auto* details=new QVBoxLayout;details->setSpacing(0);
        details->addWidget(label(telegram?"Telegram":"Discord","Section"));
        const bool configured=telegram?validTelegram(store_.current.telegramToken,store_.current.telegramChat):validDiscord(store_.current.discordWebhook);
        const bool enabled=telegram?store_.current.telegramEnabled:store_.current.discordEnabled;
        if(configured&&enabled)details->addWidget(new ReadyStatus(uiText("Готов","Ready")));
        else details->addWidget(label(!enabled?uiText("Выключен","Off"):uiText("Не настроен","Not set"),"Hint"));
        row->addLayout(details,1);
        cell->setToolTip(uiText("Проверка и подключение: Настройки → Связь","Test and configure under Settings → Connect"));
        channelRow->addWidget(cell,1);
    };
    addChannel(true);addChannel(false);v->addLayout(channelRow);
    if(!validTelegram(store_.current.telegramToken,store_.current.telegramChat) && !validDiscord(store_.current.discordWebhook)){
        auto* setup=btn(uiText("Настроить отправку","Set up notifications"));
        connect(setup,&QPushButton::clicked,this,[this]{openConnections("TelegramToken");});v->addWidget(setup);
    }
    outer->addWidget(channels);

    auto* delivery=card(&v);section(v,uiText("Отправка анонса","Stream announcement"));
    const bool sentNow=telegram_.status==Outcome::Sent || discord_.status==Outcome::Sent;
    const bool cancelled=telegram_.status==Outcome::Cancelled || discord_.status==Outcome::Cancelled;
    // Persisted delivery history belongs in the tooltip, not the current session status.
    // Otherwise yesterday's successful announcement looks freshly sent on OBS startup.
    const bool sent=announcementAttempted_&&sentNow;
    QString outcome=sent?(broadcasting_?uiText("✓ Отправлен","✓ Sent"):
        uiText("Последний анонс: ✓ отправлен","Last announcement: ✓ sent")):
        broadcasting_&&cancelled?uiText("Отправка отменена","Cancelled"):
        broadcasting_&&announcementAttempted_?uiText("Доставка не подтверждена","Delivery unconfirmed"):
        !broadcasting_&&announcementAttempted_?uiText("Последний анонс: доставка не подтверждена","Last announcement: delivery unconfirmed"):
        broadcasting_?uiText("Ещё не отправляли","Not sent yet"):
        uiText("Ожидание трансляции","Waiting for stream");
    auto* announcementStatus=label(outcome,sent?"Good":"Hint");
    announcementStatus->setProperty("announcementStatus",true);
    v->addWidget(announcementStatus);
    delivery->setToolTip(store_.current.lastTime+"\nTelegram: "+store_.current.lastTelegram+"\nDiscord: "+store_.current.lastDiscord+"\n"+statusText_);
    outer->addWidget(delivery);

    auto* donations=card(&v);
    section(v,uiText("Донаты за эфир","Stream donations"));
    auto* donationSummary=label(donationSummaryText(),"Hint");donationSummaryLabel_=donationSummary;
    donationSummary->setToolTip(uiText(
        "Полученные плагином реальные донаты за текущий/последний эфир, включая суммы ниже порога. Тесты и повторы исключены. Перезапуск OBS обнуляет эту сводку; журнал доставки сохраняется.",
        "Real donations observed during the current/last stream, including amounts below the announcement threshold. Tests and duplicates are excluded. This summary resets on OBS restart; delivery history remains saved."));
    v->addWidget(donationSummary);outer->addWidget(donations);

    auto* outputs=new sn::OutputPanel(isRu());outputs->setObjectName("OutputMonitor");
    outputs->setStyleSheet("QWidget#OutputMonitor{background:#0c1520;border:1px solid #263244;border-radius:8px;}");
    outputs->setOutputs(monitor_.views());outputPanel_=outputs;outer->addWidget(outputs);
    auto* resources=new sn::ResourcePanel(isRu());resources->setObjectName("PluginResources");
    resources->setStyleSheet("QWidget#PluginResources{background:#0c1520;border:1px solid #263244;border-radius:8px;}");
    resources->setUsage(usage_);resourcePanel_=resources;outer->addWidget(resources);
    outer->addStretch();return body;
}

QString StreamDock::donationSummaryText() const
{
    const auto summary=donation_.sessionSummary();
    if(!summary.count)return !store_.current.donations.enabled?uiText("Модуль выключен","Module off"):
        summary.started?uiText("Пока нет донатов","No donations yet"):uiText("Ожидаем эфир","Waiting for stream");
    const QLocale locale=isRu()?QLocale(QLocale::Russian):QLocale(QLocale::English);
    QString result=uiText("Получено: %1","Received: %1").arg(qulonglong(summary.count));
    if(summary.count>summary.unpriced)result+=QString::fromUtf8(" · ")+(summary.converted?QString::fromUtf8("≈"):QString())+
        locale.toString(summary.rub,'f',std::floor(summary.rub)==summary.rub?0:2)+QString::fromUtf8(" ₽");
    if(summary.unpriced)result+=uiText(" · без курса: %1"," · unpriced: %1").arg(qulonglong(summary.unpriced));
    return result;
}

QWidget* StreamDock::buildConnections()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto* global = card(&v);
    auto* all = new StreamSwitch(uiText("Анонсировать начало стрима", "Announce stream start"));
    all->setChecked(store_.current.enabled);
    connect(all, &QCheckBox::toggled, this, [this](bool enabled) {store_.current.enabled=enabled;changeSettings();});
    v->addWidget(all);
    outer->addWidget(global);

    auto* telegramCard = card(&v);
    serviceSection(v,"Telegram");
    auto* tg = new StreamSwitch(uiText("Отправлять в Telegram", "Send to Telegram"));
    tg->setChecked(store_.current.telegramEnabled);
    connect(tg, &QCheckBox::toggled, this, [this](bool val){store_.current.telegramEnabled=val;changeSettings();});
    v->addWidget(tg);
    v->addWidget(label(uiText("1. Токен из @BotFather", "1. Token from @BotFather"), "Hint"));
    auto* token = new QLineEdit(store_.current.telegramToken);
    token->setObjectName("TelegramToken");
    token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText("123456789:... (BotFather)");
    connect(token, &QLineEdit::textChanged, this, [this](const QString& s) {store_.current.telegramToken=s;changeSettings();});
    auto* showToken = btn(uiText("Показать", "Show"));
    connect(showToken, &QPushButton::clicked, this, [token] {token->setEchoMode(token->echoMode()==QLineEdit::Password ? QLineEdit::Normal : QLineEdit::Password);});
    rowWith(v, token, showToken);
    v->addWidget(label(uiText("2. Канал для уведомлений", "2. Notification channel"), "Hint"));
    auto* chat = new QLineEdit(store_.current.telegramChat);
    chat->setPlaceholderText(uiText("@канал или ссылка t.me/…", "@channel or t.me/… link"));
    chat->setToolTip(uiText("Публичный: @имя или ссылка. Приватный: ссылка на сообщение t.me/c/… или ID -100…",
        "Public: @name or link. Private: t.me/c/… message link or -100… ID."));
    connect(chat, &QLineEdit::textChanged, this, [this](const QString& s){store_.current.telegramChat=telegramChannel(s);changeSettings();});
    v->addWidget(chat);
    auto* channelHint=label(QString(),"Hint");
    channelHint->setToolTip(uiText("Укажите имя канала, а не личный ник или имя бота. Бот должен быть администратором с правом публикации.",
        "Use the channel name, not a personal username or bot name. The bot must be an administrator allowed to post."));
    auto updateChannelHint=[this,channelHint](const QString& value) {
        const QString normalized=telegramChannel(value);
        const bool valid=QRegularExpression("^(?:-?[0-9]+|@[A-Za-z0-9_]{5,})$").match(normalized).hasMatch();
        channelHint->setText(value.trimmed().isEmpty()
            ? uiText("Бот должен быть администратором канала.", "The bot must be a channel administrator.")
            : valid ? uiText("Получатель: ", "Recipient: ")+normalized
            : uiText("Нужен @канал или ссылка на сообщение. Ссылка-приглашение не подходит.",
                     "Use @channel or a message link. Invite links are not supported."));
    };
    connect(chat,&QLineEdit::textChanged,channelHint,updateChannelHint);
    updateChannelHint(chat->text());
    v->addWidget(channelHint);
    auto* h1 = new QHBoxLayout;
    auto* testTg = btn(uiText("3. Отправить тест", "3. Send test"));
    testTg->setEnabled(!testingTelegram_ && validTelegram(store_.current.telegramToken,store_.current.telegramChat));
    auto updateTelegramTest=[this,testTg] {testTg->setEnabled(!testingTelegram_ && validTelegram(store_.current.telegramToken,store_.current.telegramChat));};
    connect(token,&QLineEdit::textChanged,testTg,updateTelegramTest);
    connect(chat,&QLineEdit::textChanged,testTg,updateTelegramTest);
    connect(testTg, &QPushButton::clicked, this, [this] {testChannel(Channel::Telegram);});
    auto* helpTg = btn(uiText("Как настроить?", "Setup guide"));
    connect(helpTg, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this,uiText("Подключение Telegram", "Connect Telegram"),uiText(
            "1. Откройте @BotFather в Telegram. Создайте бота командой /newbot или выберите своего через /mybots. Скопируйте токен сюда, в поле плагина.\n\n"
            "2. В настройках канала добавьте бота в администраторы. Достаточно права «Публикация сообщений».\n\n"
            "3. Публичный канал: вставьте @имя или ссылку t.me/имя.\n"
            "Приватный канал: скопируйте ссылку на любое сообщение внутри канала (t.me/c/…) и вставьте её. Плагин определит ID. Ссылка-приглашение t.me/+… не подходит.\n\n"
            "4. Включите Telegram и нажмите «Отправить тест». Трансляцию запускать не нужно.\n\n"
            "Токен вводите только здесь. Не отправляйте его в чат или GitHub.",
            "1. Open @BotFather in Telegram. Create a bot with /newbot or select yours with /mybots. Paste its token into the plugin.\n\n"
            "2. Add the bot as a channel administrator with permission to post messages.\n\n"
            "3. Public channel: paste @name or t.me/name.\n"
            "Private channel: paste a link to a message inside the channel (t.me/c/…). The plugin extracts the ID. Invite links t.me/+… are not supported.\n\n"
            "4. Enable Telegram and click Send test. No livestream is needed.\n\n"
            "Enter the token locally only; never share it in chat or GitHub."));
    });
    h1->addWidget(testTg); h1->addWidget(helpTg);
    v->addLayout(h1);
    if (!transientTelegram_.isEmpty()) v->addWidget(label(transientTelegram_,"Hint"));
    outer->addWidget(telegramCard);

    auto* discordCard = card(&v);
    serviceSection(v,"Discord");
    auto* dc = new StreamSwitch(uiText("Отправлять в Discord", "Send to Discord"));
    dc->setChecked(store_.current.discordEnabled);
    connect(dc, &QCheckBox::toggled, this, [this](bool val){store_.current.discordEnabled=val;changeSettings();});
    v->addWidget(dc);
    auto* everyone=new StreamSwitch(uiText("Упоминать @everyone","Mention @everyone"));
    everyone->setChecked(store_.current.discordEveryone);
    everyone->setToolTip(uiText("Добавлять @everyone в конец сообщений Discord: анонсы, донаты и ручные тесты.",
        "Append @everyone to Discord messages: stream announcements, donations and manual tests."));
    connect(everyone,&QCheckBox::toggled,this,[this](bool checked){
        store_.current.discordEveryone=checked;changeSettings();
    });
    v->addWidget(everyone);

    v->addWidget(label("Webhook URL", "Hint"));
    auto* hook = new QLineEdit(store_.current.discordWebhook);
    hook->setObjectName("DiscordWebhook");
    hook->setEchoMode(QLineEdit::Password);
    hook->setPlaceholderText("https://discord.com/api/webhooks/...");
    connect(hook, &QLineEdit::textChanged, this, [this](const QString& s){store_.current.discordWebhook=s;changeSettings();});
    auto* showHook = btn(uiText("Показать", "Show"));
    connect(showHook, &QPushButton::clicked, this, [hook] {hook->setEchoMode(hook->echoMode()==QLineEdit::Password ? QLineEdit::Normal : QLineEdit::Password);});
    rowWith(v, hook, showHook);
    auto* h2 = new QHBoxLayout;
    auto* testDc = btn(uiText("Отправить тест", "Send test"));
    testDc->setEnabled(!testingDiscord_ && validDiscord(store_.current.discordWebhook));
    connect(hook,&QLineEdit::textChanged,testDc,[this,testDc]{testDc->setEnabled(!testingDiscord_ && validDiscord(store_.current.discordWebhook));});
    connect(testDc, &QPushButton::clicked, this, [this] {testChannel(Channel::Discord);});
    auto* helpDc = btn(uiText("Как создать?", "Setup guide"));
    connect(helpDc, &QPushButton::clicked, this, [] {QDesktopServices::openUrl(QUrl("https://support.discord.com/hc/en-us/articles/228383668-Intro-to-Webhooks"));});
    h2->addWidget(testDc); h2->addWidget(helpDc);
    v->addLayout(h2);
    if (!transientDiscord_.isEmpty()) v->addWidget(label(transientDiscord_,"Hint"));
    outer->addWidget(discordCard);

    auto* urls = card(&v);
    section(v,uiText("Ссылки на трансляцию", "Stream links"));
    v->addWidget(label("YouTube", "Hint"));
    auto* yt = new QLineEdit(store_.current.youtubeUrl);
    yt->setObjectName("YouTubeUrl");
    yt->setPlaceholderText("https://www.youtube.com/@channel/live");
    connect(yt, &QLineEdit::textChanged, this, [this](const QString& s) {store_.current.youtubeUrl=s;changeSettings();});
    v->addWidget(yt);
    v->addWidget(label("Twitch", "Hint"));
    auto* tw = new QLineEdit(store_.current.twitchUrl);
    tw->setObjectName("TwitchUrl");
    tw->setPlaceholderText("https://www.twitch.tv/channel");
    connect(tw, &QLineEdit::textChanged, this, [this](const QString& s) {store_.current.twitchUrl=s;changeSettings();});
    v->addWidget(tw);
    outer->addWidget(urls);
    outer->addStretch();
    return body;
}

QWidget* StreamDock::buildMessage()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto* templ = card(&v);
    templ->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    v->setAlignment(Qt::AlignTop);
    section(v, uiText("Шаблон уведомления", "Notification template"));
    v->addWidget(label(uiText("Поддерживаются {youtube} и {twitch}", "Supports {youtube} and {twitch}"), "Hint"));
    auto* edit = new QTextEdit;
    edit->setFixedHeight(store_.current.compact?92:120);
    edit->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    edit->setPlainText(store_.current.message);
    connect(edit, &QTextEdit::textChanged, this, [this,edit] {store_.current.message=edit->toPlainText();changeSettings();});
    v->addWidget(edit);
    auto* tokens = new QHBoxLayout;
    for (const QString& t : {QString("{youtube}"), QString("{twitch}")}) {
        auto* button = btn(t);
        connect(button, &QPushButton::clicked, this, [edit,t] {edit->insertPlainText(t);});
        tokens->addWidget(button);
    }
    v->addLayout(tokens);
    outer->addWidget(templ);

    auto* delay = card(&v);
    delay->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    v->setAlignment(Qt::AlignTop);
    section(v,uiText("Задержка отправки", "Notification delay"));
    v->addWidget(label(uiText("После начала эфира, включая Aitum", "After going live, including Aitum"), "Hint"));
    auto* delayRow=new QHBoxLayout;delayRow->setSpacing(4);
    for(int seconds:{60,90,120}) {
        auto* choice=btn(QString::number(seconds)+uiText(" сек"," sec"));
        choice->setCheckable(true);choice->setChecked(store_.current.sendDelay==seconds);
        if(choice->isChecked())choice->setObjectName("ActiveNav");
        connect(choice,&QPushButton::clicked,this,[this,seconds]{
            store_.current.sendDelay=seconds;changeSettings();rebuild();
        });
        delayRow->addWidget(choice,1);
    }
    v->addLayout(delayRow);
    outer->addWidget(delay);

    auto* prev = card(&v);
    prev->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    v->setAlignment(Qt::AlignTop);
    section(v,uiText("Предпросмотр", "Preview"));
    QString err;
    const QString preview = prepareMessage(store_.current, &err);
    auto* livePreview = label(err.isEmpty() ? preview : (uiText("Ошибка: ", "Error: ")+err),err.isEmpty()?"":"Bad");
    v->addWidget(livePreview);
    connect(edit, &QTextEdit::textChanged, this, [this, livePreview] {
        QString problem;
        QString val = prepareMessage(store_.current, &problem);
        livePreview->setText(problem.isEmpty() ? val : uiText("Ошибка: ", "Error: ") + problem);
    });
    auto* refresh = btn(uiText("Обновить предпросмотр", "Refresh preview"));
    connect(refresh, &QPushButton::clicked, this, [this]{rebuild();});
    v->addWidget(refresh);
    outer->addWidget(prev);
    outer->addStretch();
    return body;
}

QWidget* StreamDock::buildDonations()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto& s = store_.current.donations;
    auto addToggle=[&](QVBoxLayout* l,const QString& title,bool& target) {
        auto* c=new StreamSwitch(title);
        c->setChecked(target);
        connect(c,&QCheckBox::toggled,this,[this,value=&target](bool checked){*value=checked;changeSettings();});
        l->addWidget(c);
    };
    auto* master=card(&v);
    section(v,uiText("Уведомления о донатах", "Donation notifications"));
    addToggle(v,uiText("Включить модуль", "Enable donation module"),s.enabled);
    outer->addWidget(master);

    if(donationPage_==DonationPage::Services) {
    auto addService=[&](const QString& title,bool& enabled,QString& token,QLabel*& statusLabel,
                        const QString& status,std::function<void()> test){
        QVBoxLayout* l;
        auto* box=card(&l);
        serviceSection(l,title);
        addToggle(l,uiText("Подключить сервис", "Enable service"),enabled);
        statusLabel=label(serviceStatus(status,isRu()),"Hint");
        l->addWidget(statusLabel);
        auto* edit=new QLineEdit(token);edit->setEchoMode(QLineEdit::Password);
        edit->setPlaceholderText(title=="DonatePay"
            ?uiText("API-ключ DonatePay","DonatePay API key")
            :uiText("OAuth-токен DonationAlerts","DonationAlerts OAuth token"));
        edit->setAccessibleName(title=="DonatePay"?uiText("API-ключ DonatePay","DonatePay API key"):
            uiText("OAuth-токен DonationAlerts","DonationAlerts OAuth token"));
        connect(edit,&QLineEdit::textChanged,this,[this,value=&token](const QString& text){*value=text;changeSettings();});
        auto* reveal=btn(uiText("Показать", "Show"));
        connect(reveal,&QPushButton::clicked,this,[edit]{edit->setEchoMode(edit->echoMode()==QLineEdit::Password?QLineEdit::Normal:QLineEdit::Password);});
        rowWith(l,edit,reveal);
        auto* verify=btn(uiText("Проверить подключение", "Check connection"));
        connect(verify,&QPushButton::clicked,this,[this,title,test] {
            const auto& settings=store_.current.donations;
            const bool donatePay=title=="DonatePay";
            QString reason;
            if(!settings.enabled)reason=uiText("Включите модуль донатов вверху этой страницы.","Enable the donation module at the top of this page.");
            else if(!(donatePay?settings.dpEnabled:settings.daEnabled))reason=uiText("Включите «Подключить сервис» в этой карточке.","Turn on Enable service in this card.");
            else if((donatePay?settings.dpToken:settings.daToken).isEmpty())reason=uiText("Сначала введите ключ сервиса в поле этой карточки.","Enter the service key in this card first.");
            else if(donatePay && !settings.legacyApiOptIn)reason=uiText("Разрешите экспериментальный API DonatePay внизу этой карточки.","Allow the experimental DonatePay API at the bottom of this card.");
            else if(settings.onlyLive && !broadcasting_)reason=uiText(
                "В разделе «Отправка» включено «Только во время стрима».\nНачните эфир в OBS/Aitum или выключите это ограничение для проверки.",
                "Only while streaming is enabled under Delivery.\nGo live with OBS/Aitum or turn this restriction off to check the connection.");
            if(!reason.isEmpty()) {QMessageBox::information(this,title,reason);return;}
            test();
        });
        auto* help=btn("?");help->setFixedWidth(28);
        help->setAccessibleName(uiText("Где взять ключ ","Where to get a key: ")+title);
        help->setToolTip(help->accessibleName());
        connect(help,&QPushButton::clicked,this,[title] {
            QDesktopServices::openUrl(QUrl(title=="DonatePay"?"https://donatepay.ru/page/api":
                "https://github.com/DIMI0007/stream-notify/blob/main/README_DONATIONS_RU.md#donationalerts"));
        });
        rowWith(l,verify,help);
        if(title=="DonatePay") {
            addToggle(l,uiText("Разрешить экспериментальный API","Allow experimental API"),s.legacyApiOptIn);
            auto* note=label(uiText("Экспериментально • опрос каждые 30 сек.","Experimental • polling every 30 sec."),"Hint");
            note->setToolTip(uiText("Совместимость API и приём реальных донатов требуют проверки на вашем аккаунте.",
                "API compatibility and live donation ingestion must be verified on your account."));
            l->addWidget(note);
        }
        outer->addWidget(box);
    };
    addService("DonationAlerts",s.daEnabled,s.daToken,daStatusLabel_,donation_.daStatus(),[this]{donationConfigure_.stop();donation_.configure();donation_.checkDonationAlerts();});
    addService("DonatePay",s.dpEnabled,s.dpToken,dpStatusLabel_,donation_.dpStatus(),[this]{donationConfigure_.stop();donation_.configure();donation_.checkDonatePay();});
    }
    if(donationPage_==DonationPage::Delivery) {
    auto* rules=card(&v);
    section(v,uiText("Правила отправки","Delivery rules"));
    addToggle(v,uiText("Только во время стрима","Only while streaming"),s.onlyLive);
    addToggle(v,uiText("Публиковать сообщение донатера","Include donor message"),s.includeDonorMessage);
    v->addWidget(label(uiText("Донаты и анонсы стрима включаются независимо.","Donation and stream announcements are enabled independently."),"Hint"));
    outer->addWidget(rules);

    auto* destinations=card(&v);
    section(v,uiText("Куда отправлять", "Destinations"));
    v->addWidget(label(uiText("Используются подключения из раздела «Связь».","Uses the connections configured in Connections."),"Hint"));
    addToggle(v,QString("Telegram"),s.toTelegram);
    addToggle(v,QString("Discord"),s.toDiscord);
    outer->addWidget(destinations);

    }
    if(donationPage_==DonationPage::Amounts) {
    auto* thresholds=card(&v);
    section(v,uiText("Пороговые суммы", "Donation thresholds"));
    v->addWidget(label(uiText("Все пороги — в рублях. Курсы сохраняются один раз, без автообновлений.",
        "All thresholds are in RUB. Rates are saved once, without automatic refresh."),"Hint"));
    auto* ratesLabel=label(serviceStatus(donation_.ratesStatus(),isRu()),"Hint");
    v->addWidget(ratesLabel);
    connect(&donation_,&DonationEngine::updated,ratesLabel,[this,ratesLabel]{ratesLabel->setText(serviceStatus(donation_.ratesStatus(),isRu()));});
    auto* ratesButton=btn(uiText("Загрузить / обновить курсы", "Load / refresh rates"));
    connect(ratesButton,&QPushButton::clicked,this,[this]{donation_.fetchRates(true);});
    v->addWidget(ratesButton);
    auto* thresholdEditors=new QWidget;
    auto* editorLayout=column(thresholdEditors,5,0);
    auto populateThresholds=[this,editorLayout]() {
        while(QLayoutItem* item=editorLayout->takeAt(0)) {if(item->widget())delete item->widget();delete item;}
        const QString code="RUB";
        auto& thresholds=store_.current.donations.thresholds[code];
        if(thresholds.size()!=3)thresholds={100,500,2000};
        const QStringList names={uiText("💰 Обычный", "💰 Standard"),uiText("🔥 Крупный", "🔥 Large"),uiText("👑 Мега", "👑 Mega")};
        for(int i=0;i<3;++i){
            auto* h=new QHBoxLayout;
            h->addWidget(label(names.at(i)),1);
            auto* amount=new QDoubleSpinBox;
            amount->setRange(0.01,999999999);
            amount->setDecimals(2);
            amount->setValue(thresholds[i]);
            amount->setSuffix(" "+code);
            connect(amount,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this,code,i,amount](double val){
                auto& a=store_.current.donations.thresholds[code];
                QVector<double> next=a;next[i]=val;
                if(next[0]>0&&next[1]>next[0]&&next[2]>next[1]) {
                    a=next;amount->setStyleSheet("");changeSettings();
                } else amount->setStyleSheet("border:1px solid #e7a34d;");
            });
            h->addWidget(amount);
            editorLayout->addLayout(h);
        }
        editorLayout->addWidget(label(uiText("Пороги должны строго возрастать. Изменения применяются только к новым донатам.",
            "Thresholds must strictly increase. Changes apply to future donations only."),"Hint"));
    };
    populateThresholds();v->addWidget(thresholdEditors);
    outer->addWidget(thresholds);

    }
    if(donationPage_==DonationPage::Templates) {
    auto* templates=card(&v);
    section(v,uiText("Шаблоны трёх уровней", "Three level templates"));
    auto* select=new QComboBox;
    select->addItem(uiText("💰 Обычный", "💰 Standard"),0);
    select->addItem(uiText("🔥 Крупный", "🔥 Large"),1);
    select->addItem(uiText("👑 Мега", "👑 Mega"),2);
    v->addWidget(select);
    auto* editor=new QTextEdit;
    editor->setFixedHeight(store_.current.compact?96:132);
    editor->setPlainText(s.templates.at(0));
    v->addWidget(editor);
    v->addWidget(label("{donor} {amount} {currency} {message} {service} {stream_url}","Hint"));
    auto* preview=label("","Hint");
    preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto updatePreview=[this,select,preview]() {
        Donation test;test.service="DonationAlerts";test.id="preview";test.donor=uiText("Тестовый зритель","Test donor");
        test.message=uiText("Спасибо за стрим!","Thanks for the stream!");test.currency="RUB";
        const auto values=store_.current.donations.thresholds.value("RUB");
        test.amount=values.size()==3?values.at(select->currentIndex()):100;
        const QString url=!store_.current.youtubeUrl.isEmpty()?store_.current.youtubeUrl:store_.current.twitchUrl;
        preview->setText(donationMessage(test,store_.current.donations,select->currentIndex(),url));
    };
    connect(select,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this,editor,updatePreview](int idx){
        editor->setPlainText(store_.current.donations.templates.at(idx));updatePreview();
    });
    connect(editor,&QTextEdit::textChanged,this,[this,editor,select,updatePreview]{
        store_.current.donations.templates[select->currentIndex()]=editor->toPlainText();
        changeSettings();updatePreview();
    });
    auto* templateButtons=new QHBoxLayout;
    auto* reset=btn(uiText("По умолчанию", "Reset template"));
    connect(reset,&QPushButton::clicked,this,[this,editor,select]{
        const DonationSettings defaults;
        editor->setPlainText(defaults.templates.at(select->currentIndex()));
    });
    auto* test=btn(uiText("Отправить тест", "Send test"));
    connect(test,&QPushButton::clicked,this,[this,select]{donation_.previewTest(select->currentIndex());});
    templateButtons->addWidget(reset);templateButtons->addWidget(test);
    v->addLayout(templateButtons);
    section(v,uiText("Предпросмотр", "Preview"));
    v->addWidget(preview);updatePreview();
    outer->addWidget(templates);

    auto* journal=card(&v);
    section(v,uiText("Журнал донатов и доставки", "Donation and delivery log"));
    const auto recentDonations=donation_.journal();
    donationJournalLabel_=label(recentDonations.isEmpty()?uiText("Пока нет донатов","No donations yet"):recentDonations.mid(0,6).join("\n"),"Hint");
    donationJournalLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(donationJournalLabel_);
    auto* fullLog=btn(uiText("Весь журнал","Full log"));
    connect(fullLog,&QPushButton::clicked,this,[this] {
        QMessageBox box(this);box.setWindowTitle(uiText("Журнал донатов","Donation log"));
        const auto entries=donation_.journal();
        box.setText(entries.isEmpty()?uiText("Пока нет донатов","No donations yet"):
            uiText("Последние записи доставки. Нажмите «Показать подробности».","Recent delivery records. Open Show Details."));
        if(!entries.isEmpty())box.setDetailedText(entries.join("\n"));
        box.exec();
    });
    v->addWidget(fullLog);
    outer->addWidget(journal);
    }
    outer->addStretch();
    return body;
}

QWidget* StreamDock::buildInterface()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto* locale = card(&v);
    section(v,uiText("Язык интерфейса", "Interface language"));
    auto* languageRow=new QHBoxLayout;languageRow->setSpacing(4);
    const struct {const char* code;QString title;} languages[]={
        {"auto",uiText("Авто","Auto")},{"ru",QString::fromUtf8("Русский")},{"en",QString("English")}
    };
    for(const auto& item:languages) {
        auto* choice=btn(item.title);choice->setCheckable(true);
        choice->setChecked(store_.current.locale==item.code);
        if(choice->isChecked())choice->setObjectName("ActiveNav");
        const QString code=QString::fromLatin1(item.code);
        connect(choice,&QPushButton::clicked,this,[this,code] {
            store_.current.locale=code;changeSettings();rebuild();
        });
        languageRow->addWidget(choice,1);
    }
    v->addLayout(languageRow);
    outer->addWidget(locale);

    auto* appearance = card(&v);
    section(v,uiText("Вид панели", "Appearance"));
    auto* compact = new StreamSwitch(uiText("Компактный режим", "Compact mode"));
    compact->setChecked(store_.current.compact);
    connect(compact, &QCheckBox::toggled, this, [this](bool b){store_.current.compact=b;changeSettings();rebuild();});
    v->addWidget(compact);
    auto* show = new StreamSwitch(uiText("Показывать статус в заголовке", "Show status in header"));
    show->setChecked(store_.current.showStatus);
    connect(show, &QCheckBox::toggled, this, [this](bool b){store_.current.showStatus=b;changeSettings();rebuild();});
    v->addWidget(show);
    section(v,uiText("Длительность экрана результата", "Result screen duration"));
    auto* result = new QComboBox;
    result->addItem(uiText("30 секунд", "30 seconds"),30);
    result->addItem(uiText("45 секунд — по умолчанию", "45 seconds — default"),45);
    result->addItem(uiText("60 секунд", "60 seconds"),60);
    result->setCurrentIndex(result->findData(store_.current.resultSeconds));
    connect(result, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this,result](int){store_.current.resultSeconds=result->currentData().toInt();changeSettings();});
    v->addWidget(result);
    outer->addWidget(appearance);

    auto* history = card(&v);
    section(v,uiText("Журнал событий (последние 100)", "Event log (last 100)"));
    for (const QString& msg : store_.current.events.mid(0,12)) v->addWidget(label(msg,"Hint"));
    if(store_.current.events.isEmpty()) v->addWidget(label(uiText("Пока пусто", "No events yet"),"Hint"));
    auto* clear = btn(uiText("Очистить журнал", "Clear log"));
    connect(clear, &QPushButton::clicked, this, [this] {store_.current.events.clear();changeSettings();rebuild();});
    v->addWidget(clear);
    outer->addWidget(history);

    auto* about = card(&v);
    section(v,uiText("О плагине", "About"));
    v->addWidget(label("Stream Notify 1.0.0"));
    v->addWidget(label(uiText("Анонсы стримов и донатов.\nНастройки хранятся на этом ПК.", "Stream and donation announcements.\nSettings are stored on this PC."), "Hint"));
    outer->addWidget(about);
    outer->addStretch();
    return body;
}

// Ring updates once per countdown tick; the local pulse runs only in the final ten visible seconds.
class CountdownRing final : public QWidget {
public:
    explicit CountdownRing(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(160,160);setMaximumSize(280,280);
        QSizePolicy policy(QSizePolicy::Preferred,QSizePolicy::Preferred);policy.setHeightForWidth(true);setSizePolicy(policy);
        // Local repaint only, at 30 fps for the last ten visible seconds.
        // Sending still follows StreamDock's elapsed timer, independently.
        pulseTimer_.setParent(this);pulseTimer_.setObjectName("CountdownPulseTimer");
        pulseTimer_.setInterval(33);
        connect(&pulseTimer_,&QTimer::timeout,this,[this]{update();});
    }
    QSize sizeHint() const override {return QSize(236,236);}
    QSize minimumSizeHint() const override {return QSize(160,160);}
    bool hasHeightForWidth() const override {return true;}
    int heightForWidth(int width) const override {return std::clamp(width,160,280);}
    void setValues(int remaining, int total) {
        const int next=std::max(0,remaining);
        if(next!=remaining_ || !beat_.isValid()) beat_.start();
        remaining_=next;total_=std::max(1,total);
        refreshAnimation();update();
    }
protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);refreshAnimation();
    }
    void hideEvent(QHideEvent* event) override {
        pulseTimer_.stop();QWidget::hideEvent(event);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int side = std::min(width(), height()) - 17;
        const QRectF r((width()-side)/2.0, (height()-side)/2.0, side, side);
        QRadialGradient halo(r.center(),side/2.0);halo.setColorAt(0,QColor("#0d2635"));halo.setColorAt(1,QColor("#080f18"));
        p.setPen(Qt::NoPen);p.setBrush(halo);p.drawEllipse(r);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#26364d"), 10, Qt::SolidLine, Qt::RoundCap));
        p.drawEllipse(r);
        p.setPen(QPen(QColor("#24dcf8"), 10, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r,90*16, -int(360.0*16*remaining_/total_));
        const double angle=(90.0-360.0*remaining_/total_)*3.14159265358979323846/180.0;
        const QPointF tip(r.center().x()+r.width()/2*std::cos(angle),r.center().y()-r.height()/2*std::sin(angle));
        p.setPen(Qt::NoPen);p.setBrush(QColor("#6ef2ff"));p.drawEllipse(tip,5,5);
        // One soft heartbeat per second. No layout changes, flashing or effects.
        const double phase=beat_.isValid()?(beat_.elapsed()%1000)/1000.0:0.0;
        const double wave=lastSeconds()?std::pow(std::sin(phase*3.14159265358979323846),2):0.0;
        const qreal scale=1.0+0.06*wave;
        const QColor accent=QColor("#24dcf8").lighter(100+int(18*wave));
        p.save();p.translate(r.center());p.scale(scale,scale);p.translate(-r.center());
        p.setPen(QColor("#e4f3ff"));
        QFont f=font(); f.setPixelSize(std::clamp(int(side*0.22),32,60)); f.setBold(true); p.setFont(f);
        const QString minutes=QString("%1:").arg(remaining_/60,2,10,QChar('0'));
        const QString seconds=QString("%1").arg(remaining_%60,2,10,QChar('0'));
        const QFontMetricsF metrics(f);
        const qreal start=(width()-metrics.horizontalAdvance(minutes+seconds))/2;
        const qreal baseline=height()/2.0+metrics.ascent()/2;
        p.drawText(QPointF(start,baseline),minutes);
        p.setPen(accent);
        p.drawText(QPointF(start+metrics.horizontalAdvance(minutes),baseline),seconds);
        p.setBrush(Qt::NoBrush);p.setPen(QPen(accent,2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const qreal cx=width()/2.0,cy=height()/2.0-side*0.27;
        p.drawEllipse(QPointF(cx,cy),11,11);
        p.drawLine(QPointF(cx,cy),QPointF(cx,cy-6));
        p.drawLine(QPointF(cx,cy),QPointF(cx+5,cy+3));
        p.restore();
    }
private:
    bool lastSeconds() const {return remaining_>0 && remaining_<=10;}
    void refreshAnimation() {
        if(lastSeconds() && isVisible()) {if(!pulseTimer_.isActive())pulseTimer_.start();}
        else pulseTimer_.stop();
    }
    int remaining_ = 60;
    int total_ = 60;
    QTimer pulseTimer_;
    QElapsedTimer beat_;
};

QWidget* StreamDock::buildCountdown()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    body->setObjectName("PageBody");
    auto* v = column(body, 12, 7);
    v->addSpacing(12);
    auto* heading = label(uiText("До отправки уведомлений", "Until notifications are sent"),"Section");
    heading->setAlignment(Qt::AlignCenter);

    auto* ring = new CountdownRing;
    ring_ = ring;
    ring->setValues(std::max(0, session_.delay - int(elapsed_.elapsed()/1000)),session_.delay);
    v->addWidget(ring, 0, Qt::AlignHCenter);
    v->addWidget(heading);
    auto* help = label(uiText("Уведомления будут отправлены после таймера", "Messages will be sent when the timer expires"),"Hint");
    help->setAlignment(Qt::AlignCenter);
    v->addWidget(help);

    QVBoxLayout* channels;
    auto* box = card(&channels);
    section(channels,uiText("Активные каналы", "Active channels"));
    auto addChannel=[this,channels](const QString& name,bool enabled) {
        auto* row=new QHBoxLayout;row->setSpacing(7);
        row->addWidget(new ServiceBadge(name));
        row->addWidget(label(name,"Section"),1);
        row->addWidget(label(enabled?uiText("● Вкл","● On"):uiText("Выключен","Off"),enabled?"Good":"Hint"));
        channels->addLayout(row);
    };
    addChannel("Telegram",session_.telegram);addChannel("Discord",session_.discord);
    channels->addWidget(label(uiText("Задержка: %1 сек.", "Delay: %1 sec.").arg(session_.delay),"Hint"));
    v->addWidget(box);
    auto* cancel = btn(uiText("Отменить отправку", "Cancel notification"));
    cancel->setIcon(streamNavIcon("stop",true));cancel->setIconSize(QSize(18,18));
    cancel->setObjectName("Cancel");
    connect(cancel, &QPushButton::clicked, this, [this] {cancelCurrent("User cancelled");});
    v->addWidget(cancel);
    auto* sub = label(uiText("Отмена действует только для текущего стрима", "Cancellation only affects this stream"),"Hint");
    sub->setAlignment(Qt::AlignCenter);
    v->addWidget(sub);
    v->addStretch(1);
    return body;
}

QWidget* StreamDock::buildSending()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* v = column(body, 15, 20);
    v->addStretch();
    auto* heading = label(uiText("Отправляем уведомления…", "Sending notifications…"), "Section");
    heading->setAlignment(Qt::AlignCenter);
    v->addWidget(heading);
    auto* sub = label(uiText("Telegram и Discord обрабатываются независимо", "Telegram and Discord are handled independently"),"Hint");
    sub->setAlignment(Qt::AlignCenter);
    v->addWidget(sub);
    auto add = [&](const QString& title, const Delivery& d) {
        QVBoxLayout* c;
        auto* b = card(&c);
        section(c,title);
        c->addWidget(label(statusName(d),"Hint"));
        v->addWidget(b);
    };
    add("Telegram",telegram_);
    add("Discord",discord_);
    v->addStretch();
    return body;
}

QString StreamDock::statusName(const Delivery& d) const
{
    sn::CpuScope snCpuScope;
    switch(d.status) {
        case Outcome::Disabled: return uiText("Отключён", "Disabled");
        case Outcome::NotConfigured: return uiText("Не настроен", "Not configured");
        case Outcome::Waiting: return uiText("Ожидает отправки", "Queued");
        case Outcome::Sending: return uiText("Отправляется…", "Sending…");
        case Outcome::Sent: return uiText("✓ Отправлено", "✓ Sent");
        case Outcome::Failed: return uiText("✕ Ошибка", "✕ Error");
        case Outcome::Unknown: return uiText("? Результат неизвестен", "? Unconfirmed");
        case Outcome::Cancelled: return uiText("Отменено", "Cancelled");
    }
    return {};
}
QWidget* StreamDock::buildResult()
{
    sn::CpuScope snCpuScope;
    const bool tgSent = telegram_.status == Outcome::Sent;
    const bool dcSent = discord_.status == Outcome::Sent;
    const bool allOk = (!session_.telegram || tgSent) && (!session_.discord || dcSent);
    const bool partial = (tgSent || dcSent) && !allOk;
    auto* body = new QWidget;
    body->setObjectName("PageBody");
    auto* v = column(body, 16, 13);
    v->addSpacing(16);
    auto* symbol=new ResultEmblem(allOk?0:partial?1:2);
    v->addWidget(symbol,0,Qt::AlignHCenter);
    auto* title = label(allOk ? uiText("Уведомления отправлены", "Notifications sent") :
       partial ? uiText("Отправлено частично", "Partially delivered") : uiText("Не удалось отправить", "Delivery failed"));
    title->setAlignment(Qt::AlignHCenter);
    QFont font = title->font(); font.setPointSize(15); font.setBold(true); title->setFont(font);
    title->setStyleSheet("color:#e7fff4;font-size:17px;font-weight:700;");
    v->addWidget(title);
    auto* summary=label(allOk?uiText("Рассылка успешно завершена","Notifications delivered"):
        uiText("Подробности по каждому каналу ниже","Details for each channel below"),"Hint");
    summary->setAlignment(Qt::AlignCenter);v->addWidget(summary);
    QVBoxLayout* results;
    auto* cardWidget = card(&results);
    auto detail = [this,results](const QString& name, const Delivery& d) {
        auto* row=new QHBoxLayout;row->setSpacing(7);
        row->addWidget(new ServiceBadge(name));row->addWidget(label(name,"Section"),1);
        row->addWidget(label(statusName(d),d.status==Outcome::Sent?"Good":"Hint"));
        results->addLayout(row);
        if (d.status == Outcome::Failed || d.status == Outcome::Unknown || d.status == Outcome::NotConfigured) {
            results->addWidget(label(d.detail,"Hint"));
        }
    };
    detail("Telegram",telegram_); detail("Discord",discord_);
    v->addWidget(cardWidget);
    auto* hint = label(uiText("Панель автоматически вернётся через %1 секунд", "The normal panel will return in %1 seconds")
                       .arg(session_.resultSeconds),"Hint");
    hint->setAlignment(Qt::AlignCenter);
    QVBoxLayout* returnLayout;
    auto* returnCard=card(&returnLayout);
    returnLayout->addWidget(hint);
    auto* progress=new QProgressBar;
    progress->setRange(0,session_.resultSeconds);progress->setValue(session_.resultSeconds);progress->setTextVisible(false);
    progress->setFixedHeight(6);
    progress->setStyleSheet("QProgressBar {background:#1b293b;border:0;border-radius:3px;} QProgressBar::chunk {background:#00db89;border-radius:3px;}");
    returnLayout->addWidget(progress);
    auto* back=btn(uiText("На основную","Back to Home"));back->setIcon(streamNavIcon("home"));
    connect(back,&QPushButton::clicked,this,[this]{resultTimer_.stop();page_=Page::Home;showPhase(Phase::Idle);});
    returnLayout->addWidget(back);
    v->addWidget(returnCard);
    auto* tick=new QTimer(progress);tick->setInterval(1000);
    connect(tick,&QTimer::timeout,progress,[this,progress,hint] {
        const int remaining=std::max(0,(resultTimer_.remainingTime()+999)/1000);
        progress->setValue(remaining);
        hint->setText(uiText("Возврат к панели через %1 сек.","Back to panel in %1 sec.").arg(remaining));
    });
    tick->start();
    v->addStretch();
    return body;
}

void StreamDock::updateCountdown()
{
    sn::CpuScope snCpuScope;
    if(phase_ != Phase::Counting) {countdown_.stop(); return;}
    const qint64 msRemaining = qint64(session_.delay)*1000 - elapsed_.elapsed();
    const int remain = int(std::max<qint64>(0, (msRemaining + 999) / 1000));
    if(ring_) ring_->setValues(remain,session_.delay);
    if (msRemaining <= 0) {
        countdown_.stop();
        dispatch();
    }
}

void StreamDock::refreshStreamingState()
{
    sn::CpuScope snCpuScope;
    if(shuttingDown_)return;
    const bool live=anyStreamingOutputActive();
    if(live==broadcasting_)return;
    if(live)streamStarted();else streamStopped();
}

void StreamDock::streamStarted()
{
    sn::CpuScope snCpuScope;
    if (shuttingDown_) return;
    broadcasting_ = true;
    donation_.setLive(true);
    // One notification session spans all concurrently active streaming outputs.
    if(alreadyHandled_) {rebuild();return;}
    alreadyHandled_ = true;
    announcementAttempted_=false;telegram_={};discord_={};
    resultTimer_.stop();
    ++generation_;
    if(!store_.current.enabled) {statusText_=uiText("Автоуведомления выключены", "Automatic notifications disabled");rebuild();return;}
    QString error;
    session_ = makeSnapshot(store_.current,&error);
    telegram_ = {session_.telegram ? Outcome::Waiting : Outcome::Disabled, {}, 0};
    discord_ = {session_.discord ? Outcome::Waiting : Outcome::Disabled, {}, 0};
    if(!session_.telegram && !session_.discord) {
        statusText_=uiText("Оба мессенджера отключены", "Both channels are disabled");rebuild();return;
    }
    const bool configured = (session_.telegram && validTelegram(session_.token,session_.chat)) ||
                            (session_.discord && validDiscord(session_.webhook));
    if(!configured) {statusText_=uiText("Сначала настройте Telegram или Discord", "Configure Telegram or Discord first");rebuild();return;}
    if (!error.isEmpty()) {
        statusText_=uiText("Ошибка шаблона: ", "Invalid template: ") + error;
        rebuild();return;
    }
    elapsed_.start();
    countdown_.start();
    store_.addEvent("STREAM_STARTED");
    showPhase(Phase::Counting);
}

void StreamDock::cancelCurrent(const QString& reason)
{
    sn::CpuScope snCpuScope;
    if(phase_ != Phase::Counting) return;
    countdown_.stop();
    telegram_.status = (telegram_.status == Outcome::Waiting) ? Outcome::Cancelled : telegram_.status;
    discord_.status = (discord_.status == Outcome::Waiting) ? Outcome::Cancelled : discord_.status;
    store_.addEvent("CANCELLED: " + reason);
    statusText_ = uiText("Уведомление отменено для текущего эфира", "Notification cancelled for this stream");
    showPhase(Phase::Cancelled);
    resultTimer_.start(3000);
}

void StreamDock::streamStopped()
{
    sn::CpuScope snCpuScope;
    if (shuttingDown_) return;
    broadcasting_ = false;
    donation_.setLive(false);
    alreadyHandled_ = false;
    if (phase_ == Phase::Counting) {cancelCurrent("Stream stopped"); return;}
    if (phase_ == Phase::Sending) {
        ++generation_;
        // The remote system might already have received a request. Never retry it.
        if (telegram_.status == Outcome::Sending) telegram_ = {Outcome::Unknown, "Stream stopped during request",telegram_.attempts};
        if (discord_.status == Outcome::Sending) discord_ = {Outcome::Unknown, "Stream stopped during request",discord_.attempts};
        if (telegram_.status == Outcome::Waiting) telegram_.status = Outcome::Cancelled;
        if (discord_.status == Outcome::Waiting) discord_.status = Outcome::Cancelled;
        sender_.abortAll();
        activePending_ = 0;
        complete();
        return;
    }
    rebuild();
}

void StreamDock::dispatch()
{
    sn::CpuScope snCpuScope;
    if (phase_ != Phase::Counting) return;
    if (!anyStreamingOutputActive() || !broadcasting_) {cancelCurrent("Stream inactive");return;}
    if (telegram_.status == Outcome::Waiting && !validTelegram(session_.token,session_.chat))
        telegram_ = {Outcome::NotConfigured,"Check BotFather token and Chat ID",0};
    if (discord_.status == Outcome::Waiting && !validDiscord(session_.webhook))
        discord_ = {Outcome::NotConfigured,"Check Discord webhook URL",0};
    announcementAttempted_=true;
    activePending_ = 0;
    if (telegram_.status == Outcome::Waiting) {telegram_.status = Outcome::Sending; ++activePending_;}
    if (discord_.status == Outcome::Waiting) {discord_.status = Outcome::Sending; ++activePending_;}
    showPhase(Phase::Sending);
    const quint64 key = generation_;
    auto submit = [this,key](Channel c) {
        sender_.send(c,session_,session_.message,[this,key,c](Delivery d) {
            if(shuttingDown_ || key != generation_ || phase_ != Phase::Sending) return;
            recordOutcome(c,d);
            --activePending_;
            if(activePending_ <= 0) complete();
            else rebuild();
        });
    };
    if (telegram_.status == Outcome::Sending) submit(Channel::Telegram);
    if (discord_.status == Outcome::Sending) submit(Channel::Discord);
    if(activePending_ == 0) complete();
}

void StreamDock::recordOutcome(Channel channel, const Delivery& delivery)
{
    sn::CpuScope snCpuScope;
    if (channel == Channel::Telegram) telegram_ = delivery;
    else discord_ = delivery;
    const QString name = channel == Channel::Telegram ? "TELEGRAM" : "DISCORD";
    store_.addEvent(name + ": " + (delivery.status == Outcome::Sent ? "SENT" :
                              delivery.status == Outcome::Unknown ? "UNKNOWN" : "FAILED"));
}
void StreamDock::complete()
{
    sn::CpuScope snCpuScope;
    countdown_.stop();
    store_.current.lastTelegram = statusName(telegram_);
    store_.current.lastDiscord = statusName(discord_);
    store_.current.lastTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    store_.addEvent("SESSION_FINISHED");
    statusText_=uiText("Последняя рассылка завершена", "Last delivery completed");
    showPhase(Phase::Result);
    resultTimer_.start(session_.resultSeconds*1000);
}

void StreamDock::testChannel(Channel channel)
{
    sn::CpuScope snCpuScope;
    const bool tg = channel == Channel::Telegram;
    if(tg ? testingTelegram_ : testingDiscord_) return;
    Snapshot snap = makeSnapshot(store_.current);
    if(tg ? !validTelegram(snap.token,snap.chat) : !validDiscord(snap.webhook)) {
        if(tg) transientTelegram_ = uiText("Неверный токен или ID чата", "Invalid token or chat ID");
        else transientDiscord_ = uiText("Неверный Webhook URL", "Invalid webhook URL");
        rebuild();return;
    }
    if(tg) {testingTelegram_=true;transientTelegram_=uiText("Проверяем…", "Testing…");}
    else {testingDiscord_=true;transientDiscord_=uiText("Проверяем…", "Testing…");}
    rebuild();
    const QString message = uiText("🧪 Stream Notify — тестовое сообщение", "🧪 Stream Notify — test message");
    testSender_.send(channel,snap,message,[this,channel](Delivery d) {
        if(shuttingDown_) return;
        const QString text = d.status == Outcome::Sent ? uiText("✓ Тест отправлен", "✓ Test sent") :
         uiText("⚠ Ошибка теста: ", "⚠ Test error: ") + d.detail;
        if(channel == Channel::Telegram) {testingTelegram_=false;transientTelegram_=text;}
        else {testingDiscord_=false;transientDiscord_=text;}
        store_.addEvent(QString(channel == Channel::Telegram ? "TEST_TELEGRAM " : "TEST_DISCORD ")+
                        (d.status == Outcome::Sent ? "OK" : "FAIL"));
        rebuild();
    });
}
void StreamDock::shutdown()
{
    sn::CpuScope snCpuScope;
    if(shuttingDown_) return;
    shuttingDown_ = true;
    countdown_.stop();
    streamStateTimer_.stop();
    telemetryTimer_.stop();monitor_.clear();
    resultTimer_.stop();
    ++generation_;
    sender_.abortAll();
    testSender_.abortAll();
    donation_.stop();
    store_.flush();
}

void StreamDock::refreshTelemetry() {
    sn::CpuScope snCpuScope;
    sn::CpuScope scope;
    if(shuttingDown_)return;
    monitor_.sample(telemetryClock_.elapsed());
    uint64_t buffers=usage_.buffers;
    const bool show=phase_==Phase::Idle && page_==Page::Home && isVisible();
    if(show) {
        sn::BufferCensus census;
        sn::account(census,store_.current);sn::account(census,session_);
        for(const auto* value:{&statusText_,&transientTelegram_,&transientDiscord_,
            &telegram_.detail,&discord_.detail})census.add(*value);
        sender_.account(census);testSender_.account(census);donation_.account(census);monitor_.account(census);
        for(auto* label:findChildren<QLabel*>())census.add(label->text());
        for(auto* edit:findChildren<QLineEdit*>())census.add(edit->text());
        buffers=census.bytes();
    }
    usage_=usageSampler_.sample(buffers);
    if(show){if(outputPanel_)outputPanel_->setOutputs(monitor_.views());if(resourcePanel_)resourcePanel_->setUsage(usage_);}
}
