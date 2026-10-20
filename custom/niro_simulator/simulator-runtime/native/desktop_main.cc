#include <QApplication>
#include <QStackedLayout>
#include <QShortcut>
#include <QDir>
#include <QFontDatabase>
#include "selfdrive/ui/sunnypilot/qt/offroad/settings/settings.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/sunnypilot/ui.h"
#include "selfdrive/ui/qt/onroad/alerts.h"
#include "road_scene.h"

class ClickTrace : public QObject {
 bool eventFilter(QObject *o,QEvent *e) override {
  if(e->type()==QEvent::MouseButtonPress){auto *m=static_cast<QMouseEvent*>(e);auto *w=qobject_cast<QWidget*>(o);fprintf(stderr,"CLICK %s local %.0f %.0f global %.0f %.0f widget %d %d %d %d\n",o->metaObject()->className(),m->localPos().x(),m->localPos().y(),m->screenPos().x(),m->screenPos().y(),w?w->x():0,w?w->y():0,w?w->width():0,w?w->height():0);fflush(stderr);}
  return false;
 }
};
int main(int argc,char **argv) {
  qInstallMessageHandler(swagLogMessageHandler);
  QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  setQtSurfaceFormat();
  QDir::setCurrent(qEnvironmentVariable("NIRO_RUNTIME")+"/source/selfdrive/ui");
  QApplication app(argc,argv);ClickTrace trace;if(qEnvironmentVariableIntValue("NIRO_TRACE_CLICKS"))app.installEventFilter(&trace);app.setApplicationName("Niro Native Replay");
  QWidget window;window.setWindowTitle("Niro Dashboard · Native Replay");
  auto layout=new QStackedLayout(&window);layout->setContentsMargins(0,0,0,0);layout->setStackingMode(QStackedLayout::StackAll);
  auto pages=new QStackedWidget(&window);layout->addWidget(pages);
  auto scene=new RoadScene(pages);pages->addWidget(scene);
  auto settings=new SettingsWindowSP(nullptr);settings->setWindowFlag(Qt::Window);settings->setWindowTitle("Niro Einstellungen");settings->setWindowModality(Qt::NonModal);
  auto openSettings=[=,&window]{settings->setCurrentPanel(0);settings->setGeometry(window.geometry());settings->show();settings->raise();settings->activateWindow();};
  scene->setSettingsAction(openSettings);
  QObject::connect(settings,&SettingsWindow::closeSettings,pages,[=,&window]{settings->hide();pages->setCurrentWidget(scene);window.raise();window.activateWindow();});
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Regular.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/Inter-Bold.ttf");
  auto alerts=new OnroadAlerts(&window);alerts->setAttribute(Qt::WA_TransparentForMouseEvents);layout->addWidget(alerts);alerts->raise();
  QObject::connect(uiStateSP(),&UIState::uiUpdate,alerts,[=](const UIState&s){if(s.scene.started)alerts->updateState(s);else alerts->clear();
    auto size=(*s.sm)["selfdriveState"].getSelfdriveState().getAlertSize();
    if(!s.scene.started||size==cereal::SelfdriveState::AlertSize::NONE)alerts->setMask(QRegion(0,0,1,1));
    else alerts->clearMask();});
  auto escape=new QShortcut(QKeySequence(Qt::Key_Escape),&window);QObject::connect(escape,&QShortcut::activated,&window,[=,&window]{settings->hide();pages->setCurrentWidget(scene);window.raise();window.activateWindow();});
  auto settingsEscape=new QShortcut(QKeySequence(Qt::Key_Escape),settings);QObject::connect(settingsEscape,&QShortcut::activated,&window,[=,&window]{settings->hide();window.raise();window.activateWindow();});
  auto cameraKey=new QShortcut(QKeySequence(Qt::Key_C),&window);QObject::connect(cameraKey,&QShortcut::activated,scene,[=]{scene->toggleCamera();});
  auto settingsKey=new QShortcut(QKeySequence(Qt::Key_M),&window);QObject::connect(settingsKey,&QShortcut::activated,scene,openSettings);
  window.resize(2160,1080);window.show();return app.exec();
}
