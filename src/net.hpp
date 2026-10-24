#pragma once
#include "settings.hpp"
#include <QObject>
#include <QNetworkAccessManager>
#include <QSet>
#include <functional>
#include <memory>
#include <vector>
namespace sn {class BufferCensus;}

class QNetworkReply;
enum class Channel { Telegram, Discord };
enum class Outcome { Disabled, NotConfigured, Waiting, Sending, Sent, Failed, Unknown, Cancelled };
struct Delivery {
    Outcome status = Outcome::Disabled;
    QString detail;
    int attempts = 0;
};

class Sender : public QObject {
    Q_OBJECT
public:
    explicit Sender(QObject* parent = nullptr);
    using Completion = std::function<void(Delivery)>;
    void send(Channel channel, const Snapshot& s, const QString& message, Completion done);
    void abortAll();
    void account(sn::BufferCensus& census) const;
private:
    struct Task {
        Channel channel;
        Snapshot s;
        QString message;
        QByteArray body;
        int attempts = 0;
        quint64 epoch = 0;
        Completion done;
    };
    void start(const std::shared_ptr<Task>& task);
    std::vector<std::weak_ptr<Task>> tasks_;
    QNetworkAccessManager nam_;
    QSet<QNetworkReply*> active_;
    quint64 epoch_ = 0;
};
