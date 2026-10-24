#include "../src/resources.hpp"
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <cassert>
#include <iostream>
static void busy(int ms){QElapsedTimer clock;clock.start();volatile uint64_t n=0;while(clock.elapsed()<ms)++n;}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    const auto initial=sn::Meter::instance().totals();
    busy(25);assert(sn::Meter::instance().totals().cpuNs==initial.cpuNs);
    {sn::CpuScope outer;busy(40);{sn::CpuScope nested;busy(40);}}
    const auto after=sn::Meter::instance().totals();
    assert(after.scopes==initial.scopes+1 && after.cpuNs>initial.cpuNs);
    QString buffer;buffer.reserve(1000);buffer="owned";const QString shared=buffer;
    QByteArray bytes;bytes.reserve(2000);bytes="payload";
    sn::BufferCensus c;c.add(buffer);c.add(shared);c.add(bytes);c.add(bytes);
    assert(c.bytes()==uint64_t(buffer.capacity())*sizeof(QChar)+uint64_t(bytes.capacity()));
    QTcpServer server;assert(server.listen(QHostAddress::LocalHost));
    const QByteArray response="fixture-response";
    QObject::connect(&server,&QTcpServer::newConnection,&server,[&]{
        auto* socket=server.nextPendingConnection();
        auto incoming=std::make_shared<QByteArray>();
        QObject::connect(socket,&QTcpSocket::readyRead,socket,[socket,incoming,response]{
            *incoming+=socket->readAll();
            if(!incoming->contains("\r\n\r\n") || !incoming->endsWith("fixture-upload"))return;
            socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(response.size())+
                "\r\nConnection: close\r\n\r\n"+response);
            socket->disconnectFromHost();
        });
    });
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QString("http://127.0.0.1:%1/fixture").arg(server.serverPort())));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/octet-stream");
    const QByteArray body="fixture-upload";
    const auto before=sn::Meter::instance().totals();
    auto* reply=sn::watchReply(manager.post(request,body),body);
    QEventLoop loop;QTimer deadline;deadline.setSingleShot(true);
    QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    deadline.start(5000);loop.exec();
    assert(reply->isFinished() && reply->error()==QNetworkReply::NoError && reply->readAll()==response);
    const auto end=sn::Meter::instance().totals();
    assert(end.tx-before.tx==uint64_t(body.size()) && end.rx-before.rx==uint64_t(response.size()));
    delete reply;
    std::cout<<"Scoped thread CPU (not process CPU), nested scopes, shared buffers and local HTTP payload accounting passed\n";
}
