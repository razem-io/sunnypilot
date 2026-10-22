#include "benchmark_flags.h"
#include <sys/resource.h>
#include <unistd.h>
#include <cstdlib>

#include <QApplication>
#include <QTranslator>
#include <QDir>
#include "direct_scene_poc.h"
#include "direct_dashboard_poc.h"
#include "ui_benchmark_check.h"
#include "display_sleep.h"

#include "system/hardware/hw.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/window.h"

#ifdef SUNNYPILOT
#include "selfdrive/ui/sunnypilot/qt/window.h"
#define MainWindow MainWindowSP
#else
#include "selfdrive/ui/qt/qt_window.h"
#endif

#include "stream_bridge.h"
int main(int argc, char *argv[]) {
  // Qt's desktop GL dependency must not win symbol resolution over Adreno GLES.
  // Re-exec before creating any Qt graphics objects so the loader applies this.
  if (!std::getenv("NIRO_ADRENO_READY")) {
    setenv("NIRO_ADRENO_READY", "1", 1);
    setenv("LD_PRELOAD", "/usr/lib/aarch64-linux-gnu/libEGL_adreno.so:/usr/lib/aarch64-linux-gnu/libGLESv2_adreno.so", 1);
    execv("/proc/self/exe", argv);
    return 1;
  }
  setpriority(PRIO_PROCESS, 0, -20);

  qInstallMessageHandler(swagLogMessageHandler);
  qputenv("NIRO_RUNTIME", "/data/openpilot/custom/niro_device");
  qputenv("NIRO_ASSETS_ROOT", "/data/openpilot/custom/niro_device");
  if(niroBenchmarkFlag("profile"))qputenv("NIRO_PROFILE","1");else qunsetenv("NIRO_PROFILE");
  initApp(argc, argv);
  // Diagnostic only: separate Wayland swap waiting from scene rendering.
  if(niroBenchmarkFlag("poc-no-swap-wait")){
    auto fmt=QSurfaceFormat::defaultFormat();fmt.setSwapInterval(0);QSurfaceFormat::setDefaultFormat(fmt);
  }
  QDir::setCurrent("/data/openpilot/selfdrive/ui");

  QTranslator translator;
  QString translation_file = QString::fromStdString(Params().get("LanguageSetting"));
  if (!translator.load(QString(":/%1").arg(translation_file)) && translation_file.length()) {
    qCritical() << "Failed to load translation file:" << translation_file;
  }

  if(niroBenchmarkFlag("poc-zero-idle"))qputenv("QT_QPA_UPDATE_IDLE_TIME","0");
  if(niroRenderOption("poc-no-msaa",true)){
    auto fmt=QSurfaceFormat::defaultFormat();fmt.setSamples(0);QSurfaceFormat::setDefaultFormat(fmt);
  }
  if(niroBenchmarkFlag("poc-msaa2")){auto fmt=QSurfaceFormat::defaultFormat();fmt.setSamples(2);QSurfaceFormat::setDefaultFormat(fmt);}
  QApplication a(argc, argv);
  a.installTranslator(&translator);

  MainWindow w;
  if(!niroBenchmarkFlag("poc-direct-dashboard"))setMainWindow(&w);
  a.installEventFilter(&w);
  if(qEnvironmentVariableIsSet("NIRO_PROFILE"))device()->resetInteractiveTimeout(600);
  std::unique_ptr<DirectScenePoc> poc;
  if(niroBenchmarkFlag("poc-direct-window")){
    w.hide();poc=std::make_unique<DirectScenePoc>();poc->showLandscape();
  }
  std::unique_ptr<DirectDashboardPoc> dashboardPoc;
  if(niroBenchmarkFlag("poc-direct-dashboard")){
    RoadScene *scene=nullptr;for(auto *widget:w.findChildren<QOpenGLWidget*>())if(auto *road=dynamic_cast<RoadScene*>(widget)){scene=road;break;}
    if(scene){w.hide();dashboardPoc=std::make_unique<DirectDashboardPoc>(*scene);dashboardPoc->showLandscape();}
  }
  if(niroBenchmarkFlag("poc-ui-check"))niroRunUiCheck(&w);
  QTimer benchmarkExpiry;bool benchmarkExpired=false;
  if(niroBenchmarkLeaseValid()){
    QObject::connect(&benchmarkExpiry,&QTimer::timeout,[&benchmarkExpired]{if(!niroBenchmarkLeaseValid()){benchmarkExpired=true;qWarning("Benchmark lease ended; returning to normal UI");QCoreApplication::quit();}});benchmarkExpiry.start(5000);
  }
  std::unique_ptr<NiroDisplaySleep> displaySleep;
  for(auto *widget:w.findChildren<QOpenGLWidget*>())if(dynamic_cast<RoadScene*>(widget)){displaySleep=std::make_unique<NiroDisplaySleep>(widget);break;}
  RoadScene *projectionScene=nullptr;
  for(auto *widget:w.findChildren<QOpenGLWidget*>()) if(auto *road=dynamic_cast<RoadScene*>(widget)){projectionScene=road;break;}
  auto projectionSettings=w.findChild<SettingsWindow*>();
  std::unique_ptr<ProjectionBridge> projection;
  if(projectionScene && projectionSettings) projection=std::make_unique<ProjectionBridge>(&w,projectionSettings,projectionScene);
  else qFatal("Projection scene/settings missing");
  const int result=a.exec();
  if(benchmarkExpired){unsetenv("NIRO_PROFILE");execl("/data/openpilot/selfdrive/ui/ui","/data/openpilot/selfdrive/ui/ui",nullptr);}
  return result;
}
