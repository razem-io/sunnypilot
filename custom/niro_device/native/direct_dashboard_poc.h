#pragma once
#include "direct_scene_poc.h"
#include "road_scene.h"
// Isolated, view-only offroad benchmark of the complete dashboard paint path.
// Production input/settings/alert paths stay in the original widget UI.
class DirectDashboardPoc:public QOpenGLWindow {
 RoadScene &scene;NiroPresentationTiming presentationTiming;
public:
 explicit DirectDashboardPoc(RoadScene &s):QOpenGLWindow(NoPartialUpdate),scene(s){
  auto format=QSurfaceFormat::defaultFormat();format.setDepthBufferSize(24);format.setStencilBufferSize(8);setFormat(format);
  setTitle("Niro dashboard rendering POC");setFlags(Qt::Window|Qt::FramelessWindowHint);setMinimumSize({2160,1080});setMaximumSize({2160,1080});resize(2160,1080);
  connect(this,&QOpenGLWindow::frameSwapped,this,[this]{if(isExposed())update();});
 }
 void showLandscape(){show();auto *native=QGuiApplication::platformNativeInterface();auto *surface=static_cast<wl_surface*>(native->nativeResourceForWindow("surface",this));if(surface){wl_surface_set_buffer_transform(surface,WL_OUTPUT_TRANSFORM_270);wl_surface_commit(surface);}showFullScreen();resize(2160,1080);}
protected:
 void initializeGL()override{scene.initializeExternal();QTimer::singleShot(3000,this,[this]{grabFramebuffer().save("/data/local/niro-device-candidate/dashboard-poc-preview.png");});}
 void paintGL()override{QOpenGLPaintDevice target(QSize(width()*devicePixelRatio(),height()*devicePixelRatio()));scene.renderExternal(&target);presentationTiming.request(this);}
};
