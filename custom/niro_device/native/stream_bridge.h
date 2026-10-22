// Local projection transport for the lab build, not a desktop screen recorder.
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QBuffer>
#include <QUrlQuery>
#include <QPointer>
#include <QElapsedTimer>
#include <QLibrary>
#include "gpu_readback.h"
#include "shared_gpu.h"
#include <QScopedValueRollback>

class ProjectionBridge : public QObject {
 QTcpServer server;
 QTimer timer;
 ProjectionReadback readback;
 std::unique_ptr<SharedGpuFrames> shared;
 QImage emptyOverlay;
 bool captureInProgress=false;
 QHash<QWidget*,QPair<QString,QImage>> controlCache;
 QImage controlOverlay;QString controlKey;
 QImage controls(){
  QString key=QString::number(scene->width())+":"+QString::number(scene->height());
  auto children=scene->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly);
  for(auto c:children){auto b=qobject_cast<QPushButton*>(c);key+=QString::number(c->isVisible())+QString::number(c->x())+QString::number(c->y())+(b?b->text()+QString::number(b->isChecked())+QString::number(b->isDown()):QString());}
  if(controlOverlay.isNull()||key!=controlKey){controlKey=key;controlOverlay=QImage(1280,640,QImage::Format_RGBA8888_Premultiplied);controlOverlay.fill(Qt::transparent);QPainter p(&controlOverlay);p.scale(1280.0/scene->width(),640.0/scene->height());for(auto c:children)if(c->isVisible())c->render(&p,c->pos());}
  return controlOverlay;
 }
 QList<QPointer<QTcpSocket>> clients;
 QWidget *mainWindow, *settingsWindow;
 QOpenGLWidget *scene;
 QPointer<QWidget> pressed;
 QSize sourceSize;
 using ScaleFn=int (*)(const unsigned char*,int,int,int,unsigned char*,int,int,int,int);
 QLibrary yuvLibrary;
 ScaleFn scaleRgba=nullptr;
 using SwapFn=int (*)(const unsigned char*,int,unsigned char*,int,int,int);
 SwapFn swapChannels=nullptr;
 qint64 count=0, grabNs=0, scaleNs=0, overlayNs=0, encodeNs=0;
 public:
 ProjectionBridge(QWidget *main, QWidget *settings, QOpenGLWidget *view) : mainWindow(main), settingsWindow(settings), scene(view) {
  shared=std::make_unique<SharedGpuFrames>(scene,readback,this);
  emptyOverlay=QImage(1280,640,QImage::Format_RGBA8888_Premultiplied);emptyOverlay.fill(Qt::transparent);
  auto library=qgetenv("AA_LIBYUV");
  yuvLibrary.setFileName(library.isEmpty()?QString("/data/openpilot/custom/niro_device/release/libyuv.so.0"):QString::fromLocal8Bit(library));
  scaleRgba=reinterpret_cast<ScaleFn>(yuvLibrary.resolve("ARGBScale"));
  swapChannels=reinterpret_cast<SwapFn>(yuvLibrary.resolve("ARGBToABGR"));
  connect(&server,&QTcpServer::newConnection,this,[this]{
   while(server.hasPendingConnections()) {
    auto socket=server.nextPendingConnection();
    connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    connect(socket,&QTcpSocket::readyRead,this,[this,socket]{
     auto pending=socket->property("request").toByteArray()+socket->readAll();
     if(pending.size()>4096){socket->disconnectFromHost();return;}
     if(!pending.contains("\r\n\r\n")){socket->setProperty("request",pending);return;}
     if(socket->property("handled").toBool())return;
     socket->setProperty("handled",true);
     auto parts=pending.split('\n').first().trimmed().split(' ');
     if(parts.size()<2){socket->disconnectFromHost();return;}
     QUrl url=QUrl::fromEncoded(parts[1]);
     if(url.path()=="/stream" || url.path()=="/raw" || url.path()=="/rgba" || url.path()=="/nv12") {
      if(shared->active()){socket->write("HTTP/1.0 409 Conflict\r\nContent-Length: 0\r\n\r\n");socket->disconnectFromHost();return;}
      bool nv12=url.path()=="/nv12";
      for(auto c:clients)if(c&&c->state()==QAbstractSocket::ConnectedState&&c->property("nv12").toBool()!=nv12){socket->write("HTTP/1.0 409 Conflict\r\nContent-Length: 0\r\n\r\n");socket->disconnectFromHost();return;}
      socket->setProperty("nv12",nv12);
      socket->setProperty("raw",url.path()!="/stream");
      socket->setProperty("rgba",url.path()=="/rgba" || url.path()=="/nv12");
      socket->write("HTTP/1.0 200 OK\r\nContent-Type: application/octet-stream\r\nCache-Control: no-store\r\n\r\n");clients.append(socket);device()->resetInteractiveTimeout(60);
     } else if(url.path()=="/stats") {
      auto body=QByteArray("awake=")+QByteArray::number(device()->isAwake()?1:0)+" projection_dark="+QByteArray::number(niroDisplayDark?1:0)+" shared_frames="+QByteArray::number(qulonglong(shared->sent))+" shared_skips="+QByteArray::number(qulonglong(shared->skipped))+" source="+QByteArray::number(sourceSize.width())+"x"+QByteArray::number(sourceSize.height())+" frames="+QByteArray::number(count)+" grab_ms="+QByteArray::number(count?grabNs/1e6/count:0)+" scale_ms="+QByteArray::number(count?scaleNs/1e6/count:0)+" overlay_ms="+QByteArray::number(count?overlayNs/1e6/count:0)+" encode_ms="+QByteArray::number(count?encodeNs/1e6/count:0)+"\n";
      socket->write("HTTP/1.0 200 OK\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);socket->disconnectFromHost();
     } else if(url.path()=="/input") {
      QUrlQuery q(url);bool xok,yok,aok;
      auto x=q.queryItemValue("x").toInt(&xok),y=q.queryItemValue("y").toInt(&yok),action=q.queryItemValue("action").toInt(&aok);
      if(xok&&yok&&aok&&x>=0&&x<1280&&y>=0&&y<720&&(action==0||action==1||action==2)) {
       // Video is letterboxed 2160:1080 into 1280:720: 40px above/below.
       auto root=settingsWindow->isVisible()?settingsWindow:mainWindow;
       if(y>=40&&y<680){
        QPoint p(x*root->width()/1280,(y-40)*root->height()/640);
        QWidget *target=pressed?pressed.data():root->childAt(p);if(!target)target=root;
        if(action==0)pressed=target;
        auto local=target->mapFrom(root,p);auto global=root->mapToGlobal(p);
        QMouseEvent event(action==0?QEvent::MouseButtonPress:action==1?QEvent::MouseButtonRelease:QEvent::MouseMove,local,global,action==2?Qt::NoButton:Qt::LeftButton,action==1?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);
        const auto previousProjection=qApp->property("niroProjectionInput");qApp->setProperty("niroProjectionInput",true);
        QApplication::sendEvent(target,&event);qApp->setProperty("niroProjectionInput",previousProjection);if(action==1)pressed.clear();
       }
      }
      socket->write("HTTP/1.0 200 OK\r\nContent-Length: 2\r\n\r\nOK");socket->disconnectFromHost();
     } else {socket->write("HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n");socket->disconnectFromHost();}
    });
   }
  });
  if(!server.listen(QHostAddress::LocalHost,8779))qFatal("Projection port 8779 unavailable");
  auto capture=[this](bool gpu){
   if(captureInProgress)return;
   QScopedValueRollback<bool> capturing(captureInProgress,true);
   clients.erase(std::remove_if(clients.begin(),clients.end(),[](auto c){return !c||c->state()!=QAbstractSocket::ConnectedState;}),clients.end());
   if(clients.isEmpty())return;
   bool ready=false;for(auto c:clients)if(c&&c->bytesToWrite()==0)ready=true;
   if(!ready)return;
   device()->resetInteractiveTimeout(60);
   QElapsedTimer watch;watch.start();
   bool nv12=false;for(auto c:clients)if(c&&c->property("nv12").toBool())nv12=true;
   auto root=settingsWindow->isVisible()?settingsWindow:mainWindow;
   auto frame=settingsWindow->isVisible()?root->grab().toImage():gpu?readback.capture(scene,nv12,nv12?controls():QImage()):scene->grabFramebuffer();
   if(frame.isNull())return;
   sourceSize=scene->size();
   if(nv12&&gpu){grabNs+=watch.nsecsElapsed();++count;for(auto c:clients)if(c&&c->bytesToWrite()==0)c->write(reinterpret_cast<const char*>(frame.constBits()),frame.sizeInBytes());return;}
   grabNs+=watch.nsecsElapsed();watch.restart();
   if(frame.size()!=QSize(1280,640)) {
    if(scaleRgba){
     if(frame.depth()!=32)frame=frame.convertToFormat(QImage::Format_ARGB32);
     QImage scaled(1280,640,frame.format());
     if(scaleRgba(frame.constBits(),frame.bytesPerLine(),frame.width(),frame.height(),scaled.bits(),scaled.bytesPerLine(),1280,640,2)==0)frame=scaled;
     else frame=frame.scaled(1280,640,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    }else frame=frame.scaled(1280,640,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
   }
   scaleNs+=watch.nsecsElapsed();watch.restart();
   if(nv12){
    using Convert=int(*)(const unsigned char*,int,unsigned char*,int,unsigned char*,int,int,int);
    auto convert=reinterpret_cast<Convert>(yuvLibrary.resolve("ABGRToNV12"));
    if(!convert)return;auto rgba=frame.convertToFormat(QImage::Format_RGBA8888);QByteArray data(1280*640*3/2,0);
    if(convert(rgba.constBits(),rgba.bytesPerLine(),reinterpret_cast<unsigned char*>(data.data()),1280,reinterpret_cast<unsigned char*>(data.data())+1280*640,1280,1280,640))return;
    ++count;for(auto c:clients)if(c&&c->bytesToWrite()==0)c->write(data);return;
   }
   frame.setDevicePixelRatio(1);
   if(!settingsWindow->isVisible()) {
    QPainter p(&frame);p.scale(1280.0/scene->width(),640.0/scene->height());
    for(auto child:scene->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly)) if(child->isVisible()) {
     auto button=qobject_cast<QPushButton*>(child);
     QString key=QString::number(child->width())+":"+QString::number(child->height())+":"+(button?button->text()+QString::number(button->isChecked())+QString::number(button->isDown()):QString());
     auto &entry=controlCache[child];
     if(entry.first!=key){entry.first=key;entry.second=QImage(child->size(),QImage::Format_ARGB32_Premultiplied);entry.second.fill(Qt::transparent);QPainter cached(&entry.second);child->render(&cached);}
     p.drawImage(child->pos(),entry.second);
    }
   }
   overlayNs+=watch.nsecsElapsed();watch.restart();
   bool needJpeg=false,needRaw=false;
   for(auto c:clients)if(c&&c->bytesToWrite()==0){if(c->property("raw").toBool())needRaw=true;else needJpeg=true;}
   QByteArray jpeg,raw,rgba;
   bool needBgra=false;for(auto c:clients)if(c&&c->property("raw").toBool()&&!c->property("rgba").toBool())needBgra=true;
   if(needRaw){auto rgb=frame.convertToFormat(QImage::Format_RGBA8888);rgba=QByteArray(reinterpret_cast<const char*>(rgb.constBits()),rgb.bytesPerLine()*rgb.height());}
   if(needJpeg){QBuffer buffer(&jpeg);buffer.open(QIODevice::WriteOnly);frame.save(&buffer,"JPEG",85);}
   if(needBgra){
    QImage rgb=frame;
    bool bgra=frame.format()==QImage::Format_ARGB32||frame.format()==QImage::Format_RGB32||frame.format()==QImage::Format_ARGB32_Premultiplied;
    if(!bgra){
     if(swapChannels&&(frame.format()==QImage::Format_RGBA8888||frame.format()==QImage::Format_RGBX8888||frame.format()==QImage::Format_RGBA8888_Premultiplied)){
      rgb=QImage(1280,640,QImage::Format_ARGB32);
      if(swapChannels(frame.constBits(),frame.bytesPerLine(),rgb.bits(),rgb.bytesPerLine(),1280,640)!=0)rgb=frame.convertToFormat(QImage::Format_ARGB32);
     }else rgb=frame.convertToFormat(QImage::Format_ARGB32);
    }
    raw=QByteArray(reinterpret_cast<const char*>(rgb.constBits()),rgb.bytesPerLine()*rgb.height());
   }
   encodeNs+=watch.nsecsElapsed();++count;
   for(auto c:clients)if(c&&c->bytesToWrite()==0)c->write(c->property("raw").toBool()?(c->property("rgba").toBool()?rgba:raw):jpeg);
  };
  connect(scene,&QOpenGLWidget::aboutToCompose,this,[this,capture]{if(!settingsWindow->isVisible()){if(shared->active())shared->capture(controls());else capture(true);}});
  connect(&timer,&QTimer::timeout,this,[this,capture]{if(settingsWindow->isVisible()){
   if(shared->active()){
    auto frame=settingsWindow->grab().toImage();QImage scaled(1280,640,frame.format());
    if(scaleRgba&&frame.depth()==32&&scaleRgba(frame.constBits(),frame.bytesPerLine(),frame.width(),frame.height(),scaled.bits(),scaled.bytesPerLine(),1280,640,2)==0)frame=scaled;
    else frame=frame.scaled(1280,640,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    frame=frame.convertToFormat(QImage::Format_RGBA8888);shared->capture(emptyOverlay,&frame);
   }else capture(false);
  }});
  timer.setTimerType(Qt::PreciseTimer);timer.start(16);
 }
};
