#include <QCoreApplication>
#include <QSslSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QFileInfo>
#include <functional>
#include <memory>
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
    const QStringList endpoints={"https://api.telegram.org/","https://discord.com/api/v10/gateway"};
    int index=0,result=0;bool done=false;
    std::function<void()> next;
    next=[&] {
        if(index>=endpoints.size()){done=true;app.quit();return;}
        const int current=index++;
        QNetworkRequest request{QUrl(endpoints[current])};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        request.setTransferTimeout(15000);
        auto* reply=network.get(request);
        auto sslErrors=std::make_shared<bool>(false);
        QObject::connect(reply,&QNetworkReply::sslErrors,&app,[sslErrors](const auto&){*sslErrors=true;});
        QObject::connect(reply,&QNetworkReply::finished,&app,[&,reply,current,sslErrors] {
            const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const char* service=current==0?"Telegram":"Discord";
            if(!*sslErrors && status>=200 && status<500)
                std::cout<<service<<" HTTPS handshake verified, HTTP "<<status<<"\n";
            else {
                std::cerr<<service<<" HTTPS check failed, HTTP "<<status<<", Qt network code "<<int(reply->error())<<"\n";
                result=4;
            }
            reply->deleteLater();next();
        });
    };
    QTimer::singleShot(35000,&app,[&]{if(!done){result=5;app.quit();}});
    next();app.exec();return result;
}
