#include "benchmark_flags.h"
#include <array>
#include "studio_environment.h"
#pragma once
struct NiroRoadMesh{std::shared_ptr<QOpenGLVertexArrayObject> input;GLuint vertex=0,index=0;int count=0,material=0,wheel=0,door=-1;GLuint glazingIndex=0;int glazingCount=0;};
class NiroSceneMesh:protected QOpenGLFunctions {
 QOpenGLVertexArrayObject vao;std::array<std::unique_ptr<QOpenGLShaderProgram>,4> shaders;std::vector<NiroRoadMesh> meshes;std::vector<std::unique_ptr<QOpenGLTexture>> textures;
 NiroStudioEnvironment studioEnvironment;
 const bool environmentLut=niroBenchmarkFlag("poc-env-lut");
 const bool jointBatching=niroRenderOption("poc-joint-batching",true);
 std::array<NiroRoadMesh,4> jointMeshes;
 const bool batchState=niroRenderOption("poc-batch-state",true);
 const bool cachedInput=niroRenderOption("poc-cached-input",true);
 const bool cullHiddenFaces=niroRenderOption("poc-cull-backfaces",true);
 GLuint historyOlder=0,historyNewer=0;float historyBlend=0;
 std::array<float,5> doorAngles{};
 float cabinBrightness=0;bool lowBeam=false,highBeam=false,leftIndicator=false,rightIndicator=false,brakeLight=false;
 GLuint panoramaY=0,panoramaUV=0;float panoramaStrength=0;
 GLuint reflectionY=0;float reflectionStrength=0;
 QVector3D lightDirection{-3,5,4},lightColor{1,1,1};float lightDay=1,lightAmount=0;
public:
 void setDoors(const std::array<float,5>&angles){doorAngles=angles;}
 void setBrakeLight(bool on){brakeLight=on;}
 void setIndicators(bool left,bool right){leftIndicator=left;rightIndicator=right;}
 void setHeadlights(bool low,bool high){lowBeam=low;highBeam=high;}
 void setReflectionHistory(GLuint a,GLuint b,float t){historyOlder=a;historyNewer=b;historyBlend=t;}
 void setCabinLight(float value){cabinBrightness=value;}
 void setSunLight(QVector3D direction,QVector3D color,float day,float amount){lightDirection=direction;lightColor=color;lightDay=day;lightAmount=amount;}
 void setCameraPanorama(GLuint y,GLuint uv,bool fresh){panoramaY=y;panoramaUV=uv;panoramaStrength=fresh?1.f:0.f;}
 void setCameraReflection(GLuint texture,float strength){reflectionY=texture;reflectionStrength=texture?strength:0;}
 void initialize(){
  fprintf(stderr,"GRAPHICS: initialize begin\n");fflush(stderr);initializeOpenGLFunctions();fprintf(stderr,"GRAPHICS: functions ready\n");vao.create();vao.bind();fprintf(stderr,"GRAPHICS: VAO ready\n");
  const char *vertex=R"(#version 300 es
precision highp float;
precision highp int;
   layout(location=0) in vec3 position; layout(location=1) in vec3 normal; layout(location=2) in vec2 uv;
   uniform mat4 mvp; uniform mat4 model; out vec3 local; out vec3 n; out vec3 world; out vec2 tex;
   void main(){local=position;n=mat3(model)*normal;world=(model*vec4(position,1.0)).xyz;tex=uv;gl_Position=mvp*vec4(world,1.0);}
  )";
  const char *fragment=R"(#version 300 es
precision highp float;
precision highp int;
   out vec4 fragmentColor;
   in vec3 local; in vec3 n; in vec3 world; in vec2 tex;
   uniform sampler2D image; uniform vec4 tint; uniform float textured;
   uniform vec3 sunDirection;uniform vec3 sunColor;uniform float lightDay;uniform float lightAmount;
   uniform sampler2D cameraEnvironment; uniform float cameraReflection;
   uniform sampler2D rearOlder;uniform sampler2D rearNewer;uniform float rearBlend;uniform float rearAvailable;
   uniform sampler2D panoramaLuma;uniform sampler2D panoramaChroma;uniform float panoramaStrength;uniform float panoramaHeading;
   uniform float brakeLamp;uniform vec2 headlamps;uniform vec2 indicators;
   uniform float glass;uniform float cabinBrightness;uniform float glazingPass;
   uniform vec3 eye; uniform float paint; uniform float opacity; uniform float ghost;
   void main(){
    vec4 c=tint;if(textured>0.5)c*=texture(image,tex);if(c.a<0.1)discard;
    if(ghost>0.5){float gray=dot(c.rgb,vec3(.2126,.7152,.0722));c.rgb=mix(vec3(.32,.36,.39),vec3(gray),.35);}
    else if(((local.x < -1.85 && local.y>.38 && local.y<.43 && abs(local.z)<.85) || (local.x < -1.7 && local.y>.66 && local.y<.70 && abs(local.z)>.6) || (local.x>1.85 && local.y>.35 && local.y<.40 && abs(local.z)<.85))) c.rgb=vec3(.03,.56,.69);
    // Restrict cabin fill to dark upper glazing, excluding lamps in this atlas.
    float pane=glass*step(.88,local.y)*(1.0-smoothstep(.08,.22,max(c.r,max(c.g,c.b))));
    bool isPane=pane>.5 && ghost<.5;
    if(glazingPass<.5 && isPane)discard;
    if(glazingPass>.5 && !isPane)discard;
    vec3 N=normalize(n);vec3 V=normalize(eye-world);
    // Some imported panes are double-sided. Orient the shading normal toward
    // the visible face, then reject light from its back hemisphere.
    if(dot(N,V)<0.0)N=-N;
    vec3 L=normalize(sunDirection);
    float sunFacing=smoothstep(0.0,.06,dot(N,L));
    vec3 key=normalize(vec3(-3.0,5.0,4.0));vec3 fill=normalize(vec3(3.0,2.0,-4.0));
    vec3 base=pow(c.rgb,vec3(2.2));
    vec3 diffuse=base*(0.55+0.85*max(dot(N,key),0.0)+0.65*max(dot(N,fill),0.0));
    float spec=pow(max(dot(N,normalize(key+V)),0.0),mix(24.0,70.0,paint));
    float rim=pow(1.0-max(dot(N,V),0.0),3.0);
    vec3 R=reflect(-V,N);
    float softbox=pow(max(dot(R,normalize(vec3(-0.4,1.0,0.5))),0.0),18.0);
    float strip=pow(max(dot(R,normalize(vec3(0.5,0.7,-0.7))),0.0),35.0);
    // Two-layer automotive finish: graphite base with metallic scattering,
    // plus an uncoloured clearcoat. Reflection directions stay in world space.
    float coat=paint*(1.0-ghost);
    float grazing=pow(1.0-max(dot(N,V),0.0),5.0);
    float fresnel=.04+.96*grazing;
    // Broad sky / dark horizon / ground reflections give curved panels depth.
    // Analytic lighting avoids baking the reference photo's scenery into UVs.
    float horizon=.04+.025*sin(R.x*4.0+R.z*2.0);
    vec3 sky=mix(vec3(.48,.52,.60),vec3(.025,.035,.055),
                 smoothstep(.18,.85,R.y));
    vec3 environment=mix(vec3(.035,.032,.030),sky,
                         smoothstep(horizon-.035,horizon+.10,R.y));
    environment*=1.0-.68*exp(-pow((R.y-horizon)/.065,2.0));
    float cloud=exp(-pow((R.y-.38-.08*sin(R.x*3.0))/ .14,2.0));
    environment+=vec3(.25,.25,.24)*cloud;
    environment+=vec3(.9,.93,.98)*softbox*.75+vec3(.85,.90,.96)*strip*.45;
    // Studio light cards are replaced by a sun-directed broad reflection in Auto.
    float sunGlint=pow(max(dot(R,L),0.0),160.0)*sunFacing;
    environment=mix(environment,
      (environment-vec3(.9,.93,.98)*softbox*.75-vec3(.85,.90,.96)*strip*.45)*.12
      +sunColor*sunGlint*1.2*lightDay,lightAmount);
    vec3 ambient=vec3(0.0),reflected=vec3(0.0);
    if(cameraReflection>0.001 && ghost<0.5){
     // Left / right / overhead colours illuminate the corresponding panels.
     vec3 left=texture(cameraEnvironment,vec2(.17,.46)).rgb;
     vec3 right=texture(cameraEnvironment,vec2(.83,.46)).rgb;
     vec3 top=texture(cameraEnvironment,vec2(.5,.18)).rgb;
     vec3 floorLight=texture(cameraEnvironment,vec2(.5,.82)).rgb;
     ambient=mix(right,left,smoothstep(-.7,.7,N.z));
     ambient=mix(ambient,top,smoothstep(0.0,.8,N.y));
     ambient=mix(ambient,floorLight,smoothstep(0.0,.8,-N.y));
     vec2 reflectionPoint=clamp(vec2(.5-R.z*.40,.5-R.y*.40),vec2(.08),vec2(.92));
     reflected=texture(cameraEnvironment,reflectionPoint).rgb;
    }
    // Stretch the full front image onto a virtual 180-degree hemisphere.
    // Camera frame follows vehicle heading, never the viewer's orbit.
    float coverage=panoramaStrength;
    vec3 panorama=vec3(0);
    if(coverage>.001 && ghost<.5){
     float ch=cos(panoramaHeading),sh=sin(panoramaHeading);
     vec3 cameraR=vec3(ch*R.x-sh*R.z,R.y,sh*R.x+ch*R.z);
     float az=atan(-cameraR.z,-cameraR.x);
     float el=asin(clamp(cameraR.y,-1.0,1.0));
     vec2 panoramaPoint=vec2(.5+az/3.14159265,.5-el/3.14159265);
     vec2 p=clamp(panoramaPoint,vec2(.002),vec2(.998));
     float y=texture(panoramaLuma,p).r;vec2 chroma=texture(panoramaChroma,p).rg-.5;
     panorama=pow(clamp(vec3(y+1.402*chroma.y,y-.344*chroma.x-.714*chroma.y,y+1.772*chroma.x),0.0,1.0),vec3(2.2));
     if(rearAvailable>.5){
      vec2 rearPoint=vec2(.5+sign(az)*(3.14159265-abs(az))/3.14159265,panoramaPoint.y);
      vec3 rear=mix(texture(rearOlder,clamp(rearPoint,vec2(.002),vec2(.998))).rgb,
                    texture(rearNewer,clamp(rearPoint,vec2(.002),vec2(.998))).rgb,rearBlend);
      panorama=mix(panorama,rear,smoothstep(1.15,2.0,abs(az))*rearAvailable);
     }
    }
    // Object-anchored metallic grain; fade subpixel flakes instead of sparkling.
    vec3 cell=floor(local*650.0);
    float grain=fract(sin(dot(cell,vec3(12.9898,78.233,37.719)))*43758.5453);
    float resolved=1.0-smoothstep(.5,1.5,length(fwidth(local*650.0)));
    float flakes=1.0+(grain-.5)*.08*resolved*coat;
    vec3 lit=diffuse+vec3(.98,.98,1.0)*spec*.18;
    // Substrate reflection is tinted; the clearcoat highlights stay neutral.
    vec3 metalTint=mix(vec3(.13),c.rgb,.55);
    vec3 automotive=diffuse*.28*flakes
      +environment*(metalTint*.82+vec3(fresnel*.65))
      +vec3(.98,.98,1.0)*spec*.20;
    lit=mix(lit,automotive,coat);
    // Keep a dim presentation fill at night; no invented moon position.
    float nl=max(dot(N,normalize(sunDirection+vec3(.00001))),0.0);
    // Directional illumination depends only on surface normal and sun, NOT eye.
    vec3 solarDiffuse=base*(.035+.07*lightDay)+base*sunColor*nl*lightDay*2.8;
    // A restrained view-dependent glint sits on top of world-fixed illumination.
    float sunSpec=sunGlint*lightDay;
    vec3 solarPaint=solarDiffuse*.32*flakes+environment*(metalTint*.82+vec3(fresnel*.65))*(.08+.67*lightDay)+sunColor*sunSpec*.65;
    vec3 solar=mix(solarDiffuse+sunColor*sunSpec*.20,solarPaint,coat);
    lit=mix(lit,solar,lightAmount*(1.0-ghost));
    // Independent of solar intensity: tunnel lights still illuminate at night.
    // This is artistic ambient fill from an auto-exposed front camera, not lux.
    lit+=cameraReflection*(1.0-ghost)*(base*ambient*.65
      +coat*reflected*(metalTint*.8+vec3(fresnel*.6))*.35);
    // Restrained moving clearcoat reflection; graphite substrate stays dominant.
    lit+=coat*panorama*coverage*(vec3(.24)+metalTint*.6+vec3(fresnel))*.44;
    // Cabin-only fill: inset from the body shell, excluding paint and glazing.
    // The imported atlas has no named upholstery material, so constrain the
    // effect to dark surfaces inside the passenger compartment.
    float inside=(1.0-smoothstep(.60,.73,abs(local.z)))
      *smoothstep(-1.30,-1.08,local.x)*(1.0-smoothstep(1.45,1.65,local.x))
      *smoothstep(.32,.50,local.y)*(1.0-smoothstep(1.32,1.48,local.y));
    float upholstery=inside*(1.0-paint)*(1.0-glass)*(1.0-ghost)
      *(1.0-smoothstep(.28,.45,max(c.r,max(c.g,c.b))));
    float cabinIrradiance=cabinBrightness*cabinBrightness;
    // A small diffuse reflectance floor preserves detail in black upholstery.
    lit+=upholstery*max(base,vec3(.025)) *vec3(.94,.97,1.0)
      *cabinIrradiance*4.0*(.45+.55*max(N.y,0.0));
    // Tinted glazing: two face passes, after all opaque geometry. No fake cabin emission.
    float alpha=isPane?mix(.13,.40,grazing):1.0;
    if(isPane)lit=vec3(.008,.013,.016)+environment*(.035+.22*grazing)+sunColor*sunSpec*.08+panorama*coverage*(.3+.7*grazing)*.55;
    // Emit only from the imported headlamp lens surfaces, not floating geometry.
    bool headLens=glass>.5 && local.x< -1.45 && local.y>.64 && local.y<1.04
      && abs(local.z)>.40 && min(c.r,min(c.g,c.b))>.30;
    if(ghost<.5 && headLens){
     float inner=1.0-smoothstep(.64,.73,abs(local.z));
     float flutes=.85+.15*cos(local.y*240.0);
     lit+=vec3(.78,.88,1.)*flutes*(headlamps.x*1.9+headlamps.y*inner*4.0);
    }
    float signal=local.z>0.?indicators.x:indicators.y;
    bool rearLens=glass>.5 && local.x>1.55 && local.y>.90 && local.y<1.24 && c.r>c.g*1.8 && c.r>.25;
    if(ghost<.5 && rearLens){
     float ribs=.80+.20*cos(local.y*350.0);
     lit+=vec3(1.,.008,.003)*ribs*(headlamps.x*.75+brakeLamp*3.8);
    }
    if(ghost<.5&&glass>.5&&local.x>1.55&&local.y>1.36&&abs(local.z)<.45&&c.r>c.g*1.8&&c.r>.25)
     lit+=vec3(3.5,.015,.008)*brakeLamp;
    if(ghost<.5 && signal>.5 && (headLens || rearLens)){
     // Keep emission inside original lens geometry and beneath its reflections.
     float lower=headLens?1.0-smoothstep(.86,.93,local.y):1.0-smoothstep(1.07,1.12,local.y);
     float ribs=.75+.25*cos(local.y*350.0);
     vec3 signalLight=headLens?lit*.06+vec3(6.0,.82,.006)*ribs:lit*.35+vec3(3.8,.72,.008)*ribs;
     lit=mix(lit,signalLight,lower*(headLens?.98:.88));
    }
    fragmentColor=vec4(pow(lit/(lit+vec3(0.65)),vec3(1.0/2.2)),opacity*alpha);
   }
  )";
  for(int material=0;material<4;material++){
   auto &shader=*(shaders[material]=std::make_unique<QOpenGLShaderProgram>());
   QByteArray specialized(fragment);
   if(environmentLut){
    specialized.replace("uniform vec3 sunDirection;","uniform samplerCube studioSky;uniform samplerCube studioCards;uniform vec3 sunDirection;");
    int start=specialized.indexOf("    float softbox="),finish=specialized.indexOf("    vec3 ambient=",start);
    if(start<0||finish<start)qFatal("Environment shader replacement failed");
    specialized.replace(start,finish-start,R"(
    float coat=paint*(1.0-ghost);
    float grazing=pow(1.0-max(dot(N,V),0.0),5.0);
    float fresnel=.04+.96*grazing;
    vec3 studioBase=texture(studioSky,R).rgb;
    vec3 studioHighlights=texture(studioCards,R).rgb;
    float sunGlint=pow(max(dot(R,L),0.0),160.0)*sunFacing;
    vec3 environment=mix(studioBase+studioHighlights,studioBase*.12+sunColor*sunGlint*1.2*lightDay,lightAmount);
)");
   }
   specialized.replace("uniform float paint;", material==3?"const float paint=1.0;":"const float paint=0.0;");
   specialized.replace("uniform float glass;", material==2?"const float glass=1.0;":"const float glass=0.0;");
   // Benchmark-only material: separate mesh throughput from PBR pixel cost.
   if(niroBenchmarkFlag("poc-flat-material"))specialized=R"(#version 300 es
precision highp float;
in vec3 n;out vec4 fragmentColor;
uniform vec4 tint;uniform float opacity;
void main(){float l=.35+.65*max(dot(normalize(n),normalize(vec3(-.4,1.,.6))),0.);fragmentColor=vec4(tint.rgb*l,opacity);}
)";
   QByteArray specializedVertex(vertex);
   if(jointBatching){specializedVertex.replace("uniform mat4 model;","layout(location=3) in float joint; uniform mat4 models[10];");specializedVertex.replace("void main(){","void main(){mat4 model=models[int(joint)];");}
   if(!shader.addShaderFromSourceCode(QOpenGLShader::Vertex,specializedVertex)||!shader.addShaderFromSourceCode(QOpenGLShader::Fragment,specialized)||!shader.link())qFatal("Material shader failed");
  }
  fprintf(stderr,"GRAPHICS: textures\n");
  textures.resize(4);for(int i=0;i<3;i++){QImage im(qEnvironmentVariable("NIRO_ASSETS_ROOT")+QString("/assets/%1.png").arg(i));if(!im.isNull()){textures[i]=std::make_unique<QOpenGLTexture>(im);textures[i]->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);textures[i]->setMagnificationFilter(QOpenGLTexture::Linear);}}
  fprintf(stderr,"GRAPHICS: mesh upload\n");const QString runtime=qEnvironmentVariable("NIRO_RUNTIME");
  const bool poc=niroBenchmarkFlag("poc-small-model");
  const bool lod=niroBenchmarkFlag("poc-lod-model");
  const bool cacheModel=niroRenderOption("poc-cache-model",true)&&QFile::exists(runtime+"/native/assets/niro-cache.mesh");
  QFile file(runtime+(poc?"/native/assets/niro-poc.mesh":lod?"/native/assets/niro-lod.mesh":cacheModel?"/native/assets/niro-cache.mesh":"/native/assets/niro-doors.mesh"));if(!file.open(QIODevice::ReadOnly))qFatal("Mesh missing");auto read32=[&](){quint32 n;if(file.read(reinterpret_cast<char*>(&n),4)!=4)qFatal("Invalid mesh");return n;};auto count=read32();if(count>64)qFatal("Invalid mesh count");
  struct JointData{std::vector<float> vertices;std::vector<quint32> indices,glazing;};
  std::array<JointData,4> jointData;
  for(unsigned i=0;i<count;i++){auto nv=read32(),ni=read32(),material=read32();if(nv>1000000||ni>2000000||material>39)qFatal("Invalid mesh sizes");auto v=file.read(nv*32),indices=file.read(ni*4);if(v.size()!=int(nv*32)||indices.size()!=int(ni*4))qFatal("Truncated mesh");NiroRoadMesh m;m.count=ni;m.material=material%4;m.wheel=material/4<=4?material/4:0;m.door=material/4>=5?material/4-5:-1;
   if(jointBatching){
    auto &group=jointData[m.material];const auto base=quint32(group.vertices.size()/9);
    const auto *verts=reinterpret_cast<const float*>(v.constData());const auto *idx=reinterpret_cast<const quint32*>(indices.constData());
    for(unsigned j=0;j<nv;j++){group.vertices.insert(group.vertices.end(),verts+j*8,verts+j*8+8);group.vertices.push_back(float(material/4));}
    for(unsigned j=0;j<ni;j++){if(idx[j]>=nv)qFatal("Invalid mesh index");group.indices.push_back(base+idx[j]);}
    if(m.material==2&&!m.wheel)for(unsigned k=0;k+2<ni;k+=3){bool possible=false;for(int j=0;j<3;j++)possible|=verts[idx[k+j]*8+1]>=.88f;if(possible)for(int j=0;j<3;j++)group.glazing.push_back(base+idx[k+j]);}
    continue;
   }
   glGenBuffers(1,&m.vertex);glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBufferData(GL_ARRAY_BUFFER,v.size(),v.data(),GL_STATIC_DRAW);glGenBuffers(1,&m.index);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size(),indices.data(),GL_STATIC_DRAW);if(m.material==2&&!m.wheel){
    // The shader cannot draw glazing below y=.88. Exclude those triangles
    // before the two transparent passes instead of processing them three times.
    std::vector<quint32> glazing;const auto *idx=reinterpret_cast<const quint32*>(indices.constData());const auto *verts=reinterpret_cast<const float*>(v.constData());
    for(unsigned k=0;k+2<ni;k+=3){bool possible=false;for(int j=0;j<3;j++){if(idx[k+j]>=nv)qFatal("Invalid mesh index");possible|=verts[idx[k+j]*8+1]>=.88f;}
     if(possible)glazing.insert(glazing.end(),idx+k,idx+k+3);}
    m.glazingCount=glazing.size();if(m.glazingCount){glGenBuffers(1,&m.glazingIndex);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.glazingIndex);glBufferData(GL_ELEMENT_ARRAY_BUFFER,glazing.size()*sizeof(quint32),glazing.data(),GL_STATIC_DRAW);}
   }if(cachedInput){
    m.input=std::make_shared<QOpenGLVertexArrayObject>();m.input->create();m.input->bind();
    glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);
    for(int attribute=0;attribute<3;attribute++){glEnableVertexAttribArray(attribute);glVertexAttribPointer(attribute,attribute==2?2:3,GL_FLOAT,GL_FALSE,32,reinterpret_cast<const void*>(attribute==0?0:attribute==1?12:24));}
    m.input->release();vao.bind(); // Keep later uploads away from Qt's default VAO.
   }meshes.push_back(m);}
 if(jointBatching)for(int material=0;material<4;material++){
  auto &data=jointData[material];auto &m=jointMeshes[material];m.material=material;m.count=data.indices.size();m.glazingCount=data.glazing.size();
  if(!m.count)continue;
  vao.bind();glGenBuffers(1,&m.vertex);glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBufferData(GL_ARRAY_BUFFER,data.vertices.size()*sizeof(float),data.vertices.data(),GL_STATIC_DRAW);
  glGenBuffers(1,&m.index);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);glBufferData(GL_ELEMENT_ARRAY_BUFFER,data.indices.size()*sizeof(quint32),data.indices.data(),GL_STATIC_DRAW);
  if(m.glazingCount){glGenBuffers(1,&m.glazingIndex);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.glazingIndex);glBufferData(GL_ELEMENT_ARRAY_BUFFER,data.glazing.size()*sizeof(quint32),data.glazing.data(),GL_STATIC_DRAW);}
  m.input=std::make_shared<QOpenGLVertexArrayObject>();m.input->create();m.input->bind();glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);
  for(int attribute=0;attribute<4;attribute++){glEnableVertexAttribArray(attribute);glVertexAttribPointer(attribute,attribute==3?1:attribute==2?2:3,GL_FLOAT,GL_FALSE,36,reinterpret_cast<const void*>(attribute==0?0:attribute==1?12:attribute==2?24:32));}
  m.input->release();vao.bind();
 }
 if(environmentLut)studioEnvironment.initialize();
 fprintf(stderr,"GRAPHICS: mesh ready\n");
 }
 void draw(const QMatrix4x4&mvp,const QVector3D&eye,float wheelAngle,int w,int h,QVector3D offset={},float heading=0,bool clear=true,float opacity=1,QRect viewport={},bool ghost=false,float bodyRoll=0,float bodyPitch=0,float bodyHeave=0,std::array<float,4> wheelSpin={}){
 if(environmentLut)studioEnvironment.bind();
 vao.bind();if(viewport.isEmpty())viewport=QRect(0,0,w,h);glViewport(viewport.x(),viewport.y(),viewport.width(),viewport.height());glEnable(GL_SCISSOR_TEST);glScissor(viewport.x(),viewport.y(),viewport.width(),viewport.height());if(clear)glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);auto configure=[&](QOpenGLShaderProgram &shader){shader.bind();if(environmentLut){shader.setUniformValue("studioSky",7);shader.setUniformValue("studioCards",8);}shader.setUniformValue("mvp",mvp);shader.setUniformValue("eye",eye);shader.setUniformValue("image",0);shader.setUniformValue("opacity",opacity);shader.setUniformValue("ghost",ghost?1.f:0.f);
  shader.setUniformValue("brakeLamp",brakeLight?1.f:0.f);
  shader.setUniformValue("indicators",QVector2D(leftIndicator?1.f:0.f,rightIndicator?1.f:0.f));
  shader.setUniformValue("headlamps",QVector2D(lowBeam?1.f:0.f,highBeam?1.f:0.f));
  shader.setUniformValue("sunDirection",lightDirection);shader.setUniformValue("sunColor",lightColor);shader.setUniformValue("lightDay",lightDay);shader.setUniformValue("lightAmount",lightAmount);
  shader.setUniformValue("cabinBrightness",cabinBrightness);
  shader.setUniformValue("rearOlder",5);shader.setUniformValue("rearNewer",6);
  shader.setUniformValue("rearBlend",historyBlend);shader.setUniformValue("rearAvailable",historyOlder?1.f:0.f);
  glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,historyOlder);
  glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D,historyNewer);
  shader.setUniformValue("panoramaLuma",3);shader.setUniformValue("panoramaChroma",4);
  shader.setUniformValue("panoramaStrength",ghost?0.f:panoramaStrength);
  shader.setUniformValue("panoramaHeading",heading*float(M_PI/180));
  glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,panoramaY);
  glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,panoramaUV);
  shader.setUniformValue("cameraEnvironment",2);
  shader.setUniformValue("cameraReflection",ghost?0.f:reflectionStrength);
  glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,reflectionY);
  glActiveTexture(GL_TEXTURE0);
  };
  std::array<QMatrix4x4,10> models;
  if(jointBatching){
   QMatrix4x4 frame;frame.translate(offset);frame.rotate(heading,0,1,0);
   auto body=frame;body.translate(0,.5f+bodyHeave,0);body.rotate(bodyRoll,1,0,0);body.rotate(bodyPitch,0,0,1);body.translate(0,-.5f,0);models[0]=body;
   for(int wheel=1;wheel<=4;wheel++){auto m=frame;QVector3D pivot(wheel<=2?-1.294f:1.4145f,.3285f,wheel%2?.79f:-.79f);m.translate(pivot);if(wheel<=2)m.rotate(wheelAngle,0,1,0);m.rotate(wheelSpin[wheel-1],0,0,1);m.translate(-pivot);models[wheel]=m;}
   for(int door=0;door<5;door++){auto m=body;if(!ghost){if(door==4){QVector3D pivot(1.58f,1.48f,0);m.translate(pivot);m.rotate(82.f*doorAngles[4],0,0,1);m.translate(-pivot);}else{bool left=door%2==0;QVector3D pivot(door<2?-.83f:.28f,.85f,left?.79f:-.79f);m.translate(pivot);m.rotate((left?-62.f:62.f)*doorAngles[door],0,1,0);m.translate(-pivot);}}models[5+door]=m;}
  }
  const bool panelsClosed=std::all_of(doorAngles.begin(),doorAngles.end(),[](float angle){return angle<.001f;});
  for(int pass=0;pass<3;pass++){
   // Opaque paint first lets depth rejection avoid shading the hidden cabin.
   if(pass==0&&!ghost&&opacity>=1.f)glDisable(GL_BLEND);else glEnable(GL_BLEND);
   for(int material:std::array<int,4>{3,1,0,2}){
   if(pass>0&&(ghost||material!=2))continue;
   auto &shader=*shaders[material];configure(shader);
   shader.setUniformValue("glazingPass",float(pass));
   if(batchState){bool tex=bool(textures[material]);if(tex)textures[material]->bind(0);shader.setUniformValue("textured",tex?1.f:0.f);shader.setUniformValue("tint",material==3?QVector4D(.135,.15,.17,1):material==1?QVector4D(.65,.65,.65,1):QVector4D(1,1,1,1));}
   glDepthMask(pass==0?GL_TRUE:GL_FALSE);
   if(pass==0){if(cullHiddenFaces&&panelsClosed&&material!=2){glEnable(GL_CULL_FACE);glCullFace(GL_BACK);}else glDisable(GL_CULL_FACE);}else {glEnable(GL_CULL_FACE);glCullFace(pass==1?GL_FRONT:GL_BACK);}
  if(jointBatching){
   auto &m=jointMeshes[material];int count=pass>0?m.glazingCount:m.count;
   if(count){
    if(!batchState){bool tex=bool(textures[material]);if(tex)textures[material]->bind(0);shader.setUniformValue("textured",tex?1.f:0.f);shader.setUniformValue("tint",material==3?QVector4D(.135,.15,.17,1):material==1?QVector4D(.65,.65,.65,1):QVector4D(1,1,1,1));}
    shader.setUniformValueArray("models",models.data(),int(models.size()));m.input->bind();glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pass>0?m.glazingIndex:m.index);glDrawElements(GL_TRIANGLES,count,GL_UNSIGNED_INT,nullptr);
   }shader.release();continue;
  }
  for(auto &m:meshes){if(m.material!=material)continue;if(pass>0&&!m.glazingCount)continue;if(pass>0&&(ghost||m.material!=2))continue;QMatrix4x4 model;model.translate(offset);model.rotate(heading,0,1,0);if(!m.wheel){model.translate(0,.5f+bodyHeave,0);model.rotate(bodyRoll,1,0,0);model.rotate(bodyPitch,0,0,1);model.translate(0,-.5,0);}if(m.wheel){QVector3D pivot(m.wheel<=2?-1.294f:1.4145f,.3285f,m.wheel%2?.79f:-.79f);model.translate(pivot);if(m.wheel<=2)model.rotate(wheelAngle,0,1,0);model.rotate(wheelSpin[m.wheel-1],0,0,1);model.translate(-pivot);}if(m.door>=0&&!ghost){
   if(m.door==4){QVector3D pivot(1.58f,1.48f,0);model.translate(pivot);model.rotate(82.f*doorAngles[4],0,0,1);model.translate(-pivot);}
   else{bool left=m.door%2==0;QVector3D pivot(m.door<2?-.83f:.28f,.85f,left?.79f:-.79f);model.translate(pivot);model.rotate((left?-62.f:62.f)*doorAngles[m.door],0,1,0);model.translate(-pivot);}
  }shader.setUniformValue("model",model);if(cachedInput){m.input->bind();glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pass>0?m.glazingIndex:m.index);}else{glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pass>0?m.glazingIndex:m.index);for(auto pair:std::vector<std::pair<const char*,int>>{{"position",0},{"normal",12},{"uv",24}}){int loc=shader.attributeLocation(pair.first);shader.enableAttributeArray(loc);shader.setAttributeBuffer(loc,GL_FLOAT,pair.second,pair.second==24?2:3,32);}}if(!batchState){bool tex=bool(textures[m.material]);if(tex)textures[m.material]->bind(0);shader.setUniformValue("textured",tex?1.f:0.f);shader.setUniformValue("tint",m.material==3?QVector4D(.135,.15,.17,1):m.material==1?QVector4D(.65,.65,.65,1):QVector4D(1,1,1,1));}glDrawElements(GL_TRIANGLES,pass>0?m.glazingCount:m.count,GL_UNSIGNED_INT,nullptr);}
 shader.release();
 }
 }
 if(environmentLut)studioEnvironment.release();
 glDepthMask(GL_TRUE);glDisable(GL_CULL_FACE);
 glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);
 glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);vao.release();glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glViewport(0,0,w,h);
 }
};
