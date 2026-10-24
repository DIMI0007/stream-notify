#pragma once
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QRegularExpression>

inline QString telegramChannel(const QString& input)
{
    const QString raw=input.trimmed();
    if(raw.startsWith("@") || QRegularExpression("^-?[0-9]+$").match(raw).hasMatch())return raw;
    QString link=raw;
    if(link.startsWith("t.me/") || link.startsWith("telegram.me/"))link.prepend("https://");
    const QUrl url(link);
    const QString host=url.host().toLower();
    if((url.scheme()=="https" || url.scheme()=="http") &&
       (host=="t.me" || host=="telegram.me") && url.userInfo().isEmpty()) {
        QStringList parts=url.path().split('/',Qt::SkipEmptyParts);
        if(parts.size()>=3 && parts[0]=="c" &&
           QRegularExpression("^[1-9][0-9]*$").match(parts[1]).hasMatch() &&
           QRegularExpression("^[0-9]+$").match(parts[2]).hasMatch())
            return "-100"+parts[1];
        if(parts.size()>1 && parts[0]=="s")parts.removeFirst();
        if(!parts.isEmpty() && parts[0]!="joinchat" &&
           QRegularExpression("^[A-Za-z][A-Za-z0-9_]{4,}$").match(parts[0]).hasMatch())
            return "@"+parts[0];
    }
    return raw; // Invite links cannot identify a Bot API recipient.
}
