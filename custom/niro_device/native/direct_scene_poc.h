#include "benchmark_flags.h"
#pragma once
#include <QOpenGLWindow>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QFile>
#include <QElapsedTimer>
#include <QTimer>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <QRect>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>
#include <memory>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include "niro_scene_mesh.h"
#include "gpu_timing.h"
#include "presentation_timing.h"
class DirectScenePoc:public QOpenGLWindow,protected QOpenGLFunctions {
 NiroSceneMesh mesh;NiroGpuTiming timing;NiroPresentationTiming presentationTiming;QElapsedTimer elapsed,measure;int frames=0;double submission=0;
public:
 DirectScenePoc():QOpenGLWindow(NoPartialUpdate){
  auto format=QSurfaceFormat::defaultFormat();format.setDepthBufferSize(24);format.setStencilBufferSize(8);setFormat(format);
  setTitle("Niro direct rendering POC");setFlags(Qt::Window|Qt::FramelessWindowHint);setMinimumSize(QSize(2160,1080));setMaximumSize(QSize(2160,1080));resize(2160,1080);
  connect(this,&QOpenGLWindow::frameSwapped,this,[this]{if(isExposed())update();});
 }
 void showLandscape(){show();auto *native=QGuiApplication::platformNativeInterface();
  auto *surface=static_cast<wl_surface*>(native->nativeResourceForWindow("surface",this));
  if(surface){wl_surface_set_buffer_transform(surface,WL_OUTPUT_TRANSFORM_270);wl_surface_commit(surface);}
  showFullScreen();resize(2160,1080);
 }
protected:
 void initializeGL()override{initializeOpenGLFunctions();mesh.initialize();mesh.setSunLight(QVector3D(-3,5,4).normalized(),{1,1,1},1,1);timing.initialize();elapsed.start();measure.start();
  fprintf(stderr,"DIRECT_POC renderer=%s interval=%d\n",glGetString(GL_RENDERER),context()->format().swapInterval());
  QTimer::singleShot(3000,this,[this]{grabFramebuffer().save("/data/local/niro-device-candidate/direct-preview.png");});
 }
 void paintGL()override{
  QElapsedTimer cost;cost.start();timing.begin();int w=width()*devicePixelRatio(),h=height()*devicePixelRatio();
  glViewport(0,0,w,h);glDisable(GL_SCISSOR_TEST);glClearColor(.075,.11,.14,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  const bool inspectDoors=niroBenchmarkFlag("poc-inspect-doors");
  float t=inspectDoors?3.f:elapsed.elapsed()/1000.f;if(inspectDoors)mesh.setDoors({1,1,1,1,1});QVector3D eye(6.8f*std::cos(t*.2f),3.6f,6.8f*std::sin(t*.2f));QMatrix4x4 view,projection;view.lookAt(eye,{0,.7f,0},{0,1,0});projection.perspective(34,float(w)/h,.1f,100.f);
  mesh.draw(projection*view,eye,12*std::sin(t),w,h,{},0,true,1,{},false,1.5*std::sin(t*2),.5*std::cos(t),.02*std::sin(t*2),{t*80,t*80,t*80,t*80});
  QOpenGLPaintDevice paintDevice(QSize(w,h));QPainter p(&paintDevice);p.setPen(QColor("#d5e5eb"));p.setFont(QFont("Inter",28));p.drawText(QPoint(70,75),niroBenchmarkFlag("poc-small-model")?"POC · DIREKTE 3D-AUSGABE · 408 DREIECKE":"POC · DIREKTE 3D-AUSGABE · VOLLSTÄNDIGER NIRO");p.end();
  timing.end();presentationTiming.request(this);submission+=cost.nsecsElapsed()/1e6;
  if(++frames==120){fprintf(stderr,"DIRECT_POC fps=%.2f submit=%.3fms\n",frames*1000./measure.elapsed(),submission/frames);frames=0;submission=0;measure.restart();}
 }
};
