#include <array>
#pragma once
struct NiroRoadMesh{GLuint vertex=0,index=0;int count=0,material=0,wheel=0;};
class NiroSceneMesh:protected QOpenGLFunctions {
 QOpenGLVertexArrayObject vao;QOpenGLShaderProgram shader;std::vector<NiroRoadMesh> meshes;std::vector<std::unique_ptr<QOpenGLTexture>> textures;
 GLuint reflectionY=0,reflectionUV=0;float reflectionStrength=0;
public:
 void setCameraReflection(GLuint y,GLuint uv,float strength){reflectionY=y;reflectionUV=uv;reflectionStrength=(y&&uv)?strength:0;}
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
   uniform sampler2D reflectionLuma; uniform sampler2D reflectionChroma; uniform float cameraReflection;
   uniform vec3 eye; uniform float paint; uniform float opacity; uniform float ghost;
   void main(){
    vec4 c=tint;if(textured>0.5)c*=texture(image,tex);if(c.a<0.1)discard;
    if(ghost>0.5){float gray=dot(c.rgb,vec3(.2126,.7152,.0722));c.rgb=mix(vec3(.32,.36,.39),vec3(gray),.35);}
    else if(((local.x < -1.85 && local.y>.38 && local.y<.43 && abs(local.z)<.85) || (local.x < -1.7 && local.y>.66 && local.y<.70 && abs(local.z)>.6) || (local.x>1.85 && local.y>.35 && local.y<.40 && abs(local.z)<.85))) c.rgb=vec3(.03,.56,.69);
    vec3 N=normalize(n);vec3 V=normalize(eye-world);
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
    // Soft camera-colour reflection, not a reconstructed 360-degree environment.
    // Raw NV12 only: UI and predicted-path annotations must never reflect on paint.
    if(cameraReflection>0.001 && coat>0.5){
     vec2 reflectionPoint=clamp(vec2(.5-R.z*.38,.5-R.y*.38),vec2(.12),vec2(.88));
     vec3 reflected=vec3(0.0);
     for(int j=-1;j<=1;j++)for(int i=-1;i<=1;i++){
      vec2 samplePoint=reflectionPoint+vec2(i,j)*.075;
      float y=texture(reflectionLuma,samplePoint).r;
      vec2 uv=texture(reflectionChroma,samplePoint).rg-.5;
      reflected+=pow(clamp(vec3(y+1.402*uv.y,y-.344*uv.x-.714*uv.y,y+1.772*uv.x),0.0,1.0),vec3(2.2))/9.0;
     }
     environment=mix(environment,reflected,cameraReflection*.18);
    }
    // Object-anchored metallic grain; fade subpixel flakes instead of sparkling.
    vec3 cell=floor(local*650.0);
    float grain=fract(sin(dot(cell,vec3(12.9898,78.233,37.719)))*43758.5453);
    float resolved=1.0-smoothstep(.5,1.5,length(fwidth(local*650.0)));
    float flakes=1.0+(grain-.5)*.08*resolved*coat;
    vec3 lit=diffuse+vec3(.98,.98,1.0)*spec*.18;
    // Substrate reflection is tinted; the clearcoat highlights stay neutral.
    vec3 metalTint=mix(vec3(.20),c.rgb,.55);
    vec3 automotive=diffuse*.28*flakes
      +environment*(metalTint*.82+vec3(fresnel*.65))
      +vec3(.98,.98,1.0)*spec*.20;
    lit=mix(lit,automotive,coat);
    fragmentColor=vec4(pow(lit/(lit+vec3(0.65)),vec3(1.0/2.2)),opacity);
   }
  )";
  if(!shader.addShaderFromSourceCode(QOpenGLShader::Vertex,vertex)||!shader.addShaderFromSourceCode(QOpenGLShader::Fragment,fragment)||!shader.link())qFatal("Shader failed");
  textures.resize(4);for(int i=0;i<3;i++){QImage im(qEnvironmentVariable("NIRO_ASSETS_ROOT")+QString("/assets/%1.png").arg(i));if(!im.isNull()){textures[i]=std::make_unique<QOpenGLTexture>(im);textures[i]->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);textures[i]->setMagnificationFilter(QOpenGLTexture::Linear);}}
  QFile file(qEnvironmentVariable("NIRO_RUNTIME")+"/native/assets/niro-suspension.mesh");if(!file.open(QIODevice::ReadOnly))qFatal("Mesh missing");auto read32=[&](){quint32 n;if(file.read(reinterpret_cast<char*>(&n),4)!=4)qFatal("Invalid mesh");return n;};auto count=read32();if(count>32)qFatal("Invalid mesh count");
  for(unsigned i=0;i<count;i++){auto nv=read32(),ni=read32(),material=read32();if(nv>1000000||ni>2000000||material>19)qFatal("Invalid mesh sizes");auto v=file.read(nv*32),indices=file.read(ni*4);if(v.size()!=int(nv*32)||indices.size()!=int(ni*4))qFatal("Truncated mesh");NiroRoadMesh m;m.count=ni;m.material=material%4;m.wheel=material/4;glGenBuffers(1,&m.vertex);glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBufferData(GL_ARRAY_BUFFER,v.size(),v.data(),GL_STATIC_DRAW);glGenBuffers(1,&m.index);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size(),indices.data(),GL_STATIC_DRAW);meshes.push_back(m);}
 }
 void draw(const QMatrix4x4&mvp,const QVector3D&eye,float wheelAngle,int w,int h,QVector3D offset={},float heading=0,bool clear=true,float opacity=1,QRect viewport={},bool ghost=false,float bodyRoll=0,float bodyPitch=0,float bodyHeave=0,std::array<float,4> wheelSpin={}){
 vao.bind();if(viewport.isEmpty())viewport=QRect(0,0,w,h);glViewport(viewport.x(),viewport.y(),viewport.width(),viewport.height());glEnable(GL_SCISSOR_TEST);glScissor(viewport.x(),viewport.y(),viewport.width(),viewport.height());if(clear)glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);shader.bind();shader.setUniformValue("mvp",mvp);shader.setUniformValue("eye",eye);shader.setUniformValue("image",0);shader.setUniformValue("opacity",opacity);shader.setUniformValue("ghost",ghost?1.f:0.f);
  shader.setUniformValue("reflectionLuma",2);shader.setUniformValue("reflectionChroma",3);
  shader.setUniformValue("cameraReflection",ghost?0.f:reflectionStrength);
  glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,reflectionY);
  glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,reflectionUV);glActiveTexture(GL_TEXTURE0);
  for(auto &m:meshes){QMatrix4x4 model;model.translate(offset);model.rotate(heading,0,1,0);if(!m.wheel){model.translate(0,.5f+bodyHeave,0);model.rotate(bodyRoll,1,0,0);model.rotate(bodyPitch,0,0,1);model.translate(0,-.5,0);}if(m.wheel){QVector3D pivot(m.wheel<=2?-1.294f:1.4145f,.3285f,m.wheel%2?.79f:-.79f);model.translate(pivot);if(m.wheel<=2)model.rotate(wheelAngle,0,1,0);model.rotate(wheelSpin[m.wheel-1],0,0,1);model.translate(-pivot);}shader.setUniformValue("model",model);glBindBuffer(GL_ARRAY_BUFFER,m.vertex);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.index);for(auto pair:std::vector<std::pair<const char*,int>>{{"position",0},{"normal",12},{"uv",24}}){int loc=shader.attributeLocation(pair.first);shader.enableAttributeArray(loc);shader.setAttributeBuffer(loc,GL_FLOAT,pair.second,pair.second==24?2:3,32);}bool tex=bool(textures[m.material]);if(tex)textures[m.material]->bind(0);shader.setUniformValue("textured",tex?1.f:0.f);shader.setUniformValue("paint",m.material==3?1.f:0.f);shader.setUniformValue("tint",m.material==3?QVector4D(.205,.225,.25,1):m.material==1?QVector4D(.65,.65,.65,1):QVector4D(1,1,1,1));glDrawElements(GL_TRIANGLES,m.count,GL_UNSIGNED_INT,nullptr);}
 glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);
 shader.release();glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);vao.release();glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glViewport(0,0,w,h);
 }
};
