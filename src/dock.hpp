#pragma once
#include "settings.hpp"
#include "net.hpp"
#include "donations.hpp"
#include "stream_monitor.hpp"
#include "platform_probe.hpp"
namespace sn {class OutputPanel;class ResourcePanel;}
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QSet>

class CountdownRing;
class QPushButton;
class StreamDock : public QWidget {
#ifdef STREAM_NOTIFY_UI_PREVIEW
    friend struct DockPreview;
#endif
    Q_OBJECT
public:
    explicit StreamDock(QWidget* parent = nullptr);
    ~StreamDock() override;
    QSize sizeHint() const override { return QSize(360,620); }
    void refreshStreamingState();
    void streamStarted();
    void streamStopped();
    void shutdown();
    bool wasLoadedCleanly() const { return loadedCleanly_; }
private:
    enum class Phase { Idle, Counting, Sending, Result, Cancelled };
    enum class Page { Home, Connections, Message, Interface, Donations, ManualMessage, Journal, Preview };
    enum class DonationPage { Services, Delivery, Amounts, Templates };
    DonationPage donationPage_ = DonationPage::Services;
    SettingsStore store_;
    Sender sender_;
    Sender testSender_;
    Sender manualSender_;
    Sender secondSender_;
    sn::PlatformProbe platforms_;
    DonationEngine donation_;
    QTimer donationConfigure_;
    Phase phase_ = Phase::Idle;
    Page page_ = Page::Home;
    bool alreadyHandled_ = false;
    bool announcementAttempted_ = false;
    bool broadcasting_ = false;
    bool loadedCleanly_ = true;
    quint64 generation_ = 0;
    QElapsedTimer elapsed_;
    QTimer countdown_;
    QTimer streamStateTimer_;
    QTimer resultTimer_;
    QTimer verificationTimer_;
    Snapshot session_;
    QString mainTemplate_;
    Delivery telegram_;
    Delivery discord_;
    int activePending_ = 0;
    bool mainDelivered_=false;
    QMap<QString,QString> mainIds_;
    QSet<QString> secondPending_;
    QStringList telegramMainKeys_,discordMainKeys_;
    QString statusText_;
    QString pendingFocus_;
    QString transientTelegram_;
    QString transientDiscord_;
    QString manualDraft_;
    QString manualTelegramStatus_;
    QString manualDiscordStatus_;
    bool manualTelegramSelected_ = true;
    bool manualDiscordSelected_ = true;
    bool manualEveryoneSelected_ = false;
    bool manualSending_ = false;
    bool manualAllSent_ = false;
    int manualPending_ = 0;
    bool testingTelegram_ = false;
    bool testingDiscord_ = false;

    QVBoxLayout* root_ = nullptr;
    QLabel* title_ = nullptr;
    QPointer<QLabel> donationSummaryLabel_;
    QPointer<QLabel> homeDaIndicator_,homeDpIndicator_;
    QPointer<QPushButton> daLoginButton_;
    QPointer<sn::OutputPanel> outputPanel_;
    QPointer<sn::ResourcePanel> resourcePanel_;
    QPointer<QLabel> trafficFooter_;
    sn::StreamMonitor monitor_;
    sn::UsageSampler usageSampler_;
    sn::Usage usage_;
    QTimer telemetryTimer_;
    QElapsedTimer telemetryClock_;
    QWidget* central_ = nullptr;
    QLabel* countdownLabel_ = nullptr;
    QLabel* daStatusLabel_ = nullptr;
    QLabel* dpStatusLabel_ = nullptr;
    QLabel* donationJournalLabel_ = nullptr;
    QPointer<CountdownRing> ring_;
    bool shuttingDown_ = false;

    QString uiText(const char* ru, const char* en) const;
    bool isRu() const;
    void rebuild();
    void rebuildNow();
    bool rebuildPending_ = false;
    QWidget* buildRegular();
    QWidget* buildHome();
    QWidget* buildManualMessage();
    QWidget* buildJournal();
    QWidget* buildAnnouncementPreview();
    QString confirmedPreview(QString* error) const;
    void sendManualMessage();
    QString donationSummaryText() const;
    QString donationConnectionText() const;
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
    void openConnections(const QString& field);
    void showPhase(Phase phase);
    void updateCountdown();
    void refreshTelemetry();
    void cancelCurrent(const QString& reason);
    void dispatch();
    void probePlatforms();
    void onPlatformsChanged();
    void sendSecondPlatform(const QString& platform,const sn::LivePlatform& live);
    void complete();
    void testChannel(Channel channel);
    void recordOutcome(Channel channel, const Delivery& outcome);
    QString statusName(const Delivery& d) const;
    void changeSettings();
    void updateSaved(bool success);
};
