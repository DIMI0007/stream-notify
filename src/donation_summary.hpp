#pragma once
#include <QByteArray>
#include <QString>
#include <QSet>
#include <QCryptographicHash>
#include <cmath>
#include <cstdint>
namespace sn {
struct DonationSummary {
    bool started=false,active=false,converted=false;
    uint64_t count=0,unpriced=0;double rub=0;
};
class DonationSession {
public:
    void begin(){summary_={};summary_.started=true;summary_.active=true;}
    void end(){summary_.active=false;}
    bool observe(const QString& key,double amount,double rubRate,bool isTest,bool foreign=false) {
        if(!summary_.active || isTest || key.isEmpty() || !std::isfinite(amount) || amount<=0)return false;
        const auto hash=QCryptographicHash::hash(key.toUtf8(),QCryptographicHash::Sha256);
        if(seen_.contains(hash))return false;
        seen_.insert(hash);++summary_.count;
        const double value=amount*rubRate;
        if(std::isfinite(rubRate)&&rubRate>0&&std::isfinite(value)&&std::isfinite(summary_.rub+value)) {
            summary_.rub+=value;summary_.converted=summary_.converted || foreign;
        } else ++summary_.unpriced;
        return true;
    }
    DonationSummary value()const{return summary_;}
    template<class Census>void account(Census& census)const{for(const auto& hash:seen_)census.add(hash);}
private:
    // Hashes are retained across streams during this OBS run, preventing replay counts.
    // No donor names/messages are retained for this summary.
    QSet<QByteArray> seen_;DonationSummary summary_;
};
}
