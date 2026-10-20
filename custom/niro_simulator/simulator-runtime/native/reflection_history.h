#pragma once
#include <array>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QElapsedTimer>
// Distance-indexed, GPU-only front-image history for an artistic rear reflection.
class ReflectionHistory:protected QOpenGLFunctions_3_3_Core {
 struct Sample {std::unique_ptr<QOpenGLFramebufferObject> fbo;double distance=0;};
 std::array<Sample,32> samples;int head=-1,count=0;double distance=0;
 QElapsedTimer timer;float captureAge=1;uint32_t frame=0;
 QOpenGLShaderProgram shader;QOpenGLVertexArrayObject vao;
public:
 GLuint older=0,newer=0;float blend=0;
 void initialize(){initializeOpenGLFunctions();vao.create();
  QOpenGLFramebufferObjectFormat fmt;fmt.setInternalTextureFormat(GL_RGBA8);
  for(auto &s:samples){s.fbo=std::make_unique<QOpenGLFramebufferObject>(512,256,fmt);glBindTexture(GL_TEXTURE_2D,s.fbo->texture());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
  shader.addShaderFromSourceCode(QOpenGLShader::Vertex,R"(#version 330 core
  out vec2 uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.-1.,0,1);})");
  shader.addShaderFromSourceCode(QOpenGLShader::Fragment,R"(#version 330 core
  in vec2 uv;out vec4 color;uniform sampler2D luma;uniform sampler2D chroma;
  void main(){float y=texture(luma,uv).r;vec2 c=texture(chroma,uv).rg-.5;color=vec4(pow(clamp(vec3(y+1.402*c.y,y-.344*c.x-.714*c.y,y+1.772*c.x),0.,1.),vec3(2.2)),1);})");
  if(!shader.link())qFatal("Reflection history shader failed");
 }
 void update(GLuint y,GLuint uv,uint32_t id,bool fresh,float speed){
  float dt=timer.isValid()?timer.restart()/1000.f:0;if(!timer.isValid())timer.start();
  if(!fresh){count=0;older=newer=0;captureAge=1;return;}
  if(id<frame||dt>1){count=0;distance=0;captureAge=1;}
  distance+=std::abs(speed)*std::clamp(dt,0.f,.2f);captureAge+=dt;
  if(id!=frame && (count==0||(captureAge>=.1f && distance-samples[head].distance>=.25))){
   frame=id;captureAge=0;head=(head+1)%32;count=std::min(32,count+1);samples[head].distance=distance;
   GLint fbo,vp[4];glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_VIEWPORT,vp);
   samples[head].fbo->bind();glViewport(0,0,512,256);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);
   vao.bind();shader.bind();shader.setUniformValue("luma",0);shader.setUniformValue("chroma",1);
   glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,y);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,uv);glDrawArrays(GL_TRIANGLES,0,3);
   shader.release();vao.release();glActiveTexture(GL_TEXTURE0);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(vp[0],vp[1],vp[2],vp[3]);
  }
  if(!count)return;
  int first=(head-count+33)%32;older=newer=samples[first].fbo->texture();blend=0;
  double target=distance-12.;
  for(int n=1;n<count;n++){int a=(first+n-1)%32,b=(first+n)%32;
   if(target>=samples[a].distance){older=samples[a].fbo->texture();newer=samples[b].fbo->texture();blend=std::clamp(float((target-samples[a].distance)/std::max(.001,samples[b].distance-samples[a].distance)),0.f,1.f);}
  }
 }
};
