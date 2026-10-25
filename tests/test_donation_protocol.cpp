#include "../src/donations.hpp"
#include "../src/donatepay_reply.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <cstdlib>
#include <iostream>

static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
struct DockPreview {static void receive(DonationEngine& engine,const Donation& donation){engine.onDonation(donation);}};
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    const auto rejected=sn::parseDonatePayReply(200,R"({"status":"error","message":"Incorrect token"})");
    require(!rejected.ok&&rejected.permanent&&rejected.status=="Incorrect DonatePay API key","HTTP-200 token rejection misclassified");
    require(sn::parseDonatePayReply(429,"{}").rateLimited,"HTTP 429 not rate limited");
    require(sn::parseDonatePayReply(200,R"({"status":429})").rateLimited,"Envelope 429 not rate limited");
    for(int code:{401,403})require(sn::parseDonatePayReply(code,"{}").permanent,"Unauthorized request retried");
    require(sn::parseDonatePayReply(200,"<html>not API</html>").permanent,"HTML response accepted");
    const auto valid=sn::parseDonatePayReply(200,R"({"status":"success","data":[{"id":123,"what":"Fixture","sum":"112.21","currency":"RUB","type":"donation","status":"success","comment":"hello"}]})");
    require(valid.ok&&valid.rows.size()==1,"Official DonatePay transaction envelope rejected");
    const auto donation=parseDonatePay(valid.rows[0].toObject());
    require(donation.id=="123"&&donation.amount==112.21&&donation.currency=="RUB"&&!donation.isTest,"Transaction fields not preserved");
    auto test=valid.rows[0].toObject();test["status"]="user";require(parseDonatePay(test).isTest,"DonatePay manual event counted as real");
    test=valid.rows[0].toObject();test.remove("currency");require(parseDonatePay(test).currency.isEmpty(),"Currency inferred without evidence");
    QJsonObject da{{"id",9},{"amount","150"},{"currency","RUB"},{"username","Fixture"},{"is_test","1"}};
    require(parseDonationAlerts(da).isTest,"String test flag counted as real");da["is_test"]=false;
    require(!parseDonationAlerts(da).isTest,"Real DonationAlerts event rejected");
    DonationSettings thresholds;thresholds.thresholds["USD"]={2,10,50};Donation usd;usd.service="Fixture";usd.id="usd";usd.currency="USD";usd.amount=10;
    require(donationLevel(usd,thresholds)==1,"Explicit foreign currency thresholds ignored");
    QTemporaryDir directory;require(directory.isValid(),"Temp directory unavailable");
    qputenv("STREAM_NOTIFY_PREVIEW_DIR",directory.path().toUtf8());SettingsStore store;
    store.current.donations.enabled=true;store.current.donations.onlyLive=false;
    store.current.donations.toTelegram=false;store.current.donations.toDiscord=false;
    Donation below=donation;below.id="below-threshold";below.amount=30;
    {DonationEngine engine(&store);engine.setLive(true);DockPreview::receive(engine,below);DockPreview::receive(engine,below);
     require(engine.sessionSummary().count==1,"Below-threshold deduplication failed");}
    {DonationEngine engine(&store);engine.setLive(true);DockPreview::receive(engine,below);
     require(engine.sessionSummary().count==0,"Event replayed after engine restart");}
    const auto path=directory.filePath("ledger.json");
    {DonationLedger ledger(path);require(ledger.load()&&ledger.claim("DonatePay:123",donation,0,true,true),"Ledger claim failed");
     require(ledger.setStatus("DonatePay:123","telegram","inflight"),"Inflight state not saved");}
    {DonationLedger ledger(path);require(ledger.load()&&ledger.contains("DonatePay:123"),"Persisted ID lost on restart");
     require(!ledger.claim("DonatePay:123",donation,0,true,true),"Duplicate donation claimed after restart");
     require(ledger.recent().join('\n').contains("unknown")&&ledger.recent().join('\n').contains("not_sent_after_restart"),"Interrupted delivery states not recovered safely");}
    std::cout<<"Donation protocols: official envelope, errors, real/test flags, thresholds and persistent replay protection passed\n";
}

