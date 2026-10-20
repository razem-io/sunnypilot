#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include <QFile>
#include <QPushButton>
#include <memory>
#include <functional>
#include <vector>
#include "niro_scene_mesh.h"
#include "visual_timing.h"
#include "lane_motion.h"
#include <QPainter>
#include <QTimer>
#include <QElapsedTimer>
#include <array>
#include <cmath>
#include <algorithm>
#include "selfdrive/ui/sunnypilot/ui.h"

// Desktop prototype: fixed world camera, no image processing and no control output.
// leadsV3 entries are time hypotheses, NOT a list of distinct vehicles.
class RoadScene : public QOpenGLWidget {
 struct Curve {std::array<float,31> y{};bool valid=false;float certainty=0; qint64 lastSeen=0;};
 float changeStart=0,changeCenter=0,changeWidth=3.5f,changePeak=0;bool changeRemapped=false;
 float laneTarget=0,laneOffset=0,cameraOffset=0,previousCenter=0,egoYaw=0;bool laneTracking=false;int laneState=0,laneDirection=0;uint64_t lastModelTimestamp=0;
 Curve path,lanes[4],edges[2]; QPushButton *settingsButton=nullptr;
 NiroSceneMesh niro; QMatrix4x4 cameraMatrix; float transition=0,orbit=2.32f,elevation=.32f; QPointF pointer; bool dragging=false; QElapsedTimer clock;
 QTimer timer;QElapsedTimer tick,modelAge;uint64_t modelFrame=0;float speed=0,phase=0,dashPhase=0;
 bool valid=false,carValid=false,assistValid=false,engaged=false,lead=false;float leadX=0,leadY=0,leadAlpha=0; qint64 leadSeen=0;int count=0;
 QRectF sceneRect()const{return rect();}
 QPointF project(float x,float y,float z=0)const {auto v=cameraMatrix*QVector4D(-x,z,-y-laneOffset,1);if(v.w()<.1f)return {-10000,-10000};auto r=sceneRect();return {r.left()+(v.x()/v.w()+1)*r.width()/2,r.top()+(1-v.y()/v.w())*r.height()/2};}
 float pathAt(float x)const {float i=std::clamp(x/4,0.f,29.99f);int j=int(i);return path.y[j]+(i-j)*(path.y[j+1]-path.y[j]);}
 void ageCurve(Curve&c){float age=(clock.elapsed()-c.lastSeen)/1000.f;if(age>5)c.certainty=niro_visual::retainedOpacity(age);if(age>7)c.valid=false;}
 void curve(Curve &out,cereal::XYZTData::Reader input,float certainty,float alpha,bool followHeld=false){
  auto xs=input.getX(),ys=input.getY();bool ok=xs.size()>1&&xs.size()==ys.size()&&(certainty>(out.valid?.20f:.35f)||(followHeld&&out.valid&&certainty>.02f));
  if(!ok){ageCurve(out);return;}
  for(size_t i=0;i<xs.size();i++){if(!std::isfinite(xs[i])||!std::isfinite(ys[i])||(i&&xs[i]<=xs[i-1])){ageCurve(out);return;}}
  std::array<float,31> target{};
  for(int j=0;j<31;j++){float x=j*4.f;size_t i=0;while(i+1<xs.size()&&xs[i+1]<x)i++;
   if(i+1>=xs.size()){target[j]=ys[xs.size()-1];}else{float t=std::clamp((x-xs[i])/std::max(.001f,xs[i+1]-xs[i]),0.f,1.f);target[j]=ys[i]+t*(ys[i+1]-ys[i]);}
   if(!std::isfinite(target[j])||std::abs(target[j])>60){ageCurve(out);return;}
  }
  bool reset=!out.valid;
  for(int j=0;j<31;j++)out.y[j]=reset?target[j]:out.y[j]+alpha*(target[j]-out.y[j]);
  out.valid=true;out.certainty=reset?.05f:std::min(1.f,out.certainty+alpha);out.lastSeen=clock.elapsed();
 }
 float curveAt(const Curve&c,float x)const {if(x<0)return c.y[0]+x*(c.y[1]-c.y[0])/4.f;float t=std::clamp(x/4,0.f,29.999f);int i=int(t);return c.y[i]+(t-i)*(c.y[i+1]-c.y[i]);}
 void ribbon(QPainter&p,const Curve&c,float half,QColor color,bool dashed=false){if(!c.valid)return;p.setPen(Qt::NoPen);p.setBrush(color);
  // Negative x is a short visual tangent extension, not perception behind the car.
  auto segment=[&](float start,float end){start=std::max(-6.f,start);end=std::min(120.f,end);for(float x=start;x<end;x+=1){float next=std::min(x+1,end);QPolygonF poly;poly<<project(x,curveAt(c,x)-half)<<project(next,curveAt(c,next)-half)<<project(next,curveAt(c,next)+half)<<project(x,curveAt(c,x)+half);p.drawPolygon(poly);}};
  if(dashed){for(float start=std::floor((-6+dashPhase)/21)*21-dashPhase;start<120;start+=21)segment(start,start+3);}else segment(-6,120);
 }
 void updateModel(){
  auto&s=*uiStateSP();auto&sm=*s.sm;float dt=std::clamp(tick.restart()/1000.f,.001f,.1f);
  carValid=sm.alive("carState")&&sm.valid("carState")&&sm["carState"].getCarState().getCanValid();
  speed=carValid?std::max(0.f,sm["carState"].getCarState().getVEgo()):0;
  assistValid=sm.alive("selfdriveState")&&sm.valid("selfdriveState");engaged=assistValid&&sm["selfdriveState"].getSelfdriveState().getActive();phase=std::fmod(phase+speed*dt,12.f);dashPhase=std::fmod(dashPhase+speed*dt,21.f);transition+=(float(engaged)-transition)*(1-std::exp(-dt/.65f));laneOffset+=std::clamp((laneTarget-laneOffset)*niro_visual::blend(dt,.3f),-.9f*dt,.9f*dt);cameraOffset+=(laneOffset-cameraOffset)*niro_visual::blend(dt,1.1f);
  auto frame=sm.rcv_frame("modelV2");
  if(frame!=modelFrame){float interval=modelAge.isValid()?modelAge.elapsed()/1000.f:.05f;modelAge.restart();modelFrame=frame;
   auto m=sm["modelV2"].getModelV2();float alpha=1-std::exp(-std::min(interval,1.f)/.65f);
   uint64_t timestamp=sm["modelV2"].getLogMonoTime();
   if(lastModelTimestamp && timestamp<lastModelTimestamp){laneTarget=laneOffset=cameraOffset=0;laneTracking=false;for(auto &c:lanes)c=Curve{};for(auto &c:edges)c=Curve{};path=Curve{};}
   lastModelTimestamp=timestamp;
   laneState=int(m.getMeta().getLaneChangeState());laneDirection=int(m.getMeta().getLaneChangeDirection());
   auto rawLanes=m.getLaneLines();auto rawProbs=m.getLaneLineProbs();bool changing=laneState==2||laneState==3;
   if(changing&&rawLanes.size()>=3&&rawProbs.size()>=3&&rawProbs[1]>.02&&rawProbs[2]>.02&&rawLanes[1].getY().size()&&rawLanes[2].getY().size()){
    float left=rawLanes[1].getY()[0],right=rawLanes[2].getY()[0],center=(left+right)*.5f,width=right-left;
    if(std::isfinite(center)&&width>2&&width<5){if(!laneTracking){changeStart=laneTarget;changeCenter=center;changeWidth=width;changePeak=0;changeRemapped=false;}
     float direction=laneDirection==1?1.f:-1.f;
     float progress=direction*(center-changeCenter);
     changePeak=std::max(changePeak,progress);
     if(!changeRemapped && changePeak-progress>changeWidth*.35f){
      changeRemapped=true;
      if(direction>0){for(int i=3;i>0;--i)lanes[i]=lanes[i-1];lanes[0]=Curve{};}
      else{for(int i=0;i<3;++i)lanes[i]=lanes[i+1];lanes[3]=Curve{};}
     }
     if(changeRemapped)progress+=changeWidth;
     laneTarget=changeStart-direction*std::clamp(progress,0.f,changeWidth*1.25f);
     previousCenter=center;laneTracking=true;}else laneTracking=false;
   }else laneTracking=false;
   curve(path,m.getPosition(),1,alpha);auto ls=m.getLaneLines();auto probs=m.getLaneLineProbs();for(int i=0;i<4;i++){if(i<ls.size()&&i<probs.size())curve(lanes[i],ls[i],probs[i],alpha,changing);else ageCurve(lanes[i]);}
   auto es=m.getRoadEdges();auto stds=m.getRoadEdgeStds();for(int i=0;i<2;i++){if(i<es.size()&&i<stds.size())curve(edges[i],es[i],std::clamp(1.f-stds[i],0.f,1.f),alpha);else ageCurve(edges[i]);}
   bool previousLead=lead;lead=previousLead&&clock.elapsed()-leadSeen<3000;auto leads=m.getLeadsV3();if(leads.size()){auto l=leads[0];auto x=l.getX(),y=l.getY();if(l.getProb()>.6&&std::abs(l.getProbTime())<.01&&x.size()&&y.size()&&std::isfinite(x[0])&&std::isfinite(y[0])&&x[0]>3&&x[0]<120){lead=true;leadSeen=clock.elapsed();bool reset=!previousLead||std::abs(leadX-x[0])>25||std::abs(leadY-(y[0]-pathAt(x[0])))>4;float blend=1-std::exp(-std::min(interval,1.f)/.75f);leadX=reset?x[0]:leadX+blend*(x[0]-leadX);float offset=y[0]-pathAt(x[0]);leadY=reset?offset:leadY+blend*(offset-leadY);}}
  }
  if(!sm.alive("modelV2")||!sm.valid("modelV2")){laneState=0;laneTracking=false;}
  float yawTarget=(laneState==2||laneState==3)&&path.valid?std::clamp(float(std::atan2(pathAt(12)-pathAt(0),12.f)*180/M_PI),-12.f,12.f):0.f;
  egoYaw+=(yawTarget-egoYaw)*niro_visual::blend(dt,.45f);
  float opacityTarget=lead?niro_visual::leadOpacity((clock.elapsed()-leadSeen)/1000.f):0;leadAlpha=opacityTarget>leadAlpha?leadAlpha+(opacityTarget-leadAlpha)*niro_visual::blend(dt,.2f):opacityTarget;
  valid=path.valid&&modelAge.isValid()&&modelAge.elapsed()<500&&sm.alive("modelV2")&&sm.valid("modelV2");update();
 }
 float steeringRatio()const {auto&sm=*uiStateSP()->sm;float ratio=sm["carParams"].getCarParams().getSteerRatio();if(std::isfinite(ratio)&&ratio>1&&ratio<100)return ratio;auto lp=sm["liveParameters"].getLiveParameters();if(sm.alive("liveParameters")&&sm.valid("liveParameters")&&lp.getValid()&&lp.getSteerRatioValid()){ratio=lp.getSteerRatio();if(std::isfinite(ratio)&&ratio>1&&ratio<100)return ratio;}return 0;}
 float smCarWheel()const {float ratio=steeringRatio();if(!carValid||ratio<=1)return 0;auto&sm=*uiStateSP()->sm;return std::clamp(sm["carState"].getCarState().getSteeringAngleDeg()/ratio,-45.f,45.f);}
public:
 std::function<void()> openSettings;
 explicit RoadScene(QWidget*parent=nullptr):QOpenGLWidget(parent){clock.start();tick.start();connect(&timer,&QTimer::timeout,this,[this]{updateModel();});timer.start(33);settingsButton=new QPushButton(this);settingsButton->setStyleSheet("QPushButton{background:transparent;color:#718b98;border:none;font-size:22px;}");settingsButton->setAccessibleName("Einstellungen");settingsButton->setToolTip("Einstellungen öffnen");QPixmap gearIcon(64,64);gearIcon.fill(Qt::transparent);{QPainter g(&gearIcon);g.setRenderHint(QPainter::Antialiasing);g.setPen(QPen(QColor("#718b98"),7));g.drawEllipse(QPointF(32,32),17,17);g.drawEllipse(QPointF(32,32),6,6);for(int i=0;i<8;i++){g.save();g.translate(32,32);g.rotate(i*45);g.drawLine(QPointF(0,18),QPointF(0,26));g.restore();}}settingsButton->setIcon(QIcon(gearIcon));settingsButton->setIconSize(QSize(44,44));connect(settingsButton,&QPushButton::clicked,this,[this]{if(openSettings)openSettings();});}
protected:
 void resizeEvent(QResizeEvent*e)override{QOpenGLWidget::resizeEvent(e);if(settingsButton)settingsButton->setGeometry(2010*width()/2160,35*height()/1080,85*width()/2160,85*height()/1080);}
 void initializeGL()override{niro.initialize();}
 void mousePressEvent(QMouseEvent*e)override{pointer=e->localPos();dragging=!engaged&&QRectF(width()*.23,height()*.16,width()*.54,height()*.68).contains(pointer);}
 void mouseMoveEvent(QMouseEvent*e)override{if(dragging&&!engaged){orbit+=(e->localPos().x()-pointer.x())*.005f;elevation=std::clamp(float(elevation+(e->localPos().y()-pointer.y())*.004f),.1f,1.48f);pointer=e->localPos();update();}}
 void mouseReleaseEvent(QMouseEvent*)override{dragging=false;}
 void paintGL()override{
  float angle=orbit*(1-transition);float elev=elevation*(1-transition)+.38f*transition;float distance=7.6f+2.4f*transition;QVector3D eye(distance*std::cos(angle)*std::cos(elev),.7f+distance*std::sin(elev),distance*std::sin(angle)*std::cos(elev));QVector3D target(-6*transition,.6f,0);eye.setZ(eye.z()-cameraOffset);target.setZ(target.z()-cameraOffset);QMatrix4x4 view,projection;view.lookAt(eye,target,{0,1,0});projection.perspective(34+14*transition,sceneRect().width()/sceneRect().height(),.1,220);cameraMatrix=projection*view;

  QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QLinearGradient bg(0,0,0,height());bg.setColorAt(0,QColor("#17232e"));bg.setColorAt(.3,QColor("#243440"));bg.setColorAt(1,QColor("#0a1018"));p.fillRect(rect(),bg);
  p.save();p.setClipRect(sceneRect());
  p.setPen(QPen(QColor("#31505e"),1.5));p.setBrush(Qt::NoBrush);QPolygonF ring;for(int i=0;i<=96;i++){float a=i*2*M_PI/96;ring<<project(2.7*std::cos(a),1.75*std::sin(a),.005);}p.drawPolyline(ring);
  if(valid&&transition>.01){
   p.setOpacity(transition);
   if(edges[0].valid&&edges[1].valid){QPolygonF asphalt;for(int x=-6;x<=120;x+=2)asphalt<<project(x,curveAt(edges[0],x));for(int x=120;x>=-6;x-=2)asphalt<<project(x,curveAt(edges[1],x));p.setPen(Qt::NoPen);p.setBrush(QColor("#273440"));p.setOpacity(transition*std::min(edges[0].certainty,edges[1].certainty));p.drawPolygon(asphalt);p.setOpacity(transition);}
   for(auto&e:edges)ribbon(p,e,.055,QColor(114,140,153,int(150*e.certainty)));
   ribbon(p,path,.82,engaged?QColor(50,191,219,90):QColor(120,159,185,55));
   for(auto&l:lanes)ribbon(p,l,.045,QColor(220,230,235,int(220*l.certainty)),true);
   if(path.valid){p.setPen(QPen(QColor(85,207,226,100),2));for(float x=phase+5;x<110;x+=12){int i=std::min(29,int(x/4));float y=path.y[i];p.drawLine(project(x,y-.06,.015),project(x+1.8,y+.06,.015));}}
   if(lead){p.setOpacity(transition*leadAlpha);auto q=project(leadX,pathAt(leadX)+leadY,2.1);p.setFont(QFont("Inter",20));p.setPen(QColor("#b9d7df"));p.drawText(QRectF(q.x()-130,q.y()-45,260,40),Qt::AlignCenter,QString::number(leadX,'f',0)+" m");}
   p.setOpacity(1);
  }
  p.restore();p.end();
  auto vp=sceneRect();float pixel=devicePixelRatioF();
  float wheel=carValid?smCarWheel():0;
  niro.draw(cameraMatrix,eye,wheel,width()*devicePixelRatioF(),height()*devicePixelRatioF(),QVector3D(0,0,-laneOffset),-egoYaw,true,1,QRect(vp.left()*pixel,(height()-vp.bottom())*pixel,vp.width()*pixel,vp.height()*pixel));
  if(valid&&lead&&transition>.01){float y=pathAt(leadX)+leadY;float heading=std::atan2(pathAt(leadX+2)-pathAt(leadX-2),4.f)*180/M_PI;niro.draw(cameraMatrix,eye,0,width()*devicePixelRatioF(),height()*devicePixelRatioF(),QVector3D(-leadX,0,-y-laneOffset),-heading,false,.62f*transition*leadAlpha,QRect(vp.left()*pixel,(height()-vp.bottom())*pixel,vp.width()*pixel,vp.height()*pixel),true);}
  p.begin(this);p.setRenderHint(QPainter::Antialiasing);
  p.save();p.setClipRect(sceneRect());
  auto car=(*uiStateSP()->sm)["carState"].getCarState();bool blink=clock.elapsed()%800<400;
  // Project the same four lamp anchors as the manual HTML scene. Hide far-side lamps.
  if(carValid){for(int side:{-1,1})for(int end:{-1,1}){bool visible=(end==1?eye.x()>0:eye.x()<0)||side*eye.z()>0;
   bool on=blink&&(side>0?car.getLeftBlinker():car.getRightBlinker());if(!on||!visible)continue;
   auto at=project(end<0?1.75f:-1.85f,side*.74f,end<0?.74f:1.05f);p.setPen(QPen(QColor("#ffb234"),7));p.drawLine(at-QPointF(6,0),at+QPointF(6,0));}
  }
  if(engaged&&!valid){p.setPen(QColor("#f2c37b"));p.setFont(QFont("Inter",22));p.drawText(sceneRect(),Qt::AlignTop|Qt::AlignHCenter,"Keine aktuellen Modelldaten");}
  p.restore();p.scale(width()/2160.,height()/1080.);
  auto text=[&](int x,int y,int size,QString value,QColor color=QColor("#e4edf1")){p.setPen(color);p.setFont(QFont("Inter",size));p.drawText(QRect(x,y,800,size*2),Qt::AlignLeft|Qt::AlignTop,value);};
  QColor muted("#92a9b6"),cyan("#6cdde8");
  text(64,42,44,"KIA");text(185,55,27,"NIRO EV",muted);
  text(850,55,24,!assistValid?"STATUS UNBEKANNT":engaged?"ASSISTENZ AKTIV":"MANUELLE FAHRT",cyan);
  QString gear="—";if(carValid){switch(car.getGearShifter()){case cereal::CarState::GearShifter::PARK:gear="P";break;case cereal::CarState::GearShifter::DRIVE:gear="D";break;case cereal::CarState::GearShifter::REVERSE:gear="R";break;case cereal::CarState::GearShifter::NEUTRAL:gear="N";break;default:break;}}
  const QStringList gearLabels={"P","R","N","D"};
  p.setFont(QFont("Inter",38,QFont::Medium));
  for(int i=0;i<gearLabels.size();i++){p.setPen(carValid&&gear==gearLabels[i]?QColor("#dce9ee"):QColor("#526572"));p.drawText(QRectF(1745+i*60,35,60,85),Qt::AlignCenter,gearLabels[i]);}
  if(!carValid){p.setPen(QColor("#92a9b6"));p.setFont(QFont("Inter",14));p.drawText(QRectF(1745,122,240,25),Qt::AlignCenter,"Gang unbekannt");}
  text(64,197,21,"GESCHWINDIGKEIT",muted);text(64,235,175,carValid?QString::number(qRound(speed*3.6)):"—");text(64,465,30,"km/h",muted);
  text(64,805,21,carValid?(car.getBrakePressed()?"BREMSE BETÄTIGT":car.getGasPressed()?"GASPEDAL BETÄTIGT":""):"",cyan);
  text(1731,197,21,"FAHRZEUGSTATUS",muted);
  QStringList labels={"Blinker","Türen","Fahrergurt","Totwinkel","Parkbremse"};
  QStringList values={car.getLeftBlinker()?(car.getRightBlinker()?"Beide":"Links"):car.getRightBlinker()?"Rechts":"Aus",car.getDoorOpen()?"Tür offen":"Geschlossen",car.getSeatbeltUnlatched()?"Nicht angelegt":"Angelegt",car.getLeftBlindspot()?(car.getRightBlindspot()?"Beide Seiten":"Links"):car.getRightBlindspot()?"Rechts":"Kein Signal",car.getParkingBrake()?"Aktiv":"Nicht aktiv"};
  for(int i=0;i<5;i++){int y=260+i*110;text(1731,y,18,labels[i],muted);text(1731,y+34,26,carValid?values[i]:"—");p.setPen(QColor("#273842"));p.drawLine(1731,y+90,2095,y+90);}
  if(laneState&&engaged){QString direction=laneDirection==1?"←":laneDirection==2?"→":"";text(850,110,22,laneState==1?"SPURWECHSEL BEREIT "+direction:"SPURWECHSEL "+direction,cyan);}
  text(700,145,22,carValid?(car.getDoorOpen()?"Mindestens eine Tür offen":gear=="R"?"RÜCKWÄRTS":""):"",QColor("#ffb45f"));
  if(carValid&&blink){text(500,150,36,car.getLeftBlinker()?"◀":"",QColor("#ffb45f"));text(1620,150,36,car.getRightBlinker()?"▶":"",QColor("#ffb45f"));}
  p.setPen(QColor("#2b3d49"));p.drawLine(64,950,2095,950);
  auto&sm=*uiStateSP()->sm;auto device=sm["deviceState"].getDeviceState();bool deviceFresh=sm.alive("deviceState")&&sm.valid("deviceState");
  QStringList networks={"Offline","WLAN","2G","3G","4G","5G","Ethernet"};int net=int(device.getNetworkType());QString network=deviceFresh&&net>=0&&net<networks.size()?networks[net]:"Unbekannt";
  QString thermal=deviceFresh?(device.getThermalStatus()==cereal::DeviceState::ThermalStatus::GREEN?"Gut":device.getThermalStatus()==cereal::DeviceState::ThermalStatus::YELLOW?"Warm":"Heiß"):"Unbekannt";
  bool pandaFresh=sm.alive("pandaStates")&&sm.valid("pandaStates");bool panda=false;for(auto item:sm["pandaStates"].getPandaStates())panda|=item.getPandaType()!=cereal::PandaState::PandaType::UNKNOWN;
  // Recorded monotonic timestamps must be compared with the recorded message clock.
  auto messageTime=sm["deviceState"].getLogMonoTime(),ping=device.getLastAthenaPingTime();QString connect="Unbekannt";if(deviceFresh){connect=ping==0?"Offline":messageTime>=ping?(messageTime-ping<80000000000ULL?"Online":"Offline"):"Unbekannt";}
  // Fixed 70-unit slots: network, temperature, Panda, Connect. Never hide a slot.
  QColor good("#83b8ba"),bad("#e5aa65"),inactive("#526572");
  p.setFont(QFont("Inter",24));p.setPen(deviceFresh&&net>0?good:inactive);
  p.drawText(QRectF(60,970,56,36),Qt::AlignCenter,deviceFresh?(net>0?network:"—"):"?");
  QColor tempColor=!deviceFresh?bad:device.getThermalStatus()==cereal::DeviceState::ThermalStatus::GREEN?inactive:bad;
  p.setBrush(Qt::NoBrush);p.setPen(QPen(tempColor,2));p.drawRoundedRect(QRectF(155,975,6,19),3,3);p.drawEllipse(QPointF(158,997),5,5);p.drawLine(158,983,158,997);
  p.save();p.translate(228,988);p.scale(.55,.55);p.translate(-229,-992);
  p.setBrush(Qt::NoBrush);p.setPen(QPen(pandaFresh&&panda?good:inactive,3));
  p.drawRoundedRect(QRectF(210,977,35,28),5,5);p.drawLine(220,970,220,977);p.drawLine(235,970,235,977);p.drawLine(227,1005,227,1015);p.drawLine(227,1015,250,1015);
  if(!pandaFresh||!panda)p.drawLine(208,1010,249,972);
  p.restore();p.save();p.translate(298,988);p.scale(.55,.55);p.translate(-315,-992);p.setBrush(Qt::NoBrush);
  p.setPen(QPen(connect=="Online"?good:inactive,2));p.drawEllipse(QPointF(315,992),20,20);p.drawEllipse(QRectF(307,972,16,40));p.drawLine(295,992,335,992);
  if(connect!="Online")p.drawLine(293,1014,337,970);
  p.restore();
  setToolTip(QString("Netzwerk: %1 · Temperatur: %2 · Panda: %3 · Connect: %4 (Replay)").arg(network,thermal,pandaFresh?(panda?"Verbunden":"Nicht verbunden"):"Unbekannt",connect));
  text(64,1020,16,"STATUS AUS AUFZEICHNUNG · REPLAY",muted);text(850,1020,16,"Gestrichelt: Spur (schematisch) · Durchgezogen: Fahrbahnrand",muted);
 }
};
