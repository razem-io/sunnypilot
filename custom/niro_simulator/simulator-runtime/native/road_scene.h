#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include <QTouchEvent>
#include "selfdrive/ui/qt/onroad/model.h"
#include <QFile>
#include <QPushButton>
#include <memory>
#include <functional>
#include <vector>
#include "niro_scene_mesh.h"
#include "visual_timing.h"
#include "lane_motion.h"
#include "suspension.h"
#include "road_camera.h"
#include "solar_light.h"
#include "ambient_probe.h"
#include "reflection_history.h"
#include "cabin_light.h"
#include <QDateTime>
#include "selfdrive/ui/qt/widgets/cameraview.h"
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QElapsedTimer>
#include <array>
#include <cmath>
#include <algorithm>
#include "selfdrive/ui/sunnypilot/ui.h"

// Desktop prototype: fixed world camera, no image processing and no control output.
// leadsV3 entries are time hypotheses, NOT a list of distinct vehicles.
class RoadScene : public CameraWidget {
 struct Curve {std::array<float,31> y{};bool valid=false;float certainty=0; qint64 lastSeen=0;};
 float changeStart=0,changeCenter=0,changeWidth=3.5f,changePeak=0;bool changeRemapped=false;
 float laneTarget=0,laneOffset=0,cameraOffset=0,previousCenter=0,egoYaw=0;bool laneTracking=false;int laneState=0,laneDirection=0;uint64_t lastModelTimestamp=0;
 bool cameraMode=false;float cameraBlend=0;QElapsedTimer cameraAge;QImage cameraVeil;QPushButton *cameraButton=nullptr,*demoButton=nullptr,*motionButton=nullptr;bool demoManual=false,cameraSuspension=false;float cameraSuspensionBlend=0,cameraRoll=0,cameraPitch=0,cameraHeave=0;
 Curve path,lanes[4],edges[2]; QPushButton *settingsButton=nullptr;
 ModelRenderer cameraModel;std::unique_ptr<QOpenGLTexture> cameraOverlay;uint64_t overlayFrame=0;
 QElapsedTimer touchAge;int touchCount=0;bool touchMoved=false;QMap<int,QPointF> touchOrigins;
 CabinLight cabin;float cabinLevel=0;
 ReflectionHistory reflectionHistory;AmbientProbe ambientProbe;GLuint ambientTexture=0;float ambientBlend=0;
 RoadCamera roadCamera;NiroSceneMesh niro; QMatrix4x4 cameraMatrix; float transition=0,orbit=2.32f,elevation=.32f; QPointF pointer; bool dragging=false; QElapsedTimer clock;
 QTimer timer;QElapsedTimer tick,modelAge;uint64_t modelFrame=0;float speed=0,phase=0,dashPhase=0;
 QPushButton *lightButton=nullptr;int lightMode=0;bool solarValid=false;float lightMix=0,daylight=1;QVector3D sunDirection{-3,5,4},sunTint{1,1,1};
 SubMaster lightSensors{std::vector<const char*>{"gpsLocationExternal","gpsLocation"}};
 SubMaster motionSensors{std::vector<const char*>{"accelerometer"}};
 QVector3D accelerationBaseline;bool motionInitialized=false;float bodyRoll=0,bodyPitch=0,bodyHeave=0;NiroSpring rollSpring,pitchSpring,heaveSpring;std::array<float,4> wheelSpin{};
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
  cabinLevel+=(cabin.value()-cabinLevel)*niro_visual::blend(dt,.8f);
  lightSensors.update(0);
  niro_solar::Sun sun{45,135};double heading=0;solarValid=false;
  for(auto service:{"gpsLocationExternal","gpsLocation"}){
   if(!lightSensors.alive(service)||!lightSensors.valid(service))continue;
   auto gps=std::string(service)=="gpsLocationExternal"?lightSensors[service].getGpsLocationExternal():lightSensors[service].getGpsLocation();
   auto utc=QDateTime::fromMSecsSinceEpoch(gps.getUnixTimestampMillis(),Qt::UTC);
   if(!(gps.getHasFix()||(gps.getFlags()&1))||!utc.isValid()||utc.date().year()<2020||!std::isfinite(gps.getLatitude())||!std::isfinite(gps.getLongitude())||std::abs(gps.getLatitude())>90||std::abs(gps.getLongitude())>180||!std::isfinite(gps.getBearingDeg()))continue;
   // GPS course is unreliable while stationary: retain the last driving bearing.
   static double lastHeading=0;if(gps.getSpeed()>1.5)lastHeading=gps.getBearingDeg();heading=lastHeading;
   auto t=utc.time();sun=niro_solar::position(utc.date().dayOfYear(),utc.date().daysInYear(),t.hour()+t.minute()/60.+t.second()/3600.,gps.getLatitude(),gps.getLongitude());solarValid=true;break;
  }
  if(lightMode){sun=lightMode==1?niro_solar::Sun{55,135}:lightMode==2?niro_solar::Sun{3,250}:niro_solar::Sun{-25,0};heading=0;}
  float lx,ly,lz;niro_solar::direction(sun,heading,lx,ly,lz);
  float lightAlpha=niro_visual::blend(dt,1.2f);
  sunDirection+=(QVector3D(lx,ly,lz)-sunDirection)*lightAlpha;
  daylight+=(float(niro_solar::daylight(sun.elevation))-daylight)*lightAlpha;
  float warmth=std::clamp(float((15-sun.elevation)/15),0.f,1.f);
  sunTint+=(QVector3D(1,1-.32f*warmth,1-.57f*warmth)-sunTint)*lightAlpha;
  lightMix+=(float(lightMode||solarValid)-lightMix)*lightAlpha;
  lightButton->setText(lightMode==1?"Licht: Tag (Demo)":lightMode==2?"Licht: Abend (Demo)":lightMode==3?"Licht: Nacht (Demo)":solarValid?"Licht: GPS / Sonne":"Licht: Studio · kein GPS");
  motionSensors.update(0);
  float targetRoll=0,targetPitch=0,targetHeave=0;
  if(motionSensors.alive("accelerometer")&&motionSensors.valid("accelerometer")){
   auto a=motionSensors["accelerometer"].getAccelerometer();
   if(a.isAcceleration()&&a.getAcceleration().getV().size()==3){
    auto v=a.getAcceleration().getV();QVector3D measured(-v[2],-v[1],-v[0]);
    if(std::isfinite(measured.lengthSquared())&&measured.length()<30){
     if(!motionInitialized){accelerationBaseline=measured;motionInitialized=true;}
     accelerationBaseline+=(measured-accelerationBaseline)*niro_visual::blend(dt,3.f);
     auto dynamic=measured-accelerationBaseline;
     targetRoll=std::clamp(-dynamic.y()*2.6f,-4.5f,4.5f);
     targetPitch=std::clamp(dynamic.x()*2.2f,-3.5f,3.5f);
     targetHeave=std::clamp(dynamic.z()*.025f,-.065f,.065f);
    }
   }
  }else motionInitialized=false;
  cameraSuspensionBlend+=(float(cameraSuspension)-cameraSuspensionBlend)*niro_visual::blend(dt,.4f);
  bodyRoll=rollSpring.step(targetRoll,dt,4.5f);
  bodyPitch=pitchSpring.step(targetPitch,dt,3.5f);
  bodyHeave=heaveSpring.step(targetHeave,dt,.065f);
  // A softly isolated camera mount: retain body coupling without full jolts.
  const float cameraResponse=niro_visual::blend(dt,.22f);
  cameraRoll+=(bodyRoll*.60f-cameraRoll)*cameraResponse;
  cameraPitch+=(bodyPitch*.60f-cameraPitch)*cameraResponse;
  cameraHeave+=(bodyHeave*.65f-cameraHeave)*cameraResponse;
  bool freshCamera=cameraAge.isValid()&&cameraAge.elapsed()<500;
  ambientBlend+=(float(freshCamera)-ambientBlend)*niro_visual::blend(dt,.5f);
  cameraBlend+=(float(cameraMode&&freshCamera)-cameraBlend)*niro_visual::blend(dt,.55f);
  carValid=sm.alive("carState")&&sm.valid("carState")&&sm["carState"].getCarState().getCanValid();
  speed=carValid?std::max(0.f,sm["carState"].getCarState().getVEgo()):0;
  assistValid=sm.alive("selfdriveState")&&sm.valid("selfdriveState");engaged=assistValid&&sm["selfdriveState"].getSelfdriveState().getActive();phase=std::fmod(phase+speed*dt,12.f);dashPhase=std::fmod(dashPhase+speed*dt,21.f);transition+=(float(engaged&&!demoManual)-transition)*(1-std::exp(-dt/.65f));laneOffset=cameraOffset=0; // Steering drives pose; no extra lane-change orbit.
  if(carValid){
   auto car=sm["carState"].getCarState();auto wheels=car.getWheelSpeeds();
   std::array<float,4> speeds{wheels.getFl(),wheels.getFr(),wheels.getRl(),wheels.getRr()};
   bool available=std::any_of(speeds.begin(),speeds.end(),[](float v){return std::isfinite(v)&&v>.01f;});
   float direction=car.getGearShifter()==cereal::CarState::GearShifter::REVERSE?-1.f:1.f;
   for(int i=0;i<4;i++){
    float v=available?speeds[i]:speed;
    if(std::isfinite(v)&&v>=0&&v<100)wheelSpin[i]=std::fmod(wheelSpin[i]+direction*v/.3285f*180.f/M_PI*dt,360.f);
   }
  }
  auto frame=sm.rcv_frame("modelV2");
  if(frame!=modelFrame){float interval=modelAge.isValid()?modelAge.elapsed()/1000.f:.05f;modelAge.restart();modelFrame=frame;
   auto m=sm["modelV2"].getModelV2();float alpha=1-std::exp(-std::min(interval,1.f)/.65f);
   uint64_t timestamp=sm["modelV2"].getLogMonoTime();
   if(lastModelTimestamp && timestamp<lastModelTimestamp){laneTarget=laneOffset=cameraOffset=0;laneTracking=false;motionInitialized=false;bodyRoll=bodyPitch=bodyHeave=0;cameraRoll=cameraPitch=cameraHeave=0;rollSpring.reset();pitchSpring.reset();heaveSpring.reset();for(auto &c:lanes)c=Curve{};for(auto &c:edges)c=Curve{};path=Curve{};}
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
  // The predicted path bends back while settling into a lane; it is not
  // the current vehicle heading. Use measured steering for the visual turn.
  // Positive steering is left; egoYaw is right-positive (draw uses -egoYaw).
  float yawTarget=carValid&&speed>1.f?std::clamp(-smCarWheel()*3.5f,-10.f,10.f):0.f;
  egoYaw+=(yawTarget-egoYaw)*niro_visual::blend(dt,.45f);
  float opacityTarget=lead?niro_visual::leadOpacity((clock.elapsed()-leadSeen)/1000.f):0;leadAlpha=opacityTarget>leadAlpha?leadAlpha+(opacityTarget-leadAlpha)*niro_visual::blend(dt,.2f):opacityTarget;
  valid=path.valid&&modelAge.isValid()&&modelAge.elapsed()<500&&sm.alive("modelV2")&&sm.valid("modelV2");update();
 }
 float steeringRatio()const {auto&sm=*uiStateSP()->sm;float ratio=sm["carParams"].getCarParams().getSteerRatio();if(std::isfinite(ratio)&&ratio>1&&ratio<100)return ratio;auto lp=sm["liveParameters"].getLiveParameters();if(sm.alive("liveParameters")&&sm.valid("liveParameters")&&lp.getValid()&&lp.getSteerRatioValid()){ratio=lp.getSteerRatio();if(std::isfinite(ratio)&&ratio>1&&ratio<100)return ratio;}return 0;}
 float smCarWheel()const {float ratio=steeringRatio();if(!carValid||ratio<=1)return 0;auto&sm=*uiStateSP()->sm;return std::clamp(sm["carState"].getCarState().getSteeringAngleDeg()/ratio,-45.f,45.f);}
public:
 void toggleCamera(){cameraButton->toggle();}
 void setSettingsAction(std::function<void()> action){connect(settingsButton,&QPushButton::clicked,this,std::move(action));}
 explicit RoadScene(QWidget*parent=nullptr):CameraWidget("camerad",VISION_STREAM_ROAD,parent){
 cabin.start();
 connect(this,&CameraWidget::vipcThreadFrameReceived,this,[this]{cameraAge.restart();});
 cameraButton=new QPushButton("Kamera",this);cameraButton->setCheckable(true);cameraButton->setChecked(false);cameraButton->setAccessibleName("Kamerabild einblenden");cameraButton->setToolTip("Zwischen Kamerabild und 3D-Straße wechseln");cameraButton->setStyleSheet("QPushButton{background:transparent;border:none;color:#718b98;font-size:21px;}QPushButton:checked{color:#6cdde8;}");connect(cameraButton,&QPushButton::toggled,this,[this](bool enabled){cameraMode=enabled;});
 demoButton=new QPushButton("Demo: frei drehen",this);demoButton->setCheckable(true);demoButton->setAccessibleName("Demoansicht frei drehen");demoButton->setStyleSheet(cameraButton->styleSheet());
 connect(demoButton,&QPushButton::toggled,this,[this](bool enabled){demoManual=enabled;demoButton->setText(enabled?"Zur Fahransicht":"Demo: frei drehen");if(enabled)cameraButton->setChecked(false);});
 lightButton=new QPushButton("Licht: Auto",this);lightButton->setAccessibleName("Beleuchtung wechseln: Auto, Tag, Abend, Nacht");lightButton->setStyleSheet(cameraButton->styleSheet());connect(lightButton,&QPushButton::clicked,this,[this]{lightMode=(lightMode+1)%4;});
 motionButton=new QPushButton("Kamera federt",this);motionButton->setCheckable(true);motionButton->setAccessibleName("Kamera mit Federung bewegen");motionButton->setStyleSheet(cameraButton->styleSheet());motionButton->setToolTip("Test: Kamerabild an Karosseriebewegung koppeln");connect(motionButton,&QPushButton::toggled,this,[this](bool on){cameraSuspension=on;});
 setAttribute(Qt::WA_AcceptTouchEvents);cameraButton->setAttribute(Qt::WA_AcceptTouchEvents);cameraButton->installEventFilter(this);
 clock.start();tick.start();connect(&timer,&QTimer::timeout,this,[this]{updateModel();});timer.start(33);settingsButton=new QPushButton(this);settingsButton->setStyleSheet("QPushButton{background:transparent;color:#718b98;border:none;font-size:22px;}");settingsButton->setAccessibleName("Einstellungen");settingsButton->setToolTip("Einstellungen öffnen");QPixmap gearIcon(64,64);gearIcon.fill(Qt::transparent);{QPainter g(&gearIcon);g.setRenderHint(QPainter::Antialiasing);g.setPen(QPen(QColor("#718b98"),7));g.drawEllipse(QPointF(32,32),17,17);g.drawEllipse(QPointF(32,32),6,6);for(int i=0;i<8;i++){g.save();g.translate(32,32);g.rotate(i*45);g.drawLine(QPointF(0,18),QPointF(0,26));g.restore();}}settingsButton->setIcon(QIcon(gearIcon));settingsButton->setIconSize(QSize(44,44));for(auto *b:{demoButton,settingsButton,motionButton,lightButton}){b->setAttribute(Qt::WA_AcceptTouchEvents);b->installEventFilter(this);}}
protected:
 bool handleTouch(QEvent *event,QPushButton *button=nullptr){
  if(event->type()!=QEvent::TouchBegin&&event->type()!=QEvent::TouchUpdate&&event->type()!=QEvent::TouchEnd&&event->type()!=QEvent::TouchCancel)return false;
  auto *touch=static_cast<QTouchEvent*>(event);
  if(event->type()==QEvent::TouchBegin){touchAge.restart();touchCount=0;touchMoved=false;touchOrigins.clear();dragging=false;}
  int down=0;
  for(const auto &point:touch->touchPoints()){
   if(point.state()!=Qt::TouchPointReleased)down++;
   if(!touchOrigins.contains(point.id()))touchOrigins[point.id()]=point.screenPos();
   if(QLineF(touchOrigins[point.id()],point.screenPos()).length()>35)touchMoved=true;
  }
  touchCount=std::max(touchCount,down);
  if(!button&&(!engaged||demoManual)&&touchCount==1&&event->type()==QEvent::TouchUpdate&&touch->touchPoints().size()==1){auto point=touch->touchPoints().first();if(QRectF(width()*.23,height()*.16,width()*.54,height()*.68).contains(point.startPos())){orbit+=(point.pos().x()-point.lastPos().x())*.005f;elevation=std::clamp(float(elevation+(point.pos().y()-point.lastPos().y())*.004f),.1f,1.48f);}}

  if(event->type()==QEvent::TouchEnd){if(touchCount==3&&!touchMoved&&touchAge.isValid()&&touchAge.elapsed()<650)cameraButton->toggle();else if(button&&touchCount==1&&!touchMoved&&touchAge.elapsed()<650)button->click();touchCount=0;}
  if(event->type()==QEvent::TouchCancel)touchCount=0;
  event->accept();return true;
 }
 bool event(QEvent *e)override{if(handleTouch(e))return true;return CameraWidget::event(e);}
 bool eventFilter(QObject *object,QEvent *e)override{if(handleTouch(e,qobject_cast<QPushButton*>(object)))return true;return CameraWidget::eventFilter(object,e);}
 mat4 calcFrameMatrix()override{
  // Cover the scene without letterbox edges; the vignette supplies its silhouette.
  float aspect=float(width())/std::max(1,height());float cameraAspect=float(stream_width)/std::max(1,stream_height);
  float x=std::max(cameraAspect/aspect,1.f),y=std::max(aspect/cameraAspect,1.f);
  return mat4{{x,0,0,0, 0,y,0,0, 0,0,1,0, 0,0,0,1}};
 }
 void resizeEvent(QResizeEvent*e)override{QOpenGLWidget::resizeEvent(e);if(lightButton)lightButton->setGeometry(1320*width()/2160,975*height()/1080,420*width()/2160,75*height()/1080);if(motionButton)motionButton->setGeometry(1760*width()/2160,975*height()/1080,330*width()/2160,75*height()/1080);if(demoButton)demoButton->setGeometry(1240*width()/2160,35*height()/1080,270*width()/2160,85*height()/1080);if(cameraButton)cameraButton->setGeometry(1510*width()/2160,35*height()/1080,180*width()/2160,85*height()/1080);cameraVeil=QImage();if(settingsButton)settingsButton->setGeometry(2010*width()/2160,35*height()/1080,85*width()/2160,85*height()/1080);}
 void initializeGL()override{CameraWidget::initializeGL();niro.initialize();roadCamera.initialize();ambientProbe.initialize();reflectionHistory.initialize();}
 void mousePressEvent(QMouseEvent*e)override{pointer=e->localPos();dragging=(!engaged||demoManual)&&QRectF(width()*.23,height()*.16,width()*.54,height()*.68).contains(pointer);}
 void mouseMoveEvent(QMouseEvent*e)override{if(dragging&&(!engaged||demoManual)){orbit+=(e->localPos().x()-pointer.x())*.005f;elevation=std::clamp(float(elevation+(e->localPos().y()-pointer.y())*.004f),.1f,1.48f);pointer=e->localPos();update();}}
 void mouseReleaseEvent(QMouseEvent*)override{dragging=false;}
 void paintGL()override{
  float angle=orbit*(1-transition);float elev=elevation*(1-transition)+.38f*transition;float distance=7.6f+2.4f*transition;QVector3D eye(distance*std::cos(angle)*std::cos(elev),.7f+distance*std::sin(elev),distance*std::sin(angle)*std::cos(elev));QVector3D target(-6*transition,.6f,0);eye.setX(eye.x()+2.2f*cameraBlend);target.setX(target.x()+2.2f*cameraBlend);// A low chase viewpoint limits the parallax introduced by the ground projection.
  float hybrid=cameraBlend*transition;
  eye=eye*(1-hybrid)+QVector3D(11.5f,2.8f,0)*hybrid;
  target=target*(1-hybrid)+QVector3D(-12.f,.8f,0)*hybrid;
  eye.setZ(eye.z()-cameraOffset);target.setZ(target.z()-cameraOffset);QMatrix4x4 view,projection;view.lookAt(eye,target,{0,1,0});projection.perspective(34+14*transition-6*hybrid,sceneRect().width()/sceneRect().height(),.1,220);projection(1,2)=-.08f*hybrid;cameraMatrix=projection*view;

  if(cameraAge.isValid()&&cameraAge.elapsed()<500){glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);CameraWidget::paintGL();}
  ambientTexture=ambientProbe.update(textures[0],textures[1],prev_frame_id,cameraAge.isValid()&&cameraAge.elapsed()<500&&stream_width>0&&stream_height>0);
  reflectionHistory.update(textures[0],textures[1],prev_frame_id,ambientTexture!=0,speed);
  QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QLinearGradient bg(0,0,0,height());bg.setColorAt(0,QColor("#17232e"));bg.setColorAt(.3,QColor("#243440"));bg.setColorAt(1,QColor("#0a1018"));p.fillRect(rect(),bg);
  if(cameraBlend>.001f&&stream_width>0&&stream_height>0){
   p.end();auto &state=*uiStateSP();auto calibration=state.sm->operator[]("liveCalibration").getLiveCalibration();
   float cameraHeight=1.22f;auto heights=calibration.getHeight();if(heights.size()&&std::isfinite(heights[0])&&heights[0]>.5f&&heights[0]<3)cameraHeight=heights[0];
   Eigen::Matrix3f intrinsic=FCAM_INTRINSIC_MATRIX;
   // The original demo uses the 1928x1208 AR0231 road camera.
   intrinsic.row(0)*=float(stream_width)/1928;intrinsic.row(1)*=float(stream_height)/1208;
   Eigen::Matrix3f transform=(intrinsic*state.scene.view_from_calib).inverse();QMatrix3x3 cameraTransform;
   for(int r=0;r<3;r++)for(int c=0;c<3;c++)cameraTransform(r,c)=transform(r,c);
   bool overlayValid=valid&&calibration.getHeight().size()>0&&state.sm->operator[]("modelV2").getModelV2().getPosition().getX().size()>1;
   if(overlayValid&&(!cameraOverlay||overlayFrame!=modelFrame)){
    QImage overlay(stream_width,stream_height,QImage::Format_RGBA8888);overlay.fill(Qt::transparent);
    QPainter annotation(&overlay);annotation.setRenderHint(QPainter::Antialiasing);annotation.setPen(Qt::NoPen);
    cameraModel.setTransform(intrinsic*state.scene.view_from_calib);cameraModel.draw(annotation,overlay.rect());annotation.end();
    if(!cameraOverlay){cameraOverlay=std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);cameraOverlay->setSize(stream_width,stream_height);cameraOverlay->setFormat(QOpenGLTexture::RGBA8_UNorm);cameraOverlay->allocateStorage(QOpenGLTexture::RGBA,QOpenGLTexture::UInt8);cameraOverlay->setMinMagFilters(QOpenGLTexture::Linear,QOpenGLTexture::Linear);cameraOverlay->setWrapMode(QOpenGLTexture::ClampToEdge);}
    cameraOverlay->setData(QOpenGLTexture::RGBA,QOpenGLTexture::UInt8,overlay.constBits());overlayFrame=modelFrame;
   }
   // The video is an ego-mounted backdrop, not scenery at 85 m. Match its
   // screen displacement to the near vehicle instead of introducing depth parallax.
   QVector3D centeredEye=eye,centeredTarget=target;
   centeredEye.setZ(centeredEye.z()+cameraOffset-laneOffset);
   centeredTarget.setZ(centeredTarget.z()+cameraOffset-laneOffset);
   QMatrix4x4 centeredView;centeredView.lookAt(centeredEye,centeredTarget,{0,1,0});
   QMatrix4x4 centeredCamera=projection*centeredView;
   QVector4D anchor(2.2f*cameraBlend,.7f,-laneOffset,1);
   QVector4D actual=cameraMatrix*anchor,centered=centeredCamera*anchor;
   QMatrix4x4 follow;
   if(actual.w()>.1f&&centered.w()>.1f){
    // Compensate for image enlargement so both move equal pixels.
    follow.translate((actual.x()/actual.w()-centered.x()/centered.w())/RoadCamera::imageScale,
                     (actual.y()/actual.w()-centered.y()/centered.w())/RoadCamera::imageScale,0);
   }
   // Share the Niro's damped heading. Pivot at the image center so yaw
   // tilts the backdrop with the car without swinging an 85 m lever arm.
   QMatrix4x4 imageHeading;
   imageHeading.translate(-85.f,cameraHeight,-laneOffset);
   imageHeading.rotate(-egoYaw,0,1,0);
   imageHeading.translate(85.f,-cameraHeight,laneOffset);
   // Optional rigid attachment to the same body roll/pitch pivot as the mesh.
   // Original camera pixels and model annotations stay together.
   QMatrix4x4 bodyAttachment;
   bodyAttachment.translate(2.2f*cameraBlend,.5f,-laneOffset);
   bodyAttachment.rotate(-egoYaw,0,1,0);
   bodyAttachment.rotate(cameraRoll*cameraSuspensionBlend,1,0,0);
   bodyAttachment.rotate(cameraPitch*cameraSuspensionBlend,0,0,1);
   bodyAttachment.rotate(egoYaw,0,1,0);
   bodyAttachment.translate(-2.2f*cameraBlend,-.5f,laneOffset);
   QVector4D lifted=cameraMatrix*(anchor+QVector4D(0,cameraHeave*cameraSuspensionBlend,0,0));
   if(lifted.w()>.1f&&actual.w()>.1f)follow.translate(
    (lifted.x()/lifted.w()-actual.x()/actual.w())/RoadCamera::imageScale,
    (lifted.y()/lifted.w()-actual.y()/actual.w())/RoadCamera::imageScale,0);
   QMatrix4x4 videoCamera=follow*centeredCamera*bodyAttachment*imageHeading;
   roadCamera.draw(videoCamera,cameraTransform,cameraHeight,laneOffset,textures[0],textures[1],stream_width,stream_height,cameraBlend,overlayValid&&cameraOverlay?cameraOverlay->textureId():0);
   p.begin(this);p.setRenderHint(QPainter::Antialiasing);
  }
  p.save();p.setClipRect(sceneRect());
  p.setPen(QPen(QColor("#31505e"),1.5));p.setBrush(Qt::NoBrush);QPolygonF ring;for(int i=0;i<=96;i++){float a=i*2*M_PI/96;ring<<project(2.7*std::cos(a)-2.2f*cameraBlend,1.75*std::sin(a),.005);}p.drawPolyline(ring);
  if(valid&&transition>.01&&cameraBlend<.999f){
   const float roadOpacity=transition*(1-cameraBlend);
   p.setOpacity(roadOpacity);
   if(edges[0].valid&&edges[1].valid){QPolygonF asphalt;for(int x=-6;x<=120;x+=2)asphalt<<project(x,curveAt(edges[0],x));for(int x=120;x>=-6;x-=2)asphalt<<project(x,curveAt(edges[1],x));p.setPen(Qt::NoPen);p.setBrush(QColor("#273440"));p.setOpacity(roadOpacity*std::min(edges[0].certainty,edges[1].certainty));p.drawPolygon(asphalt);p.setOpacity(roadOpacity);}
   for(auto&e:edges)ribbon(p,e,.055,QColor(114,140,153,int(150*e.certainty)));
   ribbon(p,path,.82,engaged?QColor(50,191,219,90):QColor(120,159,185,55));
   for(auto&l:lanes)ribbon(p,l,.045,QColor(220,230,235,int(220*l.certainty)),true);
   if(lead){p.setOpacity(roadOpacity*leadAlpha);auto q=project(leadX,pathAt(leadX)+leadY,2.1);p.setFont(QFont("Inter",20));p.setPen(QColor("#b9d7df"));p.drawText(QRectF(q.x()-130,q.y()-45,260,40),Qt::AlignCenter,QString::number(leadX,'f',0)+" m");}
   p.setOpacity(1);
  }

  // Direction diagnostic: silhouette projected onto the road plane by the
  // exact same light vector used for the car (never by the orbit camera).
  QMatrix4x4 shadowFrame;shadowFrame.rotate(-egoYaw,0,1,0);
  QVector3D shadowLight=shadowFrame.mapVector(sunDirection).normalized();
  if(lightMix>.01f&&daylight>.01f&&shadowLight.y()>.04f){
   QMatrix4x4 body;body.translate(2.2f*cameraBlend,.5f+bodyHeave,0);
   body.rotate(-egoYaw,0,1,0);body.rotate(bodyRoll,1,0,0);body.rotate(bodyPitch,0,0,1);body.translate(0,-.5f,0);
   std::vector<QPointF> ground;
   for(float y:{.08f,1.15f,1.55f})for(float x:{-2.15f,2.05f})for(float z:{-.86f,.86f}){
    // Roof is inset from the bumper footprint; diagnostic hull, not shadow map.
    QVector3D w=body.map(QVector3D(y>1.5f?x*.60f:x,y,y>1.5f?z*.82f:z));
    float travel=std::min(w.y()/shadowLight.y(),12.f);
    ground.emplace_back(w.x()-shadowLight.x()*travel,w.z()-shadowLight.z()*travel);
   }
   // Cache a feathered mask once. Two textured quads replace 56 blended
   // vector paths and thousands of projections on every frame.
   static const QImage mask=[] {
    QImage im(256,256,QImage::Format_ARGB32_Premultiplied);im.fill(Qt::transparent);
    for(int y=0;y<256;y++)for(int x=0;x<256;x++){
     float u=(x-127.5f)/127.5f,v=(y-127.5f)/127.5f;
     float r=std::pow(std::pow(u,4)+std::pow(v,4),.25f);
     float t=std::clamp((r-.28f)/.72f,0.f,1.f),a=.72f*(1-t*t*(3-2*t));
     im.setPixel(x,y,qRgba(0,0,0,int(255*a)));
    }return im;
   }();
   auto inverse=shadowFrame.inverted();float minX=1e6,maxX=-1e6,minZ=1e6,maxZ=-1e6;
   for(auto q:ground){auto v=inverse.mapVector(QVector3D(q.x(),0,q.y()));minX=std::min(minX,v.x());maxX=std::max(maxX,v.x());minZ=std::min(minZ,v.z());maxZ=std::max(maxZ,v.z());}
   auto drawMask=[&](float cx,float cz,float rx,float rz,float opacity){
    QPolygonF screen;for(auto c:{QPointF(-1,-1),QPointF(1,-1),QPointF(1,1),QPointF(-1,1)}){
     auto v=shadowFrame.mapVector(QVector3D(cx+c.x()*rx,0,cz+c.y()*rz));screen<<project(-v.x(),-v.z(),.012f);
    }
    QPolygonF source;source<<QPointF(0,0)<<QPointF(256,0)<<QPointF(256,256)<<QPointF(0,256);
    QTransform transform;if(!QTransform::quadToQuad(source,screen,transform))return;
    p.save();p.setOpacity(opacity);p.setRenderHint(QPainter::SmoothPixmapTransform);p.setWorldTransform(transform,true);p.drawImage(QPointF(0,0),mask);p.restore();
   };
   drawMask((minX+maxX)*.5f,(minZ+maxZ)*.5f,(maxX-minX)*.67f,(maxZ-minZ)*.67f,.85f*lightMix*daylight);
   auto contact=inverse.mapVector(QVector3D(2.2f*cameraBlend,0,0));
   drawMask(contact.x(),contact.z(),2.4f,1.05f,.60f*lightMix*daylight);
  }
  p.restore();p.end();
  auto vp=sceneRect();float pixel=devicePixelRatioF();
  float wheel=carValid?smCarWheel():0;
  // sunDirection is already relative to the measured GPS vehicle course.
  // Apply the SAME presentation yaw as the car; do not count steering twice.
  // Orbit/view rotation is deliberately absent from this transform.
  QMatrix4x4 lightFrame;lightFrame.rotate(-egoYaw,0,1,0);
  niro.setSunLight(lightFrame.mapVector(sunDirection).normalized(),sunTint,daylight,lightMix);
  niro.setCameraReflection(ambientTexture,ambientBlend);
  niro.setCameraPanorama(textures[0],textures[1],cameraAge.isValid()&&cameraAge.elapsed()<500&&stream_width>0&&stream_height>0);
  niro.setReflectionHistory(reflectionHistory.older,reflectionHistory.newer,reflectionHistory.blend);
  niro.setCabinLight(cabinLevel);
  niro.draw(cameraMatrix,eye,wheel,width()*devicePixelRatioF(),height()*devicePixelRatioF(),QVector3D(2.2f*cameraBlend,0,-laneOffset),-egoYaw,true,1,QRect(vp.left()*pixel,(height()-vp.bottom())*pixel,vp.width()*pixel,vp.height()*pixel),false,bodyRoll,bodyPitch,bodyHeave,wheelSpin);
  if(valid&&lead&&transition>.01&&cameraBlend<.999f){float y=pathAt(leadX)+leadY;float heading=std::atan2(pathAt(leadX+2)-pathAt(leadX-2),4.f)*180/M_PI;niro.draw(cameraMatrix,eye,0,width()*devicePixelRatioF(),height()*devicePixelRatioF(),QVector3D(-leadX,0,-y-laneOffset),-heading,false,.62f*transition*leadAlpha*(1-cameraBlend),QRect(vp.left()*pixel,(height()-vp.bottom())*pixel,vp.width()*pixel,vp.height()*pixel),true);}
  p.begin(this);p.setRenderHint(QPainter::Antialiasing);
  p.save();p.setClipRect(sceneRect());
  auto car=(*uiStateSP()->sm)["carState"].getCarState();bool blink=clock.elapsed()%800<400;
  // Project the same four lamp anchors as the manual HTML scene. Hide far-side lamps.
  if(carValid){for(int side:{-1,1})for(int end:{-1,1}){bool visible=(end==1?eye.x()>0:eye.x()<0)||side*eye.z()>0;
   bool on=blink&&(side>0?car.getLeftBlinker():car.getRightBlinker());if(!on||!visible)continue;
   float lampX=end<0?1.75f:-1.85f,lampY=-side*.74f,yaw=egoYaw*M_PI/180;auto at=project(lampX*std::cos(yaw)-lampY*std::sin(yaw)-2.2f*cameraBlend,lampX*std::sin(yaw)+lampY*std::cos(yaw),end<0?.74f:1.05f);p.setPen(QPen(QColor("#ffb234"),7));p.drawLine(at-QPointF(6,0),at+QPointF(6,0));}
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
  text(64,1020,16,demoManual?"DEMOANSICHT · DATEN AUS REPLAY":"STATUS AUS AUFZEICHNUNG · REPLAY",muted);text(850,1020,16,cameraMode?(cameraAge.isValid()&&cameraAge.elapsed()<500?"HYBRID · KAMERABILD":"Kamerabild fehlt · 3D-Ansicht"):"Gestrichelt: Spur (schematisch) · Durchgezogen: Fahrbahnrand",muted);
 }
};
