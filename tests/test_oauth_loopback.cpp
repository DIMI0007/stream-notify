#include "../src/oauth_loopback.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <cstdlib>

static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static QByteArray exchange(const QUrl& url,const QByteArray& request){
    QTcpSocket socket;socket.connectToHost(QHostAddress::LocalHost,quint16(url.port()));
    QElapsedTimer elapsed;elapsed.start();
    while(socket.state()!=QAbstractSocket::ConnectedState&&elapsed.elapsed()<2000){QCoreApplication::processEvents();QThread::msleep(1);}
    require(socket.state()==QAbstractSocket::ConnectedState,"Local test connection failed");
    socket.write(request);QByteArray response;
    while(elapsed.elapsed()<2500){QCoreApplication::processEvents();response+=socket.readAll();if(socket.state()==QAbstractSocket::UnconnectedState)break;QThread::msleep(1);}
    return response;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);OAuthLoopback receiver;QString token;
    QObject::connect(&receiver,&OAuthLoopback::received,&app,[&](const QUrlQuery& q){token=q.queryItemValue("access_token",QUrl::FullyDecoded);});
    require(receiver.begin(0,"/callback",true),"Receiver did not start");
    const auto url=receiver.redirect();const auto host=url.authority().toUtf8();const auto state=receiver.state().toUtf8();
    const auto page=exchange(url,"GET /callback HTTP/1.1\r\nHost: "+host+"\r\n\r\n");
    require(page.startsWith("HTTP/1.1 200")&&page.contains("Content-Security-Policy")&&page.contains("history.replaceState"),"Implicit bridge protections missing");
    require(exchange(url,"GET /callback HTTP/1.1\r\nHost: attacker.example\r\n\r\n").startsWith("HTTP/1.1 400"),"Foreign Host accepted");
    auto post=[&](const QByteArray& origin,const QByteArray& body){return exchange(url,"POST /oauth-complete/"+state+" HTTP/1.1\r\nHost: "+host+"\r\nOrigin: "+origin+"\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);};
    const auto body="state="+state+"&access_token=FAKE%2BTOKEN&token_type=Bearer";
    require(post("https://attacker.example",body).startsWith("HTTP/1.1 400")&&token.isEmpty(),"Foreign Origin accepted");
    require(post(url.adjusted(QUrl::RemovePath).toString().toUtf8(),"state=wrong&access_token=FAKE").startsWith("HTTP/1.1 400")&&receiver.active(),"Invalid state accepted or receiver stopped");
    require(post(url.adjusted(QUrl::RemovePath).toString().toUtf8(),body).startsWith("HTTP/1.1 200"),"Valid callback rejected");
    require(token=="FAKE+TOKEN"&&!receiver.active(),"Token decoding or one-use shutdown failed");
    require(receiver.begin(0,"/callback",false),"Code receiver did not start");
    const auto codeUrl=receiver.redirect();
    const auto code=exchange(codeUrl,"GET /callback?state="+receiver.state().toUtf8()+"&code=TEST_CODE HTTP/1.1\r\nHost: "+codeUrl.authority().toUtf8()+"\r\n\r\n");
    require(code.startsWith("HTTP/1.1 200")&&!receiver.active(),"Authorization-code callback failed");
    std::cout<<"Loopback OAuth: Host, Origin, state, encoding and one-use lifecycle passed\n";
}

