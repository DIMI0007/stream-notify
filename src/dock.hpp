#pragma once
#include "settings.hpp"
#include "net.hpp"
#include "donations.hpp"
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

class CountdownRing;
class StreamDock : public QWidget {
    Q_OBJECT
public:
    explicit StreamDock(QWidget* parent = nullptr);
    ~StreamDock() override;
    void streamStarted();
    void streamStopped();
    void shutdown();
    bool wasLoadedCleanly() const { return loadedCleanly_; }
private:
    enum class Phase { Idle, Counting, Sending, Result, Cancelled };
    enum class Page { Home, Connections, Message, Interface, Donations };
    SettingsStore store_;
    Sender sender_;
    Sender testSender_;
    DonationEngine donation_;
    QTimer donationConfigure_;
    Phase phase_ = Phase::Idle;
    Page page_ = Page::Home;
    bool alreadyHandled_ = false;
    bool broadcasting_ = false;
    bool loadedCleanly_ = true;
    quint64 generation_ = 0;
    QElapsedTimer elapsed_;
    QTimer countdown_;
    QTimer resultTimer_;
    Snapshot session_;
    Delivery telegram_;
    Delivery discord_;
    int activePending_ = 0;
    QString statusText_;
    QString transientTelegram_;
    QString transientDiscord_;
    bool testingTelegram_ = false;
    bool testingDiscord_ = false;

    QVBoxLayout* root_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* saveLabel_ = nullptr;
    QWidget* central_ = nullptr;
    QLabel* countdownLabel_ = nullptr;
    QLabel* daStatusLabel_ = nullptr;
    QLabel* dpStatusLabel_ = nullptr;
    QLabel* donationJournalLabel_ = nullptr;
    QPointer<CountdownRing> ring_;
    bool shuttingDown_ = false;

    QString tr(const char* ru, const char* en) const;
    bool isRu() const;
    void rebuild();
    QWidget* buildRegular();
    QWidget* buildHome();
    QWidget* buildConnections();
    QWidget* buildMessage();
    QWidget* buildInterface();
    QWidget* buildDonations();
    QWidget* buildCountdown();
    QWidget* buildSending();
    QWidget* buildResult();
    QWidget* card(QVBoxLayout** layout);
    QLabel* label(const QString& text, const QString& name = QString());
    void section(QVBoxLayout* layout, const QString& text);
    void showPage(Page page);
    void showPhase(Phase phase);
    void updateCountdown();
    void cancelCurrent(const QString& reason);
    void dispatch();
    void complete();
    void testChannel(Channel channel);
    void recordOutcome(Channel channel, const Delivery& outcome);
    QString statusName(const Delivery& d) const;
    void changeSettings();
    void updateSaved(bool success);
};
