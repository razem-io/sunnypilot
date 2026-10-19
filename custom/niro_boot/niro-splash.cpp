#include <QApplication>
#include <QWidget>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>
#include <QPainter>
#include <QElapsedTimer>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QDateTime>
#include <QtEndian>
#include <ctime>
#include <cstdio>

static QString root;
static void log(const QString &s) { fprintf(stderr,"%s %s\n",qPrintable(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)),qPrintable(s)); fflush(stderr); }
static QByteArray read(const QString &p) { QFile f(p); return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray(); }
static double clockSeconds(clockid_t id) { timespec ts{};clock_gettime(id,&ts);return ts.tv_sec+ts.tv_nsec/1e9; }
static int readyUi() {
  for (const QString &p:QDir("/proc").entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
    bool number=false; int pid=p.toInt(&number); if(!number)continue;
    if(read("/proc/"+p+"/comm").trimmed()!="ui")continue;
    if(!QFileInfo("/proc/"+p+"/exe").symLinkTarget().endsWith("/selfdrive/ui/ui"))continue;
    QByteArray b=read("/dev/shm/wd_"+p);
    if(b.size()!=8)continue;
    double t=qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(b.constData()))/1e9;
    double a=clockSeconds(CLOCK_BOOTTIME)-t, c=clockSeconds(CLOCK_MONOTONIC)-t;
    if((a>=0 && a<3)||(c>=0 && c<3))return pid;
  }
  return 0;
}
class Splash:public QWidget {
public:
  QTimer watch, animation; QElapsedTimer elapsed; QImage current;
  bool test=false, shown=false, finished=false, graphicLogged=false, textLogged=false; int lastFrame=-1;
  explicit Splash(bool t):test(t) {
    setWindowTitle("Kia Niro EV Startup");
    setWindowFlags(Qt::Window|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);setCursor(Qt::BlankCursor);setFixedSize(2160,1080);
    connect(&watch,&QTimer::timeout,this,[this]{
      if(!test && readyUi()){log("HANDOFF: sunnypilot UI heartbeat ready");QApplication::quit();}
    });watch.start(100);
    animation.setTimerType(Qt::PreciseTimer);
    connect(&animation,&QTimer::timeout,this,[this]{tick();});
  }
  void showLandscape(){
    // Match setMainWindow in the installed sunnypilot native UI exactly.
    show();
    auto native=QGuiApplication::platformNativeInterface();
    auto surface=static_cast<wl_surface*>(native->nativeResourceForWindow("surface",windowHandle()));
    if(!surface){log("FAIL_OPEN: Wayland surface unavailable");QApplication::exit(1);return;}
    wl_surface_set_buffer_transform(surface,WL_OUTPUT_TRANSFORM_270);
    wl_surface_commit(surface);
    setWindowState(Qt::WindowFullScreen);show();
    log("LANDSCAPE: 2160x1080 buffer, Wayland transform=270");
  }
  void tick(){
    if(!test && !shown && readyUi()){log("HANDOFF: UI ready before frame");QApplication::quit();return;}
    // Wall-clock selection skips late frames instead of stretching the timing.
    double seconds=elapsed.nsecsElapsed()/1e9;
    if(seconds>=9.6){hold();return;}
    if(seconds>=4.3 && !graphicLogged){graphicLogged=true;log("GRAPHIC_COMPLETE elapsed_ms="+QString::number(elapsed.elapsed()));}
    if(seconds>=7.8 && !textLogged){textLogged=true;log("TEXT_FADE_STARTED elapsed_ms="+QString::number(elapsed.elapsed()));}
    int index=int(seconds*30);
    if(index>=130 && index<234)index=130;
    if(index==lastFrame)return;
    current=QImage(root+"/frames/"+QString::number(index).rightJustified(3,'0')+".jpg");
    if(current.isNull()){log("FAIL_OPEN: animation frame unavailable");QApplication::exit(1);return;}
    lastFrame=index;
    if(!shown){shown=true;if(test)show();else showLandscape();log("REVEAL_STARTED");}
    update();
    if(test && index==130)current.save(root+"/test/graphic-complete.png");
    if(test && index>=234 && index<=237)current.save(root+"/test/text-start.png");
    if(test && index>=260 && index<=264)current.save(root+"/test/text-midpoint.png");
  }
  void hold(){
    animation.stop();current=QImage(root+"/final-hold.png");
    if(current.isNull()){log("FAIL_OPEN: hold asset unavailable");QApplication::exit(1);return;}
    finished=true;
    if(!test && readyUi()){log("HANDOFF: UI ready before hold");QApplication::quit();return;}
    if(!shown){shown=true;if(test)show();else showLandscape();}
    update();log("STATIC_HOLD: no looping; elapsed_ms="+QString::number(elapsed.isValid()?elapsed.elapsed():0));
    if(test){current.save(root+"/test/hold.png");QTimer::singleShot(2500,[]{log("TEST_HOLD_COMPLETE");QApplication::quit();});}
  }
  void start(bool replayed){
    if(replayed){log("ALREADY_PLAYED_THIS_BOOT: holding final only");hold();return;}
    elapsed.start();tick();animation.start(33);
  }
protected:
  void paintEvent(QPaintEvent*)override {QPainter p(this);p.fillRect(rect(),Qt::black);if(!current.isNull()){QSize size=current.size();size.scale(this->size(),Qt::KeepAspectRatio);QRect dst(QPoint((width()-size.width())/2,(height()-size.height())/2),size);p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(dst,current);}}
  void mousePressEvent(QMouseEvent*)override {log("DISMISSED_BY_TOUCH");QApplication::quit();}
};
int main(int argc,char **argv){
  bool test=false,check=false;
  for(int i=1;i<argc;i++){test|=QString(argv[i])=="--test";check|=QString(argv[i])=="--check";}
  if(test||check)qputenv("QT_QPA_PLATFORM","offscreen");
  QApplication app(argc,argv);
  root=QCoreApplication::applicationDirPath();
  if(check){log("READY_UI_PID="+QString::number(readyUi()));return 0;}
  if(test){QDir().mkpath(root+"/test");Splash s(true);s.start(false);return app.exec();}
  QLockFile lock("/dev/shm/niro-boot.lock");lock.setStaleLockTime(0);
  if(!lock.tryLock()){log("ALREADY_RUNNING: exiting");return 0;}
  QString boot=QString::fromLatin1(read("/proc/sys/kernel/random/boot_id").trimmed());
  if(boot.isEmpty()){log("FAIL_OPEN: boot id missing");return 1;}
  QString marker="/dev/shm/niro-boot-played-"+boot;
  bool played=QFile::exists(marker);
  QFile mark(marker);if(!played && !mark.open(QIODevice::WriteOnly|QIODevice::NewOnly)){log("FAIL_OPEN: cannot create boot marker");return 1;}mark.close();
  if(readyUi()){log("UI_ALREADY_READY: no splash shown");return 0;}
  Splash s(false);s.start(played);return app.exec();
}
