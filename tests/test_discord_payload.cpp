#include "../src/discord_payload.hpp"
#include <iostream>
int main() {
    auto check=[](bool value){if(!value){std::cerr<<"Discord mention test failed\n";return false;}return true;};
    const QString text=QString::fromUtf8("Стрим начался!\nhttps://example.com/live");
    const auto off=discordPayload(text,false),on=discordPayload(text,true);
    if(!check(off&&on))return 1;
    if(!check(off->value("content").toString()==text &&
        off->value("allowed_mentions").toObject().value("parse").toArray().isEmpty()))return 2;
    if(!check(on->value("content").toString()==text+"\n\n@everyone" &&
        on->value("allowed_mentions").toObject().value("parse").toArray()==QJsonArray{"everyone"}))return 3;
    const auto existing=discordPayload(text+"\n@everyone",true);
    if(!check(existing && existing->value("content")==on->value("content")))return 4;
    const auto manual=discordPayload(QString::fromUtf8("Первая строка\nВторая строка\n@everyone"),true);
    if(!check(manual && manual->value("content").toString().contains(QString::fromUtf8("Первая строка\nВторая строка")) &&
        manual->value("allowed_mentions").toObject().value("parse").toArray()==QJsonArray{"everyone"}))return 7;
    const auto here=discordPayload("Hello @here",true);
    if(!check(here&&!here->value("content").toString().contains("@here")))return 5;
    if(!check(discordPayload(QString(1989,'a'),true).has_value() &&
        !discordPayload(QString(1990,'a'),true).has_value() &&
        discordPayload(QString(2000,'a'),false).has_value()))return 6;
    std::cout<<"Discord everyone on/off, suffix and payload limit tests passed\n";
}
