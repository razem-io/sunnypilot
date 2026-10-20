#include <array>
#pragma once
struct NiroRoadMesh{GLuint vertex=0,index=0;int count=0,material=0,wheel=0;};
class NiroSceneMesh:protected QOpenGLFunctions {
 QOpenGLVertexArrayObject vao;QOpenGLShaderProgram shader;std::vector<NiroRoadMesh> meshes;std::vector<std::unique_ptr<QOpenGLTexture>> textures;
 GLuint historyOlder=0,historyNewer=0;float historyBlend=0;
 float cabinBrightness=0;
 GLuint panoramaY=0,panoramaUV=0;float panoramaStrength=0;
 GLuint reflectionY=0;float reflectionStrength=0;
 QVector3D lightDirection{-3,5,4},lightColor{1,1,1};float lightDay=1,lightAmount=0;
public:
 void setReflectionHistory(GLuint a,GLuint b,float t){historyOlder=a;historyNewer=b;historyBlend=t;}
 void setCabinLight(float value){cabinBrightness=value;}
 void setSunLight(QVector3D direction,QVector3D color,float day,float amount){lightDirection=direction;lightColor=color;lightDay=day;lightAmount=amount;}
 void setCameraPanorama(GLuint y,GLuint uv,bool fresh){panoramaY=y;panoramaUV=uv;panoramaStrength=fresh?1.f:0.f;}
 void setCameraReflection(GLuint texture,float strength){reflectionY=texture;reflectionStrength=texture?strength:0;}
 void initialize(){
  fprintf(stderr,"GRAPHICS: initialize begin\n");fflush(stderr);initializeOpenGLFunctions();vao.create();vao.bind();
  const char *vertex=R"(#version 330 core
   in vec3 position; in vec3 normal; in vec2 uv;
   uniform mat4 mvp; uniform mat4 model; out vec3 local; out vec3 n; out vec3 world; out vec2 tex;
   void main(){local=position;n=mat3(model)*normal;world=(model*vec4(position,1.0)).xyz;tex=uv;gl_Position=mvp*vec4(world,1.0);}
  )";
  const char *fragment=R"(#version 330 core
   out vec4 fragmentColor;
   in vec3 local; in vec3 n; in vec3 world; in vec2 tex;
   uniform sampler2D image; uniform vec4 tint; uniform float textured;
   uniform vec3 sunDirection;uniform vec3 sunColor;uniform float lightDay;uniform float lightAmount;
   uniform sampler2D cameraEnvironment; uniform float cameraReflection;
   uniform sampler2D rearOlder;uniform sampler2D rearNewer;uniform float rearBlend;uniform float rearAvailable;
   uniform sampler2D panoramaLuma;uniform sampler2D panoramaChroma;uniform float panoramaStrength;uniform float panoramaHeading;
   uniform float glass;uniform float cabinBrightness;uniform float glazingPass;
   uniform vec3 eye; uniform float paint; uniform float opacity; uniform float ghost;
   void main(){
    vec4 c=tint;if(textured>0.5)c*=texture(image,tex);if(c.a<0.1)discard;
    if(ghost>0.5){float gray=dot(c.rgb,vec3(.2126,.7152,.0722));c.rgb=mix(vec3(.32,.36,.39),vec3(gray),.35);}
    else if(((local.x < -1.85 && local.y>.38 && local.y<.43 && abs(local.z)<.85) || (local.x < -1.7 && local.y>.66 && local.y<.70 && abs(local.z)>.6) || (local.x>1.85 && local.y>.35 && local.y<.40 && abs(local.z)<.85))) c.rgb=vec3(.03,.56,.69);
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
    float ch=cos(panoramaHeading),sh=sin(panoramaHeading);
    vec3 cameraR=vec3(ch*R.x-sh*R.z,R.y,sh*R.x+ch*R.z);
    float az=atan(-cameraR.z,-cameraR.x);
    float el=asin(clamp(cameraR.y,-1.0,1.0));
    vec2 panoramaPoint=vec2(.5+az/3.14159265,.5-el/3.14159265);
    float coverage=panoramaStrength;
    vec3 panorama=vec3(0);
    if(coverage>.001 && ghost<.5){
     vec2 p=clamp(panoramaPoint,vec2(.002),vec2(.998));
     float y=texture(panoramaLuma,p).r;vec2 chroma=texture(panoramaChroma,p).rg-.5;
     panorama=pow(clamp(vec3(y+1.402*chroma.y,y-.344*chroma.x-.714*chroma.y,y+1.772*chroma.x),0.0,1.0),vec3(2.2));
    }
    // Reverse the horizontal sweep behind the car; join both hemispheres softly.
    vec2 rearPoint=vec2(.5+sign(az)*(3.14159265-abs(az))/3.14159265,panoramaPoint.y);
    vec3 rear=mix(texture(rearOlder,clamp(rearPoint,vec2(.002),vec2(.998))).rgb,
                  texture(rearNewer,clamp(rearPoint,vec2(.002),vec2(.998))).rgb,rearBlend);
    panorama=mix(panorama,rear,smoothstep(1.15,2.0,abs(az))*rearAvailable);
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
    // Restrict cabin fill to dark upper glazing, excluding lamps in this atlas.
    float pane=glass*step(.88,local.y)*(1.0-smoothstep(.08,.22,max(c.r,max(c.g,c.b))));
    bool isPane=pane>.5 && ghost<.5;
    if(glazingPass<.5 && isPane)discard;
    if(glazingPass>.5 && !isPane)discard;
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
    fragmentColor=vec4(pow(lit/(lit+vec3(0.65)),vec3(1.0/2.2)),opacity*alpha);
   }
  )";
  if(!shader.addShaderFromSourceCode(QOpenGLShader::Vertex,vertex)||!shader.addShaderFromSourceCode(QOpenGLShader::Fragment,fragment)||!shader.link())qFatal("Shader failed");
  textures.resize(4);for(int i=0;i<3;i++){QImage im(qEnvironmentVariable("NIRO_ASSETS_ROOT")+QString("/assets/%1.png").arg(i));if(!im.isNull()){textures[i]=std::make_unique<QOpenGLTexture>(im);textures[i]->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);textures[i]->setMagnificationFilter(QOpenGLTexture::Linear);}}
  QFile file(qEnvironmentVariable("NIRO_RUNTIME")+"/native/assets/niro-suspension.mesh");if(!file.open(QIODevice::ReadOnly))qFatal("Mesh missing");auto read32=[&](){quint32 n;if(file.read(reinterpret_cast<char*>(&n),4)!=4)qFatal("Invalid mesh");return n;};auto count=read32();if(count>32)qFatal("Invalid mesh count");
  for(unsigned i=0;i<count;i++){auto nv=read32(),ni=read32(),material=read32();if(nv>1000000||ni>2000000||material>19)qFatal("Invalid mesh sizes");auto v=file.read(nv*32),indices=file.read(ni*4);if(v.size()!=int(nv*32)||indices.size()!=int(ni*4))qFatal("Truncated mesh");NiroRoadMesh m;m.count=ni;m.material=material%4;m.wheel=material/4;glGenBuffers(1,&m.vertex);glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBufferData(GL_ARRAY_BUFFER,v.size(),v.data(),GL_STATIC_DRAW);glGenBuffers(1,&m.index);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size(),indices.data(),GL_STATIC_DRAW);meshes.push_back(m);}
 }
 void draw(const QMatrix4x4&mvp,const QVector3D&eye,float wheelAngle,int w,int h,QVector3D offset={},float heading=0,bool clear=true,float opacity=1,QRect viewport={},bool ghost=false,float bodyRoll=0,float bodyPitch=0,float bodyHeave=0,std::array<float,4> wheelSpin={}){
 vao.bind();if(viewport.isEmpty())viewport=QRect(0,0,w,h);glViewport(viewport.x(),viewport.y(),viewport.width(),viewport.height());glEnable(GL_SCISSOR_TEST);glScissor(viewport.x(),viewport.y(),viewport.width(),viewport.height());if(clear)glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);shader.bind();shader.setUniformValue("mvp",mvp);shader.setUniformValue("eye",eye);shader.setUniformValue("image",0);shader.setUniformValue("opacity",opacity);shader.setUniformValue("ghost",ghost?1.f:0.f);
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
  for(int pass=0;pass<3;pass++){
   shader.setUniformValue("glazingPass",float(pass));
   glDepthMask(pass==0?GL_TRUE:GL_FALSE);
   if(pass==0)glDisable(GL_CULL_FACE);else {glEnable(GL_CULL_FACE);glCullFace(pass==1?GL_FRONT:GL_BACK);}
  for(auto &m:meshes){if(pass>0&&(ghost||m.material!=2))continue;QMatrix4x4 model;model.translate(offset);model.rotate(heading,0,1,0);if(!m.wheel){model.translate(0,.5f+bodyHeave,0);model.rotate(bodyRoll,1,0,0);model.rotate(bodyPitch,0,0,1);model.translate(0,-.5,0);}if(m.wheel){QVector3D pivot(m.wheel<=2?-1.294f:1.4145f,.3285f,m.wheel%2?.79f:-.79f);model.translate(pivot);if(m.wheel<=2)model.rotate(wheelAngle,0,1,0);model.rotate(wheelSpin[m.wheel-1],0,0,1);model.translate(-pivot);}shader.setUniformValue("model",model);glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);for(auto pair:std::vector<std::pair<const char*,int>>{{"position",0},{"normal",12},{"uv",24}}){int loc=shader.attributeLocation(pair.first);shader.enableAttributeArray(loc);shader.setAttributeBuffer(loc,GL_FLOAT,pair.second,pair.second==24?2:3,32);}bool tex=bool(textures[m.material]);if(tex)textures[m.material]->bind(0);shader.setUniformValue("textured",tex?1.f:0.f);shader.setUniformValue("paint",m.material==3?1.f:0.f);shader.setUniformValue("glass",m.material==2?1.f:0.f);shader.setUniformValue("tint",m.material==3?QVector4D(.135,.15,.17,1):m.material==1?QVector4D(.65,.65,.65,1):QVector4D(1,1,1,1));glDrawElements(GL_TRIANGLES,m.count,GL_UNSIGNED_INT,nullptr);}
 }
 glDepthMask(GL_TRUE);glDisable(GL_CULL_FACE);
 glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);
 shader.release();glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);vao.release();glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glViewport(0,0,w,h);
 }
};
