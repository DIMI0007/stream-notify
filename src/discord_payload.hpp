#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <optional>
inline std::optional<QJsonObject> discordPayload(const QString& message,bool everyone) {
    QString content=message;
    QJsonArray parse;
    if(everyone) {
        content=content.trimmed();
        // Avoid a duplicate automatic suffix if the template already ends with it.
        while(content.endsWith("@everyone"))content.chop(9),content=content.trimmed();
        // Discord's "everyone" permission also permits @here. Only enable the
        // explicitly requested @everyone ping.
        content.replace("@here",QString("@")+QChar(0x200B)+"here");
        content+="\n\n@everyone";
        parse.append("everyone");
    }
    if(content.size()>2000)return std::nullopt;
    return QJsonObject{{"content",content},{"allowed_mentions",QJsonObject{{"parse",parse}}}};
}
