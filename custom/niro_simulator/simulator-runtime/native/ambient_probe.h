#pragma once
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QElapsedTimer>
#include <memory>
// Low resolution, linear-light, temporally smoothed camera environment.
// Two FBOs avoid read/write feedback. No CPU image readback.
class AmbientProbe:protected QOpenGLFunctions_3_3_Core {
 QOpenGLShaderProgram shader;QOpenGLVertexArrayObject vao;
 std::unique_ptr<QOpenGLFramebufferObject> buffers[2];int front=0;bool ready=false;QElapsedTimer timer;uint32_t frame=0;
public:
 void initialize(){initializeOpenGLFunctions();vao.create();QOpenGLFramebufferObjectFormat fmt;fmt.setInternalTextureFormat(GL_RGBA16F);
  for(auto &b:buffers){b=std::make_unique<QOpenGLFramebufferObject>(32,16,fmt);glBindTexture(GL_TEXTURE_2D,b->texture());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
  const char* vs=R"(#version 330 core
  out vec2 uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.0-1.0,0,1);})";
  const char* fs=R"(#version 330 core
  in vec2 uv;out vec4 color;uniform sampler2D luma;uniform sampler2D chroma;uniform sampler2D history;uniform float alpha;
  void main(){vec3 rgb=vec3(0);for(int j=-2;j<=2;j++)for(int i=-2;i<=2;i++){
   vec2 p=clamp(uv+vec2(i,j)*vec2(.018,.025),vec2(.01),vec2(.99));
   float y=texture(luma,p).r;vec2 c=texture(chroma,p).rg-.5;
   rgb+=pow(clamp(vec3(y+1.402*c.y,y-.344*c.x-.714*c.y,y+1.772*c.x),0.0,1.0),vec3(2.2))/25.0;
  }color=vec4(alpha>=1.0?rgb:mix(texture(history,uv).rgb,rgb,alpha),1);})";
  if(!shader.addShaderFromSourceCode(QOpenGLShader::Vertex,vs)||!shader.addShaderFromSourceCode(QOpenGLShader::Fragment,fs)||!shader.link())qFatal("Ambient probe shader failed");
 }
 GLuint update(GLuint y,GLuint uv,uint32_t id,bool fresh){
  if(!fresh||!y||!uv){ready=false;return 0;}
  if(ready&&frame==id)return buffers[front]->texture();
  float dt=timer.isValid()?timer.restart()/1000.f:1; if(!timer.isValid())timer.start();
  float alpha=(!ready||id<frame||dt>1)?1.f:1.f-std::exp(-std::clamp(dt,0.f,1.f)/.35f);
  GLint previous=0,viewport[4];glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);glGetIntegerv(GL_VIEWPORT,viewport);
  int next=1-front;buffers[next]->bind();glViewport(0,0,32,16);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);
  vao.bind();shader.bind();shader.setUniformValue("luma",0);shader.setUniformValue("chroma",1);shader.setUniformValue("history",2);shader.setUniformValue("alpha",alpha);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,y);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,uv);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,buffers[front]->texture());glDrawArrays(GL_TRIANGLES,0,3);
  shader.release();vao.release();glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);glBindFramebuffer(GL_FRAMEBUFFER,previous);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
  ready=true;front=next;frame=id;return buffers[front]->texture();
 }
};
