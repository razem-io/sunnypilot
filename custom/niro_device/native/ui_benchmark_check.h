#pragma once
#include <QApplication>
#include <QPushButton>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QTimer>
#include "dashboard_alerts.h"
// A bounded, offroad UI regression sequence. No messages are published, no
// vehicle data or settings are changed. Only enabled by the benchmark lease.
inline void niroRunUiCheck(QWidget *window){
 auto state=std::make_shared<bool>(true);
 auto record=[state](const char *name,bool okay){fprintf(stderr,"UI_CHECK %s %s\n",okay?"PASS":"FAIL",name);*state&=okay;};
 auto button=[window](const QString &name){for(auto *b:window->findChildren<QPushButton*>())if(b->isVisible()&&b->accessibleName()==name)return b;return static_cast<QPushButton*>(nullptr);};
 auto mouse=[](QPushButton *b){if(!b)return;QMouseEvent down(QEvent::MouseButtonPress,b->rect().center(),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(b,&down);QMouseEvent up(QEvent::MouseButtonRelease,b->rect().center(),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(b,&up);};
 auto touch=[](QPushButton *b){
  if(!b)return;static QTouchDevice device;device.setType(QTouchDevice::TouchScreen);device.setCapabilities(QTouchDevice::Position);device.setMaximumTouchPoints(3);
  QTouchEvent::TouchPoint point(0);QPointF pos=b->rect().center();point.setPos(pos);point.setStartPos(pos);point.setLastPos(pos);point.setScreenPos(b->mapToGlobal(pos.toPoint()));point.setStartScreenPos(point.screenPos());point.setState(Qt::TouchPointPressed);
  QTouchEvent down(QEvent::TouchBegin,&device,Qt::NoModifier,Qt::TouchPointPressed,{point});QApplication::sendEvent(b,&down);
  point.setState(Qt::TouchPointReleased);QTouchEvent up(QEvent::TouchEnd,&device,Qt::NoModifier,Qt::TouchPointReleased,{point});QApplication::sendEvent(b,&up);
 };
 auto capture=[window,record](const QString &name){bool ok=window->grab().save("/data/local/niro-device-candidate/ui-check-"+name+".png");record(qPrintable(name),ok);};
 QTimer::singleShot(5000,window,[=]{capture("00-dashboard");auto *b=button("Kamerabild einblenden");record("camera-button-present",b);mouse(b);record("mouse-camera-on",b&&b->isChecked());});
 QTimer::singleShot(7000,window,[=]{capture("01-camera");auto *b=button("Kamerabild einblenden");touch(b);record("touch-camera-off",b&&!b->isChecked());});
 QTimer::singleShot(9000,window,[=]{auto *b=button("Einstellungen");record("settings-button-present",b);mouse(b);});
 QTimer::singleShot(11000,window,[=]{capture("02-settings");QPushButton *close=nullptr;for(auto *b:window->findChildren<QPushButton*>())if(b->isVisible()&&b->text()==QString::fromUtf8("×")){close=b;break;}record("settings-visible-with-close",close);mouse(close);});
 QTimer::singleShot(13000,window,[=]{capture("03-returned");record("dashboard-restored",button("Kamerabild einblenden")&&button("Einstellungen"));});
 QTimer::singleShot(15000,window,[=]{NiroDashboardAlerts *alerts=nullptr;for(auto *child:window->findChildren<QWidget*>())if(auto *a=dynamic_cast<NiroDashboardAlerts*>(child)){alerts=a;break;}record("alert-layer-found",alerts);if(alerts)alerts->showBenchmarkAlert();});
 QTimer::singleShot(16000,window,[=]{capture("04-alert");bool visible=false;for(auto *child:window->findChildren<QWidget*>())if(auto *a=dynamic_cast<NiroDashboardAlerts*>(child))visible=a->isVisible();record("full-alert-visible",visible);});
 QTimer::singleShot(19000,window,[=]{capture("05-cleared");bool hidden=false;for(auto *child:window->findChildren<QWidget*>())if(auto *a=dynamic_cast<NiroDashboardAlerts*>(child))hidden=a->isHidden();record("empty-alert-hidden",hidden);fprintf(stderr,"UI_CHECK COMPLETE %s\n",*state?"PASS":"FAIL");});
}
