// Generated desktop adaptation. See prepare_dashboard.py.
#include <QApplication>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QPainter>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QRegion>
#include <cmath>
#include <algorithm>
#include <functional>
#include <QTimer>
#include <QProcess>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QWindow>
#include <QSurfaceFormat>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QOpenGLVertexArrayObject>
#include <QProcessEnvironment>

#include <unistd.h>
#include <vector>
#include <memory>
static QString root;
struct Mesh{GLuint vertex=0,index=0;int count=0,material=0,wheel=0;};
class Dashboard:public QOpenGLWidget,protected QOpenGLFunctions{
 QOpenGLVertexArrayObject vao; QOpenGLShaderProgram shader;std::vector<Mesh> meshes;std::vector<std::unique_ptr<QOpenGLTexture>> textures;
 QProcess bridge;QTimer timer;QElapsedTimer received;QJsonObject data;QByteArray pending;QPointF dragPoint;bool dragging=false;float azimuth=-.711f,elevation=.315f;bool requested=true;bool permitted=false;QString lastStatus;QByteArray lastPaint;QString heartbeat;int frames=0;
public:
 std::function<void(bool)> selectView;
 void closeDashboard(){requested=false;permitted=false;if(selectView)selectView(false);}
 std::function<void(QString)> statusChanged;
 void openDashboard(){requested=true;fprintf(stderr,"INPUT: open resident dashboard\n");fflush(stderr);}
 Dashboard(bool standby):requested(!standby){setMinimumSize(1080,540);setWindowTitle("Niro dashboard");setAttribute(Qt::WA_AcceptTouchEvents);
 heartbeat=QString("/tmp/niro-desktop-dashboard-%1").arg(getpid());
 connect(&bridge,&QProcess::readyReadStandardOutput,this,[this]{pending+=bridge.readAllStandardOutput();int p;while((p=pending.indexOf('\n'))>=0){auto line=pending.left(p);pending.remove(0,p+1);QJsonParseError e;auto doc=QJsonDocument::fromJson(line,&e);if(e.error==QJsonParseError::NoError&&doc.isObject()){data=doc.object();received.restart();}}});
 connect(&bridge,&QProcess::readyReadStandardError,this,[this]{auto e=bridge.readAllStandardError();fprintf(stderr,"%s",e.constData());});
 received.start();bridge.setWorkingDirectory(qEnvironmentVariable("NIRO_RUNTIME"));bridge.start(qEnvironmentVariable("NIRO_PYTHON"),{qEnvironmentVariable("NIRO_RUNTIME")+"/native/bridge.py"});
 connect(&timer,&QTimer::timeout,this,[this]{QFile h(heartbeat);if(h.open(QIODevice::WriteOnly)){h.write("ok");h.close();}
  bool ok=requested&&received.elapsed()<300&&bridge.state()==QProcess::Running&&data.value("show").toBool(false);
  QString status=!ok?"stock":"dashboard";if(status!=lastStatus){lastStatus=status;QFile f(qEnvironmentVariable("NIRO_RUNTIME")+"/native-state");if(f.open(QIODevice::WriteOnly))f.write(status.toUtf8());fprintf(stderr,"VIEW: %s\n",qPrintable(status));fflush(stderr);}
  if(statusChanged)statusChanged(data.value("reason").toString()); if(selectView)selectView(ok); if(!ok){permitted=false;return;}permitted=true;
  {if(!isVisible()){if(frames==0){QFile warm(heartbeat+".warming");if(warm.open(QIODevice::WriteOnly))warm.write("initial graphics load");}show();lastPaint.clear();}auto next=QJsonDocument(data).toJson(QJsonDocument::Compact);if(next!=lastPaint){lastPaint=next;update();}}
 });timer.start(50);
 }
 ~Dashboard(){bridge.terminate();bridge.waitForFinished(500);QFile::remove(heartbeat);}
protected:
 void initializeGL()override{
  fprintf(stderr,"GRAPHICS: initialize begin\n");fflush(stderr);initializeOpenGLFunctions();vao.create();vao.bind();
  const char *vertex=R"(#version 330 core
   in vec3 position; in vec3 normal; in vec2 uv;
   uniform mat4 mvp; uniform mat4 model; out vec3 n; out vec3 world; out vec2 tex;
   void main(){n=mat3(model)*normal;world=(model*vec4(position,1.0)).xyz;tex=uv;gl_Position=mvp*vec4(world,1.0);}
  )";
  const char *fragment=R"(#version 330 core
   out vec4 fragmentColor;
   in vec3 n; in vec3 world; in vec2 tex;
   uniform sampler2D image; uniform vec4 tint; uniform float textured;
   uniform vec3 eye; uniform float paint;
   void main(){
    vec4 c=tint;if(textured>0.5)c*=texture(image,tex);if(c.a<0.1)discard;
    vec3 N=normalize(n);vec3 V=normalize(eye-world);
    vec3 key=normalize(vec3(-3.0,5.0,4.0));vec3 fill=normalize(vec3(3.0,2.0,-4.0));
    vec3 base=pow(c.rgb,vec3(2.2));
    vec3 diffuse=base*(0.55+0.85*max(dot(N,key),0.0)+0.65*max(dot(N,fill),0.0));
    float spec=pow(max(dot(N,normalize(key+V)),0.0),mix(24.0,70.0,paint));
    float rim=pow(1.0-max(dot(N,V),0.0),3.0);
    vec3 R=reflect(-V,N);
    float softbox=pow(max(dot(R,normalize(vec3(-0.4,1.0,0.5))),0.0),18.0);
    float strip=pow(max(dot(R,normalize(vec3(0.5,0.7,-0.7))),0.0),35.0);
    vec3 lit=diffuse+vec3(0.95,0.98,1.0)*spec*(0.18+paint*0.65)
      +paint*(vec3(0.65,0.77,0.88)*softbox*0.65+vec3(0.32,0.65,0.72)*strip*0.5+vec3(0.12,0.20,0.24)*rim);
    fragmentColor=vec4(pow(lit/(lit+vec3(0.65)),vec3(1.0/2.2)),1.0);
   }
  )";
  if(!shader.addShaderFromSourceCode(QOpenGLShader::Vertex,vertex)||!shader.addShaderFromSourceCode(QOpenGLShader::Fragment,fragment)||!shader.link())qFatal("Shader failed");
  textures.resize(4);for(int i=0;i<3;i++){QImage im(root+QString("/assets/%1.png").arg(i));if(!im.isNull()){textures[i]=std::make_unique<QOpenGLTexture>(im);textures[i]->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);textures[i]->setMagnificationFilter(QOpenGLTexture::Linear);}}
  QFile file(root+"/assets/niro-steering.mesh");if(!file.open(QIODevice::ReadOnly))qFatal("Mesh missing");auto read32=[&](){quint32 n;if(file.read(reinterpret_cast<char*>(&n),4)!=4)qFatal("Invalid mesh");return n;};auto count=read32();if(count>32)qFatal("Invalid mesh count");
  for(unsigned i=0;i<count;i++){auto nv=read32(),ni=read32(),material=read32();if(nv>1000000||ni>2000000||material>11)qFatal("Invalid mesh sizes");auto v=file.read(nv*32),indices=file.read(ni*4);if(v.size()!=int(nv*32)||indices.size()!=int(ni*4))qFatal("Truncated mesh");Mesh m;m.count=ni;m.material=material%4;m.wheel=material/4;glGenBuffers(1,&m.vertex);glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBufferData(GL_ARRAY_BUFFER,v.size(),v.data(),GL_STATIC_DRAW);glGenBuffers(1,&m.index);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size(),indices.data(),GL_STATIC_DRAW);meshes.push_back(m);}
 }
 void paintGL()override{
  vao.bind();glViewport(0,0,width()*devicePixelRatioF(),height()*devicePixelRatioF());glClearColor(.031,.047,.063,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
  QVector3D eye(10*std::sin(azimuth)*std::cos(elevation),.7+10*std::sin(elevation),10*std::cos(azimuth)*std::cos(elevation));QMatrix4x4 view,projection;view.lookAt(eye,QVector3D(0,.7,0),QVector3D(0,1,0));projection.perspective(34,2,.1,100);shader.bind();shader.setUniformValue("mvp",projection*view);shader.setUniformValue("image",0);shader.setUniformValue("eye",eye);
  bool wheelLive=data.value("wheelAngle").isDouble();bool steeringLive=data.value("steering").isDouble();
  float wheelAngle=wheelLive?data.value("wheelAngle").toDouble():0.f;
  wheelAngle=std::clamp(wheelAngle,-45.f,45.f);
  for(auto &m:meshes){QMatrix4x4 model;if(m.wheel){QVector3D pivot(-1.294,.3285,m.wheel==1?.79:-.79);model.translate(pivot);model.rotate(wheelAngle,0,1,0);model.translate(-pivot);}shader.setUniformValue("model",model);glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);for(auto pair:std::vector<std::pair<const char*,int>>{{"position",0},{"normal",12},{"uv",24}}){int loc=shader.attributeLocation(pair.first);shader.enableAttributeArray(loc);shader.setAttributeBuffer(loc,GL_FLOAT,pair.second,pair.second==24?2:3,32);}bool tex=bool(textures[m.material]);if(tex)textures[m.material]->bind(0);shader.setUniformValue("textured",tex?1.f:0.f);shader.setUniformValue("paint",m.material==3?1.f:0.f);shader.setUniformValue("tint",m.material==3?QVector4D(.43,.47,.51,1):m.material==1?QVector4D(.65,.65,.65,1):QVector4D(1,1,1,1));glDrawElements(GL_TRIANGLES,m.count,GL_UNSIGNED_INT,nullptr);}
  shader.release();glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);glDisable(GL_DEPTH_TEST);
  vao.release();QPainter p(this);p.scale(width()/2160.0,height()/1080.0);p.setRenderHint(QPainter::Antialiasing);auto text=[&](int x,int y,int size,QString s,QColor color=QColor("#e2edf1")){p.setPen(color);p.setFont(QFont("Inter",size));p.drawText(QRect(x,y,800,size*2),Qt::AlignLeft|Qt::AlignTop,s);};QColor muted("#8298a6"),cyan("#65d3e3");
  text(64,45,32,"KIA   NIRO EV");text(860,55,23,"REPLAY · LOKAL",cyan);
  p.setPen(QPen(QColor("#43838e"),2));p.setBrush(QColor("#17272f"));p.drawRoundedRect(QRect(1770,30,320,105),12,12);text(1830,52,30,"Menü");
  bool live=data.value("live").toBool();bool demo=false;
  text(1160,55,22,demo?"WEITERE SENSOREN · DEMO":"FAHRZEUGDATEN",cyan);
  text(64,225,22,data.value("speed").isDouble()?"GESCHWINDIGKEIT · LIVE":"GESCHWINDIGKEIT",muted);text(64,285,155,data.value("speed").isDouble()?QString::number(data.value("speed").toInt()):"—");text(75,510,32,"km/h",muted);
  text(64,650,23,"LEISTUNG",muted);text(64,705,40,demo?"18 kW":"— kW");text(64,810,19,demo?"SENSOREN · DEMO":live?"FAHRZEUGDATEN":"KEINE FAHRZEUGDATEN",muted);
  text(1760,225,22,"BATTERIE",muted);text(1760,295,95,demo?"78 %":"— %");text(1760,520,54,demo?"284 km":"— km");text(1760,610,20,"REICHWEITE",muted);
  text(820,175,21,"FAHRZEUGSTATUS",muted);text(820,850,16,wheelLive?"RADSTELLUNG AUS FAHRZEUGDATEN":"WARTE AUF FAHRZEUGDATEN",muted);
  if(demo){p.setPen(Qt::NoPen);p.setBrush(QColor("#22343d"));p.drawRoundedRect(QRect(1760,450,300,12),6,6);p.setBrush(cyan);p.drawRoundedRect(QRect(1760,450,234,12),6,6);}
  double steering=steeringLive?data.value("steering").toDouble():0;
  text(1760,710,17,steeringLive?"LENKRAD · LIVE":"LENKRAD",muted);
  text(1760,748,32,steeringLive?QString::number(steering,'f',1)+"°":"— °",cyan);
  text(1760,810,13,wheelLive?"RADSTELLUNG · BERECHNET":"RADSTELLUNG UNBEKANNT",muted);
  p.setPen(QColor("#293940"));p.drawLine(64,900,2096,900);
  QStringList titles={"VERBRAUCH","MOTORTEMP.","BORDNETZ","AKKU"};QStringList values=demo?QStringList{"15,8 kWh/100 km","42 °C","13,8 V","26 °C"}:QStringList{"— kWh/100 km","— °C","— V","— °C"};for(int i=0;i<4;i++){text(64+i*515,930,19,titles[i],muted);text(64+i*515,980,29,values[i]);}
  p.end();if(frames==0){QFile::remove(heartbeat+".warming");fprintf(stderr,"GRAPHICS: first frame complete\n");fflush(stderr);}if(++frames==1)QTimer::singleShot(0,this,[this]{grabFramebuffer().save(qEnvironmentVariable("NIRO_RUNTIME")+"/native-dashboard-preview.png");});
 }
 void pressAt(QPointF pos){
  if(!permitted)return;
  if(QRectF(1770,30,320,105).contains(pos)){fprintf(stderr,"INPUT: menu\n");fflush(stderr);dragging=false;requested=false;permitted=false;if(selectView)selectView(false);return;}
  // Camera interaction is limited to parking; this never changes vehicle controls.
  dragging=data.value("parked").toBool()&&QRectF(500,250,1150,610).contains(pos);dragPoint=pos;
 }
 void moveAt(QPointF pos){if(!dragging||!permitted||!data.value("parked").toBool())return;auto delta=pos-dragPoint;dragPoint=pos;azimuth-=delta.x()*.006f;elevation=std::clamp(elevation+float(delta.y())*.004f,.1f,1.3f);update();}
 void mousePressEvent(QMouseEvent *e)override{pressAt(QPointF(e->localPos().x()*2160/width(),e->localPos().y()*1080/height()));e->accept();}
 void mouseMoveEvent(QMouseEvent *e)override{moveAt(QPointF(e->localPos().x()*2160/width(),e->localPos().y()*1080/height()));e->accept();}
 void mouseReleaseEvent(QMouseEvent *e)override{dragging=false;e->accept();}
 bool event(QEvent *event)override{
  if(event->type()==QEvent::TouchBegin||event->type()==QEvent::TouchUpdate||event->type()==QEvent::TouchEnd||event->type()==QEvent::TouchCancel){
   auto t=static_cast<QTouchEvent*>(event);if(!t->touchPoints().isEmpty()){auto pos=t->touchPoints().first().pos();if(event->type()==QEvent::TouchBegin)pressAt(pos);else if(event->type()==QEvent::TouchUpdate)moveAt(pos);}
   if(event->type()==QEvent::TouchEnd||event->type()==QEvent::TouchCancel)dragging=false;event->accept();return true;
  }return QOpenGLWidget::event(event);
 }

};
