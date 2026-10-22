#pragma once
#include <QApplication>
#include <QElapsedTimer>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QTouchEvent>
#include "selfdrive/ui/qt/onroad/alerts.h"

// Only backlight output is suppressed. Rendering and Android Auto are independent.
inline bool niroDisplayDark = false;

class NiroDisplaySleep final : public QObject {
 QWidget *scene;
 QPushButton *button;
 QTimer timer;
 QElapsedTimer clock;
 qint64 deadline=0, swallowMouseUntil=0;
 bool consumeTouch=false, consumeMouse=false;
 int lastRemaining=-1;
 QIcon moon;
 void place() {button->setGeometry(1410*scene->width()/2160,35*scene->height()/1080,85*scene->width()/2160,85*scene->height()/1080);}
 void showState() {
  int remaining=deadline?int(std::max<qint64>(1,(deadline-clock.elapsed()+999)/1000)):0;
  if(remaining==lastRemaining)return;lastRemaining=remaining;
  QString text=remaining?QString::number(remaining):QString();
  if(button->text()!=text)button->setText(text);
  button->setIcon(remaining?QIcon():moon);
  button->setAccessibleDescription(remaining?QString("Display wird in %1 Sekunden dunkel. Erneut tippen zum Abbrechen.").arg(remaining):"Display nach fünf Sekunden abdunkeln. Berühren zum Aufwecken.");
 }
 void wake() {niroDisplayDark=false;deadline=0;device()->resetInteractiveTimeout(60);showState();}
public:
 explicit NiroDisplaySleep(QWidget *view):QObject(view),scene(view),button(new QPushButton(view)) {
  clock.start();button->setObjectName("niroDisplaySleep");button->setAccessibleName("Display abdunkeln");
  button->setToolTip("Display in 5 Sekunden aus · Berühren zum Aufwecken");
  button->setStyleSheet("QPushButton{background:transparent;border:none;color:#718b98;font-size:28px;}");
  button->setIconSize(QSize(40,40));button->setAttribute(Qt::WA_AcceptTouchEvents);
  QPixmap icon(64,64);icon.fill(Qt::transparent);{QPainter p(&icon);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#718b98"),5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));QPainterPath path;path.moveTo(43,10);path.cubicTo(5,3,0,52,33,55);path.cubicTo(46,57,56,47,57,37);path.cubicTo(29,47,20,21,43,10);p.drawPath(path);}moon=QIcon(icon);
  connect(button,&QPushButton::clicked,this,[this]{if(deadline||niroDisplayDark)wake();else{deadline=clock.elapsed()+5000;device()->resetInteractiveTimeout(60);showState();}});
  connect(&timer,&QTimer::timeout,this,[this]{
   if(deadline||niroDisplayDark){
    device()->resetInteractiveTimeout(60);
    // Reuse visible native warnings, including computed timeout warnings.
    for(auto *alert:scene->window()->findChildren<OnroadAlerts*>())if(alert->isVisible()){wake();return;}
   }
   if(deadline && clock.elapsed()>=deadline){deadline=0;niroDisplayDark=true;}
   showState();
  });
  scene->installEventFilter(this);qApp->installEventFilter(this);place();showState();button->show();timer.start(100);
 }
 ~NiroDisplaySleep() override {niroDisplayDark=false;qApp->removeEventFilter(this);}
 bool eventFilter(QObject *object,QEvent *event) override {
  if(object==scene && event->type()==QEvent::Resize)place();
  // Headunit input can operate the dashboard but must not wake the local panel.
  if(qApp->property("niroProjectionInput").toBool())return false;
  auto type=event->type();auto now=clock.elapsed();
  bool touch=type==QEvent::TouchBegin||type==QEvent::TouchUpdate||type==QEvent::TouchEnd||type==QEvent::TouchCancel;
  bool mouse=type==QEvent::MouseButtonPress||type==QEvent::MouseButtonRelease||type==QEvent::MouseMove;
  if(touch){
   if(type==QEvent::TouchBegin && niroDisplayDark){wake();consumeTouch=true;swallowMouseUntil=now+600;}
   if(consumeTouch){if(type==QEvent::TouchEnd||type==QEvent::TouchCancel){consumeTouch=false;swallowMouseUntil=now+600;}event->accept();return true;}
   if(deadline)deadline=now+5000;
   // This new control is outside RoadScene's existing touch-button registry.
   if(object==button){if(type==QEvent::TouchEnd)button->click();event->accept();return true;}
  }
  if(mouse){
   if(now<swallowMouseUntil){event->accept();return true;}
   if(type==QEvent::MouseButtonPress && niroDisplayDark){wake();consumeMouse=true;}
   if(consumeMouse){if(type==QEvent::MouseButtonRelease)consumeMouse=false;event->accept();return true;}
   if(deadline && (type==QEvent::MouseButtonPress||type==QEvent::MouseButtonRelease))deadline=now+5000;
  }
  return QObject::eventFilter(object,event);
 }
};
