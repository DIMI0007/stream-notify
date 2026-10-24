#include <QCoreApplication>
#include <QSslSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QFileInfo>
#include <iostream>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(argc!=2)return 2;
    const QString plugins=QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    // Exclude development Qt plugin paths: verify the staged package alone.
    QCoreApplication::setLibraryPaths({plugins});
    if(!QSslSocket::availableBackends().contains("schannel") ||
       !QSslSocket::setActiveBackend("schannel") || !QSslSocket::supportsSsl()) {
        std::cerr<<"Packaged Schannel backend failed to initialize\n";return 3;
    }
    std::cout<<"Packaged Schannel TLS backend loaded\n";
    QNetworkAccessManager network;
    QNetworkRequest request(QUrl("https://api.telegram.org/"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(20000);
    auto* reply=network.get(request);
    bool sslErrors=false,finished=false;int result=4;
    QObject::connect(reply,&QNetworkReply::sslErrors,&app,[&](const auto&){sslErrors=true;});
    QObject::connect(reply,&QNetworkReply::finished,&app,[&]{
        finished=true;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(!sslErrors && status>=200 && status<500) {
            std::cout<<"Telegram HTTPS handshake verified, HTTP "<<status<<"\n";result=0;
        } else std::cerr<<"Telegram HTTPS check failed, HTTP "<<status<<", Qt network code "<<int(reply->error())<<"\n";
        reply->deleteLater();app.quit();
    });
    QTimer::singleShot(25000,&app,[&]{if(!finished)reply->abort();app.quit();});
    app.exec();return result;
}
