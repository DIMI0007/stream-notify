#pragma once
#include <QCheckBox>
#include <QFontMetrics>
#include <algorithm>
#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <QPixmap>
#include <QVBoxLayout>
#include <QLabel>

class StreamSwitch final : public QCheckBox {
public:
    explicit StreamSwitch(const QString& title,QWidget* parent=nullptr):QCheckBox(title,parent) {
        setCursor(Qt::PointingHandCursor);
        QSizePolicy policy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        policy.setHeightForWidth(true);setSizePolicy(policy);
    }
    QSize sizeHint() const override {return QSize(180,heightForWidth(180));}
    QSize minimumSizeHint() const override {return QSize(120,24);}
    bool hasHeightForWidth() const override {return true;}
    int heightForWidth(int width) const override {
        return std::max(24,fontMetrics().boundingRect(QRect(0,0,std::max(40,width-42),1000),
            Qt::TextWordWrap,text()).height()+4);
    }
protected:
    bool hitButton(const QPoint& point) const override {return rect().contains(point);}
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.setOpacity(isEnabled()?1.0:0.45);
        const QRectF track(0,(height()-18)/2.0,32,18);
        p.setPen(QPen(isChecked()?QColor("#04de89"):QColor("#46536b"),1));
        p.setBrush(isChecked()?QColor("#00bd70"):QColor("#202a3a"));
        p.drawRoundedRect(track,9,9);
        p.setPen(Qt::NoPen);p.setBrush(QColor("#f0f5ff"));
        p.drawEllipse(QPointF(isChecked()?23:9,height()/2.0),7,7);
        p.setPen(QColor("#e3e9fb"));
        p.drawText(QRect(42,0,std::max(0,width()-42),height()),Qt::AlignVCenter|Qt::TextWordWrap,text());
        if(hasFocus()) {p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#7467ff"),1,Qt::DotLine));p.drawRoundedRect(rect().adjusted(1,1,-1,-1),4,4);}
    }
};

class ServiceBadge final : public QWidget {
public:
    explicit ServiceBadge(const QString& name,QWidget* parent=nullptr):QWidget(parent),name_(name) {
        setFixedSize(28,28);setAccessibleName(name);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.scale(width()/32.0,height()/32.0);
        QColor color=name_=="Telegram"?QColor("#009aef"):name_=="Discord"?QColor("#5865f2"):
            name_=="YouTube"?QColor("#ff1538"):name_=="Twitch"?QColor("#9146ff"):
            name_=="DonationAlerts"?QColor("#f47e20"):name_=="DonatePay"?QColor("#10bb7c"):QColor("#06382c");
        p.setPen(Qt::NoPen);p.setBrush(color);p.drawRoundedRect(QRectF(0,0,32,32),9,9);
        p.setBrush(Qt::white);
        if(name_=="Telegram") {
            QPainterPath plane;plane.moveTo(5,14);plane.lineTo(27,6);plane.lineTo(22,26);
            plane.lineTo(15,20);plane.lineTo(11,24);plane.lineTo(11,18);plane.closeSubpath();p.drawPath(plane);
            p.setPen(QPen(color,1));p.drawLine(QPointF(11,18),QPointF(23,10));
        } else if(name_=="YouTube") {
            p.drawRoundedRect(QRectF(5,8,22,16),5,5);p.setBrush(color);
            QPainterPath play;play.moveTo(13,11);play.lineTo(22,16);play.lineTo(13,21);play.closeSubpath();p.drawPath(play);
        } else if(name_=="Discord") {
            QPainterPath shape;shape.moveTo(8,9);shape.cubicTo(12,7,20,7,24,9);
            shape.lineTo(28,23);shape.lineTo(22,26);shape.lineTo(20,23);shape.lineTo(12,23);
            shape.lineTo(10,26);shape.lineTo(4,23);shape.closeSubpath();p.drawPath(shape);
            p.setBrush(color);p.drawEllipse(QPointF(12,17),2,2.5);p.drawEllipse(QPointF(20,17),2,2.5);
        } else if(name_=="Twitch") {
            QPainterPath bubble;bubble.moveTo(7,6);bubble.lineTo(26,6);bubble.lineTo(26,21);
            bubble.lineTo(20,27);bubble.lineTo(14,27);bubble.lineTo(10,30);bubble.lineTo(10,25);bubble.lineTo(7,25);bubble.closeSubpath();
            p.drawPath(bubble);p.setPen(QPen(color,2));p.drawLine(15,11,15,19);p.drawLine(21,11,21,19);
        } else if(name_=="Stream Notify") {
            p.setBrush(QColor("#00df8c"));p.drawEllipse(QPointF(16,16),3,3);
            p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#00df8c"),2.5,Qt::SolidLine,Qt::RoundCap));
            p.drawArc(QRectF(7,7,18,18),-50*16,100*16);p.drawArc(QRectF(7,7,18,18),130*16,100*16);
            p.drawArc(QRectF(3,3,26,26),-45*16,90*16);p.drawArc(QRectF(3,3,26,26),135*16,90*16);
        } else {
            QFont f=p.font();f.setPixelSize(17);f.setBold(true);p.setFont(f);p.setPen(Qt::white);
            p.drawText(QRectF(0,0,32,32),Qt::AlignCenter,name_=="DonationAlerts"?"D":"₽");
        }
    }
private: QString name_;
};

inline QIcon streamEyeIcon() {
    QPixmap pixmap(24,24);pixmap.fill(Qt::transparent);QPainter p(&pixmap);p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#bcc8ef"),1.6));p.setBrush(Qt::NoBrush);
    QPainterPath eye;eye.moveTo(3,12);eye.cubicTo(8,4,16,4,21,12);eye.cubicTo(16,20,8,20,3,12);
    p.drawPath(eye);p.drawEllipse(QPointF(12,12),3,3);return QIcon(pixmap);
}
inline void serviceSection(QVBoxLayout* layout,const QString& name) {
    auto* row=new QHBoxLayout;row->setSpacing(7);row->addWidget(new ServiceBadge(name));
    auto* title=new QLabel(name);title->setObjectName("Section");row->addWidget(title,1);layout->addLayout(row);
}
