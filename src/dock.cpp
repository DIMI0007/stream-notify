#include "dock.hpp"
#include <obs.h>
#include <obs-frontend-api.h>
#include <QPainter>
#include <QPaintEvent>
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
#include <QProgressBar>
#include <QTextBrowser>
#include <cmath>
#include <algorithm>

namespace {
const char* css = R"(
QWidget#StreamRoot { background: #0b1320; color: #dfe8ff; }
QWidget#Card { background: #131f31; border: 1px solid #283750; border-radius: 12px; }
QLabel { color: #dfe8ff; background: transparent; }
QLabel#Hint { color: #9baccc; }
QLabel#Title { font-weight: 750; font-size: 21px; }
QLabel#Section { color: #d6e5fa; font-size: 15px; font-weight: 700; }
QLabel#Good { color: #07df91; font-weight: 700; }
QLabel#Bad { color: #ff8484; font-weight: 700; }
QLabel#Count { color: #59dfff; font-weight: 750; font-size: 57px; }
QPushButton { color: #e1ebff; background: #22334c; border: 1px solid #445674; border-radius: 8px; min-height: 26px; padding: 5px 11px; }
QPushButton:hover { background: #2f4565; }
QPushButton#ActiveNav { background: #203966; border-color: #4969f2; font-weight: 700; }
QPushButton#Cancel { background: #16395b; border-color: #40b7ff; font-weight: 700; min-height: 42px; }
QLineEdit,QTextEdit,QComboBox { color: #e7eeff; background: #101a2b; border: 1px solid #344865; border-radius: 6px; padding: 7px; selection-background-color: #316ac4; }
QComboBox QAbstractItemView { background: #101a2b; color: #e7eeff; }
QCheckBox { color: #dce7fb; spacing: 10px; min-height: 24px; }
QCheckBox::indicator { width: 21px; height: 21px; }
QCheckBox::indicator:unchecked { border-radius: 11px; background: #344357; border: 1px solid #65748d; }
QCheckBox::indicator:checked { border-radius: 11px; background: #00d37d; border: 1px solid #2ff9a0; }
QScrollArea { border: 0; background: transparent; }
QScrollBar:vertical { border: none; background: #101929; width: 9px; }
QScrollBar::handle:vertical { background: #384d6c; border-radius: 4px; }
)";

QVBoxLayout* column(QWidget* w, int gap = 9, int padding = 12)
{
    auto* v = new QVBoxLayout(w);
    v->setSpacing(gap);
    v->setContentsMargins(padding,padding,padding,padding);
    return v;
}
void rowWith(QVBoxLayout* v, QWidget* first, QWidget* last)
{
    auto* h = new QHBoxLayout();
    h->setSpacing(7);
    h->addWidget(first, 1);
    if(last) h->addWidget(last);
    v->addLayout(h);
}
QPushButton* btn(const QString& text, QWidget* parent = nullptr)
{
    auto* p = new QPushButton(text, parent);
    p->setCursor(Qt::PointingHandCursor);
    return p;
}
QLabel* paragraph(const QString& text, QWidget* parent = nullptr)
{
    auto* l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}
}

StreamDock::StreamDock(QWidget* parent) : QWidget(parent), store_(this), sender_(this), testSender_(this), donation_(&store_,this)
{
    setObjectName("StreamRoot");
    setMinimumWidth(275);
    setStyleSheet(css);
    loadedCleanly_ = store_.load();
    if (!loadedCleanly_) statusText_ = store_.error();
    connect(&store_, &SettingsStore::persisted, this, &StreamDock::updateSaved);
    countdown_.setInterval(1000);
    connect(&countdown_, &QTimer::timeout, this, &StreamDock::updateCountdown);
    resultTimer_.setSingleShot(true);
    connect(&resultTimer_, &QTimer::timeout, this, [this] { showPhase(Phase::Idle); });
    root_ = column(this, 9, 9);
    donationConfigure_.setSingleShot(true);
    donationConfigure_.setInterval(1400);
    connect(&donationConfigure_, &QTimer::timeout, &donation_, &DonationEngine::configure);
    connect(&donation_, &DonationEngine::updated, this, [this] {
        if(daStatusLabel_) daStatusLabel_->setText(donation_.daStatus());
        if(dpStatusLabel_) dpStatusLabel_->setText(donation_.dpStatus());
        if(donationJournalLabel_) donationJournalLabel_->setText(donation_.journal().join("\n"));
    });
    broadcasting_ = obs_frontend_streaming_active();
    donation_.setLive(broadcasting_);
    donation_.configure();
    if (broadcasting_) alreadyHandled_ = true; // No retrospective notifications.
    rebuild();
}
StreamDock::~StreamDock() { shutdown(); }

bool StreamDock::isRu() const
{
    if (store_.current.locale == "ru") return true;
    if (store_.current.locale == "en") return false;
    const char* locale = obs_get_locale();
    return locale && QString::fromUtf8(locale).startsWith("ru", Qt::CaseInsensitive);
}
QString StreamDock::uiText(const char* ru, const char* en) const
{ return QString::fromUtf8(isRu() ? ru : en); }
QLabel* StreamDock::label(const QString& text, const QString& name)
{
    auto* w = paragraph(text);
    if(!name.isEmpty()) w->setObjectName(name);
    return w;
}
void StreamDock::section(QVBoxLayout* l, const QString& text)
{ l->addWidget(label(text, "Section")); }
QWidget* StreamDock::card(QVBoxLayout** l)
{
    QWidget* w = new QWidget;
    w->setObjectName("Card");
    *l = column(w, store_.current.compact ? 5 : 9, store_.current.compact ? 7 : 12);
    return w;
}
void StreamDock::changeSettings() { store_.scheduleSave(); donationConfigure_.start(); }
void StreamDock::showPage(Page page) { page_ = page; rebuild(); }
void StreamDock::showPhase(Phase phase) { phase_ = phase; rebuild(); }
void StreamDock::updateSaved(bool success)
{
    if(saveLabel_) saveLabel_->setText(success ? uiText("✓ Сохранено автоматически", "✓ Saved automatically")
                                              : uiText("⚠ Ошибка сохранения", "⚠ Save failed"));
}

void StreamDock::rebuild()
{
    // All text inputs continuously update SettingsStore::current, so rebuilding
    // for a language switch never discards uncommitted typed text.
    while (QLayoutItem* item = root_->takeAt(0)) {
        if (QWidget* w = item->widget()) delete w;
        delete item;
    }
    root_->setSpacing(store_.current.compact ? 4 : 9);
    title_ = nullptr;
    saveLabel_ = nullptr;
    daStatusLabel_ = nullptr; dpStatusLabel_ = nullptr; donationJournalLabel_ = nullptr;
    central_ = nullptr;
    auto* header = new QWidget;
    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(5,4,5,4);
    auto* t = label("◉ Stream Notify", "Title");
    title_ = t;
    h->addWidget(t, 1);
    if (store_.current.showStatus)
        h->addWidget(label(store_.current.enabled ? uiText("● Активен", "● Active") : uiText("○ Выключен", "○ Disabled"),
                           store_.current.enabled ? "Good" : "Hint"));
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
    root_->addWidget(central_, 1);
    if (phase_ == Phase::Idle) {
        saveLabel_ = label(uiText("Изменения сохраняются автоматически", "Changes are saved automatically"), "Hint");
        saveLabel_->setAlignment(Qt::AlignCenter);
        root_->addWidget(saveLabel_);
    }
    auto* author = new QLabel(QString::fromUtf8("Автор: ") +
        "<a href=\"https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/\"><span style=\"color:#59dfff;\">OTMOPOZOK</span></a>");
    author->setTextFormat(Qt::RichText);
    author->setOpenExternalLinks(true);
    author->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    author->setAlignment(Qt::AlignCenter);
    root_->addWidget(author);
}

QWidget* StreamDock::buildRegular()
{
    auto* wrap = new QWidget;
    auto* vertical = column(wrap, 6, 0);
    auto* tabs = new QWidget;
    auto* nav = new QHBoxLayout(tabs);
    nav->setContentsMargins(0,0,0,0);
    auto* home = btn(uiText("⌂  Основное", "⌂  Home"));
    auto* settings = btn(uiText("⚙ Настройки", "⚙ Settings"));
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
        const struct {Page page; QString name;} pages[] = {
            {Page::Connections, uiText("Связь", "Connect")},
            {Page::Message, uiText("Сообщение", "Message")},
            {Page::Interface, uiText("Интерфейс", "Interface")},
            {Page::Donations, uiText("Донаты", "Donations")}
        };
        int subIndex = 0;
        for (const auto& p : pages) {
            auto* b = btn(p.name);
            if(p.page == page_) b->setObjectName("ActiveNav");
            connect(b, &QPushButton::clicked, this, [this,p] {showPage(p.page);});
            subnav->addWidget(b, subIndex / 2, subIndex % 2);
            ++subIndex;
        }
        vertical->addWidget(sub);
    }
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget* body = nullptr;
    switch(page_) {
        case Page::Home: body = buildHome(); break;
        case Page::Connections: body = buildConnections(); break;
        case Page::Message: body = buildMessage(); break;
        case Page::Interface: body = buildInterface(); break;
        case Page::Donations: body = buildDonations(); break;
    }
    scroll->setWidget(body);
    vertical->addWidget(scroll,1);
    return wrap;
}

QWidget* StreamDock::buildHome()
{
    auto* body = new QWidget;
    auto* outer = column(body, 10, 3);
    QVBoxLayout* v;
    auto* enabled = card(&v);
    auto* toggle = new QCheckBox(uiText("Автоуведомления включены", "Automatic notifications enabled"));
    toggle->setChecked(store_.current.enabled);
    connect(toggle, &QCheckBox::toggled, this, [this](bool val) {
        store_.current.enabled = val;
        changeSettings();
        if (!val && phase_ == Phase::Counting) cancelCurrent("Disabled");
        else rebuild();
    });
    v->addWidget(toggle);
    v->addWidget(label(uiText("Отправлять при начале трансляции", "Send when streaming begins"), "Hint"));
    outer->addWidget(enabled);

    auto* status = card(&v);
    section(v, uiText("Статус трансляции", "Stream status"));
    QString state = broadcasting_ ? uiText("● Стрим активен", "● Stream is live") : uiText("◉ Ожидание трансляции", "◉ Waiting for stream");
    v->addWidget(label(state, broadcasting_ ? "Good" : "Section"));
    QString hint = statusText_;
    if (hint.isEmpty()) hint = broadcasting_ ? uiText("Рассылка не запущена", "No notification pending") :
      uiText("Начните трансляцию в OBS", "Start streaming in OBS");
    v->addWidget(label(hint, "Hint"));
    outer->addWidget(status);

    auto* channelCard = card(&v);
    section(v, uiText("Каналы уведомлений", "Notification channels"));
    auto addChannel = [&](Channel c) {
        const bool tg = (c == Channel::Telegram);
        auto* h = new QHBoxLayout;
        h->addWidget(label(tg ? "Telegram" : "Discord", "Section"));
        QString st = ! (tg ? store_.current.telegramEnabled : store_.current.discordEnabled) ? uiText("Отключён", "Disabled") :
            (tg ? validTelegram(store_.current.telegramToken, store_.current.telegramChat) : validDiscord(store_.current.discordWebhook)) ?
                uiText("● Готов", "● Ready") : uiText("⚠ Не настроен", "⚠ Not configured");
        h->addWidget(label(st, "Hint"));
        h->addStretch();
        auto* test = btn(uiText("Тест", "Test"));
        test->setEnabled(!(tg ? testingTelegram_ : testingDiscord_));
        connect(test, &QPushButton::clicked, this, [this,c] {testChannel(c);});
        h->addWidget(test);
        v->addLayout(h);
        QString temp = tg ? transientTelegram_ : transientDiscord_;
        if(!temp.isEmpty()) v->addWidget(label(temp, "Hint"));
    };
    addChannel(Channel::Telegram); addChannel(Channel::Discord);
    outer->addWidget(channelCard);

    auto* links = card(&v);
    section(v, uiText("Ссылки на трансляцию", "Stream links"));
    auto linkRow = [&](const QString& name, const QString& value) {
        v->addWidget(label(name, "Section"));
        auto* h = new QHBoxLayout;
        auto* text = label(value.isEmpty() ? uiText("Не задана", "Not set") : value, "Hint");
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        h->addWidget(text,1);
        auto* copy = btn(uiText("Копировать", "Copy"));
        connect(copy, &QPushButton::clicked, this, [value] {QApplication::clipboard()->setText(value);});
        h->addWidget(copy);
        v->addLayout(h);
    };
    linkRow("YouTube", store_.current.youtubeUrl); linkRow("Twitch", store_.current.twitchUrl);
    outer->addWidget(links);

    auto* last = card(&v);
    section(v, uiText("Последнее уведомление", "Last notification"));
    v->addWidget(label(store_.current.lastTime.isEmpty() ? uiText("Ещё не отправлялись", "Nothing sent yet") : store_.current.lastTime, "Hint"));
    if (!store_.current.lastTelegram.isEmpty()) v->addWidget(label("Telegram: " + store_.current.lastTelegram));
    if (!store_.current.lastDiscord.isEmpty()) v->addWidget(label("Discord: " + store_.current.lastDiscord));
    outer->addWidget(last);
    outer->addStretch();
    return body;
}

QWidget* StreamDock::buildConnections()
{
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto* global = card(&v);
    auto* all = new QCheckBox(uiText("Автоуведомления включены", "Automatic notifications enabled"));
    all->setChecked(store_.current.enabled);
    connect(all, &QCheckBox::toggled, this, [this](bool enabled) {store_.current.enabled=enabled;changeSettings();});
    v->addWidget(all);
    outer->addWidget(global);

    auto* telegramCard = card(&v);
    section(v,"Telegram");
    auto* tg = new QCheckBox(uiText("Отправлять в Telegram", "Send to Telegram"));
    tg->setChecked(store_.current.telegramEnabled);
    connect(tg, &QCheckBox::toggled, this, [this](bool val){store_.current.telegramEnabled=val;changeSettings();});
    v->addWidget(tg);
    v->addWidget(label(uiText("Токен бота", "Bot token"), "Hint"));
    auto* token = new QLineEdit(store_.current.telegramToken);
    token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText("123456789:... (BotFather)");
    connect(token, &QLineEdit::textChanged, this, [this](const QString& s) {store_.current.telegramToken=s;changeSettings();});
    auto* showToken = btn(uiText("Показать", "Show"));
    connect(showToken, &QPushButton::clicked, this, [token] {token->setEchoMode(token->echoMode()==QLineEdit::Password ? QLineEdit::Normal : QLineEdit::Password);});
    rowWith(v, token, showToken);
    v->addWidget(label(uiText("ID чата / @канал", "Chat ID / @channel"), "Hint"));
    auto* chat = new QLineEdit(store_.current.telegramChat);
    connect(chat, &QLineEdit::textChanged, this, [this](const QString& s){store_.current.telegramChat=s;changeSettings();});
    v->addWidget(chat);
    auto* h1 = new QHBoxLayout;
    auto* testTg = btn(uiText("Тест Telegram", "Test Telegram"));
    testTg->setEnabled(!testingTelegram_);
    connect(testTg, &QPushButton::clicked, this, [this] {testChannel(Channel::Telegram);});
    auto* helpTg = btn(uiText("Как настроить?", "Setup guide"));
    connect(helpTg, &QPushButton::clicked, this, [] {QDesktopServices::openUrl(QUrl("https://t.me/BotFather"));});
    h1->addWidget(testTg); h1->addWidget(helpTg);
    v->addLayout(h1);
    if (!transientTelegram_.isEmpty()) v->addWidget(label(transientTelegram_,"Hint"));
    outer->addWidget(telegramCard);

    auto* discordCard = card(&v);
    section(v,"Discord");
    auto* dc = new QCheckBox(uiText("Отправлять в Discord", "Send to Discord"));
    dc->setChecked(store_.current.discordEnabled);
    connect(dc, &QCheckBox::toggled, this, [this](bool val){store_.current.discordEnabled=val;changeSettings();});
    v->addWidget(dc);
    v->addWidget(label("Webhook URL", "Hint"));
    auto* hook = new QLineEdit(store_.current.discordWebhook);
    hook->setEchoMode(QLineEdit::Password);
    hook->setPlaceholderText("https://discord.com/api/webhooks/...");
    connect(hook, &QLineEdit::textChanged, this, [this](const QString& s){store_.current.discordWebhook=s;changeSettings();});
    auto* showHook = btn(uiText("Показать", "Show"));
    connect(showHook, &QPushButton::clicked, this, [hook] {hook->setEchoMode(hook->echoMode()==QLineEdit::Password ? QLineEdit::Normal : QLineEdit::Password);});
    rowWith(v, hook, showHook);
    auto* h2 = new QHBoxLayout;
    auto* testDc = btn(uiText("Тест Discord", "Test Discord"));
    testDc->setEnabled(!testingDiscord_);
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
    yt->setPlaceholderText("https://www.youtube.com/@channel/live");
    connect(yt, &QLineEdit::textChanged, this, [this](const QString& s) {store_.current.youtubeUrl=s;changeSettings();});
    v->addWidget(yt);
    v->addWidget(label("Twitch", "Hint"));
    auto* tw = new QLineEdit(store_.current.twitchUrl);
    tw->setPlaceholderText("https://www.twitch.tv/channel");
    connect(tw, &QLineEdit::textChanged, this, [this](const QString& s) {store_.current.twitchUrl=s;changeSettings();});
    v->addWidget(tw);
    outer->addWidget(urls);
    outer->addStretch();
    return body;
}

QWidget* StreamDock::buildMessage()
{
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto* templ = card(&v);
    section(v, uiText("Шаблон уведомления", "Notification template"));
    v->addWidget(label(uiText("Поддерживаются {youtube} и {twitch}", "Supports {youtube} and {twitch}"), "Hint"));
    auto* edit = new QTextEdit;
    edit->setMinimumHeight(150);
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
    section(v,uiText("Задержка отправки", "Notification delay"));
    v->addWidget(label(uiText("Отсчёт после подтверждённого старта OBS", "Countdown after confirmed OBS stream start"), "Hint"));
    auto* selection = new QComboBox;
    selection->addItem(uiText("60 секунд", "60 seconds"), 60);
    selection->addItem(uiText("90 секунд", "90 seconds"), 90);
    selection->addItem(uiText("120 секунд", "120 seconds"), 120);
    selection->setCurrentIndex(selection->findData(store_.current.sendDelay));
    connect(selection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this,selection](int){store_.current.sendDelay=selection->currentData().toInt();changeSettings();});
    v->addWidget(selection);
    outer->addWidget(delay);

    auto* prev = card(&v);
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
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto& s = store_.current.donations;
    auto addToggle=[&](QVBoxLayout* l,const QString& title,bool& target) {
        auto* c=new QCheckBox(title);
        c->setChecked(target);
        connect(c,&QCheckBox::toggled,this,[this,value=&target](bool checked){*value=checked;changeSettings();});
        l->addWidget(c);
    };
    auto* master=card(&v);
    section(v,uiText("Уведомления о донатах", "Donation notifications"));
    addToggle(v,uiText("Включить модуль", "Enable donation module"),s.enabled);
    addToggle(v,uiText("Только во время стрима", "Only while streaming"),s.onlyLive);
    addToggle(v,uiText("Публиковать сообщение донатера", "Include donor message"),s.includeDonorMessage);
    v->addWidget(label(uiText("Анонсы старта трансляции работают независимо от донатов.",
        "Stream start announcements work independently of donations."),"Hint"));
    outer->addWidget(master);

    auto addService=[&](const QString& title,bool& enabled,QString& token,QLabel*& statusLabel,
                        const QString& status,std::function<void()> test){
        QVBoxLayout* l;
        auto* box=card(&l);
        section(l,title);
        addToggle(l,uiText("Подключить сервис", "Enable service"),enabled);
        statusLabel=label(status,"Hint");
        l->addWidget(statusLabel);
        auto* edit=new QLineEdit(token);edit->setEchoMode(QLineEdit::Password);
        edit->setPlaceholderText(title+" API token");
        connect(edit,&QLineEdit::textChanged,this,[this,value=&token](const QString& text){*value=text;changeSettings();});
        auto* reveal=btn(uiText("Показать", "Show"));
        connect(reveal,&QPushButton::clicked,this,[edit]{edit->setEchoMode(edit->echoMode()==QLineEdit::Password?QLineEdit::Normal:QLineEdit::Password);});
        rowWith(l,edit,reveal);
        auto* verify=btn(uiText("Проверить подключение", "Check connection"));
        connect(verify,&QPushButton::clicked,this,[test]{test();});
        l->addWidget(verify);
        outer->addWidget(box);
    };
    addService("DonationAlerts",s.daEnabled,s.daToken,daStatusLabel_,donation_.daStatus(),[this]{donationConfigure_.stop();donation_.configure();donation_.checkDonationAlerts();});
    addService("DonatePay",s.dpEnabled,s.dpToken,dpStatusLabel_,donation_.dpStatus(),[this]{donationConfigure_.stop();donation_.configure();donation_.checkDonatePay();});
    auto* experimental=card(&v);
    addToggle(v,uiText("DonatePay: включить проверяемый старый API (экспериментально)",
                   "DonatePay: enable legacy API adapter (experimental)"),s.legacyApiOptIn);
    v->addWidget(label(uiText("Официальная страница API требует входа. Перед включением проверьте совместимость метода в своём аккаунте. Интервал запросов — 30 сек.",
        "Official API documentation requires sign-in. Verify the endpoint with your account before enabling. 30-second polling."),"Hint"));
    outer->addWidget(experimental);

    auto* destinations=card(&v);
    section(v,uiText("Куда отправлять", "Destinations"));
    addToggle(v,uiText("Telegram (использовать настройки Stream Notify)","Telegram (shared Stream Notify settings)"),s.toTelegram);
    addToggle(v,uiText("Discord (использовать настройки Stream Notify)","Discord (shared Stream Notify settings)"),s.toDiscord);
    outer->addWidget(destinations);

    auto* thresholds=card(&v);
    section(v,uiText("Пороговые суммы", "Donation thresholds"));
    v->addWidget(label(uiText("Все пороги — в рублях. Курсы сохраняются один раз, без автообновлений.",
        "All thresholds are in RUB. Rates are saved once, without automatic refresh."),"Hint"));
    auto* ratesLabel=label(donation_.ratesStatus(),"Hint");
    v->addWidget(ratesLabel);
    connect(&donation_,&DonationEngine::updated,ratesLabel,[this,ratesLabel]{ratesLabel->setText(donation_.ratesStatus());});
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

    auto* templates=card(&v);
    section(v,uiText("Шаблоны трёх уровней", "Three level templates"));
    auto* select=new QComboBox;
    select->addItem(uiText("💰 Обычный", "💰 Standard"),0);
    select->addItem(uiText("🔥 Крупный", "🔥 Large"),1);
    select->addItem(uiText("👑 Мега", "👑 Mega"),2);
    v->addWidget(select);
    auto* editor=new QTextEdit;
    editor->setMinimumHeight(200);
    editor->setPlainText(s.templates.at(0));
    v->addWidget(editor);
    v->addWidget(label("{donor} {amount} {currency} {message} {service} {stream_url}","Hint"));
    auto* preview=label("","Hint");
    preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto updatePreview=[this,select,preview]() {
        Donation test;test.service="DonationAlerts";test.id="preview";test.donor="Test donor";
        test.message="Спасибо за стрим!";test.currency="RUB";
        test.amount=select->currentIndex()==0?100:select->currentIndex()==1?500:2000;
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
    donationJournalLabel_=label(donation_.journal().join("\n"),"Hint");
    donationJournalLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(donationJournalLabel_);
    outer->addWidget(journal);
    outer->addStretch();
    return body;
}

QWidget* StreamDock::buildInterface()
{
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto* locale = card(&v);
    section(v,uiText("Язык интерфейса", "Interface language"));
    auto* lang = new QComboBox;
    lang->addItem(uiText("Автоматически (OBS)", "Automatic (OBS)"), "auto");
    lang->addItem("Русский", "ru");
    lang->addItem("English", "en");
    lang->setCurrentIndex(lang->findData(store_.current.locale));
    connect(lang, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this,lang](int) {
        store_.current.locale=lang->currentData().toString();
        changeSettings();
        rebuild();
    });
    v->addWidget(lang);
    outer->addWidget(locale);

    auto* appearance = card(&v);
    section(v,uiText("Вид панели", "Appearance"));
    auto* compact = new QCheckBox(uiText("Компактный режим", "Compact mode"));
    compact->setChecked(store_.current.compact);
    connect(compact, &QCheckBox::toggled, this, [this](bool b){store_.current.compact=b;changeSettings();rebuild();});
    v->addWidget(compact);
    auto* show = new QCheckBox(uiText("Показывать статус в заголовке", "Show status in header"));
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
    v->addWidget(label(uiText("Встроенная панель OBS без фонового опроса. Данные остаются на вашем ПК.", "Native OBS dock without background polling. Data stays on your PC."), "Hint"));
    outer->addWidget(about);
    outer->addStretch();
    return body;
}

// Circular progress is repainted once per second only while counting.
class CountdownRing final : public QWidget {
public:
    explicit CountdownRing(QWidget* parent = nullptr) : QWidget(parent) {setMinimumSize(210,210);}
    void setValues(int remaining, int total) { remaining_=remaining; total_=std::max(1,total); update(); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int side = std::min(width(), height()) - 17;
        const QRectF r((width()-side)/2.0, (height()-side)/2.0, side, side);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#26364d"), 10, Qt::SolidLine, Qt::RoundCap));
        p.drawEllipse(r);
        p.setPen(QPen(QColor("#24dcf8"), 10, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r,90*16, -int(360.0*16*remaining_/total_));
        p.setPen(QColor("#e4f3ff"));
        QFont f=font(); f.setPointSize(39); f.setBold(true); p.setFont(f);
        const QString value = QString("%1:%2").arg(remaining_/60,2,10,QChar('0')).arg(remaining_%60,2,10,QChar('0'));
        p.drawText(rect(), Qt::AlignCenter,value);
    }
private:
    int remaining_ = 60;
    int total_ = 60;
};

QWidget* StreamDock::buildCountdown()
{
    auto* body = new QWidget;
    auto* v = column(body, 12, 7);
    v->addStretch(1);
    auto* heading = label(uiText("До отправки уведомлений", "Until notifications are sent"),"Section");
    heading->setAlignment(Qt::AlignCenter);
    v->addWidget(heading);
    auto* ring = new CountdownRing;
    ring_ = ring;
    ring->setValues(std::max(0, session_.delay - int(elapsed_.elapsed()/1000)),session_.delay);
    v->addWidget(ring, 0, Qt::AlignHCenter);
    auto* help = label(uiText("Уведомления будут отправлены после таймера", "Messages will be sent when the timer expires"),"Hint");
    help->setAlignment(Qt::AlignCenter);
    v->addWidget(help);

    QVBoxLayout* channels;
    auto* box = card(&channels);
    section(channels,uiText("Активные каналы", "Active channels"));
    channels->addWidget(label(QString("Telegram: %1").arg(session_.telegram ? uiText("● ВКЛ", "● ON") : uiText("выключен", "off"))));
    channels->addWidget(label(QString("Discord: %1").arg(session_.discord ? uiText("● ВКЛ", "● ON") : uiText("выключен", "off"))));
    channels->addWidget(label(uiText("Задержка: %1 сек.", "Delay: %1 sec.").arg(session_.delay),"Hint"));
    v->addWidget(box);
    auto* cancel = btn(uiText("■  Отменить отправку", "■  Cancel notification"));
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
    const bool tgSent = telegram_.status == Outcome::Sent;
    const bool dcSent = discord_.status == Outcome::Sent;
    const bool allOk = (!session_.telegram || tgSent) && (!session_.discord || dcSent);
    const bool partial = (tgSent || dcSent) && !allOk;
    auto* body = new QWidget;
    body->setObjectName("ResultPanel");
    if(allOk) body->setStyleSheet("QWidget#ResultPanel {background-color: #08643b;border: 1px solid #0cca7c;border-radius: 15px;}");
    else if (partial) body->setStyleSheet("QWidget#ResultPanel {background-color: #5b3c22;border: 1px solid #cb8a28;border-radius: 15px;}");
    else body->setStyleSheet("QWidget#ResultPanel {background-color: #58262c;border: 1px solid #de5863;border-radius: 15px;}");
    auto* v = column(body, 16, 13);
    v->addStretch();
    auto* symbol = label(allOk ? "✔" : partial ? "⚠" : "✕");
    QFont big = symbol->font(); big.setPointSize(74); big.setBold(true); symbol->setFont(big);
    symbol->setStyleSheet("color:#ffffff;");
    symbol->setAlignment(Qt::AlignHCenter);
    v->addWidget(symbol);
    auto* title = label(allOk ? uiText("Уведомления отправлены", "Notifications sent") :
       partial ? uiText("Отправлено частично", "Partially delivered") : uiText("Не удалось отправить", "Delivery failed"));
    title->setAlignment(Qt::AlignHCenter);
    QFont font = title->font(); font.setPointSize(20); font.setBold(true); title->setFont(font);
    title->setStyleSheet("color:#ffffff;");
    v->addWidget(title);
    QVBoxLayout* results;
    auto* cardWidget = card(&results);
    auto detail = [this,results](const QString& name, const Delivery& d) {
        results->addWidget(label(name + "  " + statusName(d), "Section"));
        if (d.status == Outcome::Failed || d.status == Outcome::Unknown || d.status == Outcome::NotConfigured) {
            results->addWidget(label(d.detail,"Hint"));
        }
    };
    detail("Telegram",telegram_); detail("Discord",discord_);
    v->addWidget(cardWidget);
    auto* hint = label(uiText("Панель автоматически вернётся через %1 секунд", "The normal panel will return in %1 seconds")
                       .arg(session_.resultSeconds),"Hint");
    hint->setAlignment(Qt::AlignCenter);
    v->addWidget(hint);
    v->addStretch();
    return body;
}

void StreamDock::updateCountdown()
{
    if(phase_ != Phase::Counting) {countdown_.stop(); return;}
    const qint64 msRemaining = qint64(session_.delay)*1000 - elapsed_.elapsed();
    const int remain = int(std::max<qint64>(0, (msRemaining + 999) / 1000));
    if(ring_) ring_->setValues(remain,session_.delay);
    if (msRemaining <= 0) {
        countdown_.stop();
        dispatch();
    }
}

void StreamDock::streamStarted()
{
    if (shuttingDown_) return;
    broadcasting_ = true;
    donation_.setLive(true);
    // STREAMING_STARTED is the only trigger, never scene changes or reconnect callbacks.
    if(alreadyHandled_) {rebuild();return;}
    alreadyHandled_ = true;
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
    if (phase_ != Phase::Counting) return;
    if (!obs_frontend_streaming_active() || !broadcasting_) {cancelCurrent("Stream inactive");return;}
    if (telegram_.status == Outcome::Waiting && !validTelegram(session_.token,session_.chat))
        telegram_ = {Outcome::NotConfigured,"Check BotFather token and Chat ID",0};
    if (discord_.status == Outcome::Waiting && !validDiscord(session_.webhook))
        discord_ = {Outcome::NotConfigured,"Check Discord webhook URL",0};
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
    if (channel == Channel::Telegram) telegram_ = delivery;
    else discord_ = delivery;
    const QString name = channel == Channel::Telegram ? "TELEGRAM" : "DISCORD";
    store_.addEvent(name + ": " + (delivery.status == Outcome::Sent ? "SENT" :
                              delivery.status == Outcome::Unknown ? "UNKNOWN" : "FAILED"));
}
void StreamDock::complete()
{
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
    if(shuttingDown_) return;
    shuttingDown_ = true;
    countdown_.stop();
    resultTimer_.stop();
    ++generation_;
    sender_.abortAll();
    testSender_.abortAll();
    donation_.stop();
    store_.flush();
}
