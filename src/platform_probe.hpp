#pragma once
#include "secure_store.hpp"
#include "oauth_loopback.hpp"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QTimer>
#include <QDateTime>
#include <QSet>
#include <functional>

namespace sn {
enum class LivePhase { Offline, Starting, Live, Reconnecting, Error };
struct LivePlatform {
    LivePhase phase=LivePhase::Offline;
    QString id,url,error;
    bool confirmed=false;
    QDateTime checkedAt;
};

class PlatformProbe final : public QObject {
public:
    explicit PlatformProbe(QObject* parent=nullptr);
    void setChanged(std::function<void()> callback){changed_=std::move(callback);}
    void check(bool youtubeOutput,bool twitchOutput,const QString& youtubeFallback,const QString& twitchFallback);
    void beginYouTubeLogin();
    void beginTwitchLogin();
    void signOutYouTube();
    void signOutTwitch();
    QString youtubeAccount()const;
    QString twitchAccount()const;
    QString twitchDeviceCode()const{return twitchCode_;}
    LivePlatform youtube()const{return youtube_;}
    LivePlatform twitch()const{return twitch_;}
    void stop();
private:
    using Response=std::function<void(int,QJsonObject)>;
    void request(const QUrl& url,const QByteArray& method,const QByteArray& body,
                 const QByteArray& bearer,const QByteArray& clientId,Response done);
    void checkYouTube();
    void checkTwitch();
    void refreshYouTube();
    void refreshTwitch();
    void validateTwitch();
    void setYouTube(LivePlatform value);
    void setTwitch(LivePlatform value);
    void failYouTube(const QString& reason);
    void failTwitch(const QString& reason);
    void loadAccounts();
    QString googleSecret()const;
    QString accountPath(const QString& name,bool chat)const;
    void saveOwnYouTube(const QJsonObject& value);
    void saveOwnTwitch(const QJsonObject& value);
    void pollTwitchDevice();
    QNetworkAccessManager http_;
    QSet<QNetworkReply*> replies_;
    OAuthLoopback googleLogin_;
    QTimer twitchDeviceTimer_;
    SecureStore ownYouTube_,ownTwitch_,chatYouTube_,chatTwitch_;
    QJsonObject youtubeToken_,twitchToken_;
    LivePlatform youtube_,twitch_;
    QString youtubeFallback_,twitchFallback_,twitchLogin_,twitchDevice_,twitchCode_,googleVerifier_;
    QDateTime twitchDeviceExpires_;
    bool youtubeLocal_=false,twitchLocal_=false,youtubeFromChat_=false,twitchFromChat_=false;
    bool youtubeBusy_=false,twitchBusy_=false,stopping_=false;
    int twitchDeviceInterval_=5;
    quint64 epoch_=0;
    std::function<void()> changed_;
};
}
