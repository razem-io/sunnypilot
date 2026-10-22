#pragma once
#include "scene_antialias.h"
#include <QVector>
// Cache only the transparent HUD; car, road, lamps and suspension still draw
// every frame. A key of displayed values invalidates it immediately on change.
class NiroHudCache:protected QOpenGLFunctions {
 std::unique_ptr<QOpenGLFramebufferObject> layer;
 std::unique_ptr<NiroTargetPaintDevice> device;
 std::unique_ptr<QOpenGLShaderProgram> program;
 QOpenGLVertexArrayObject vao;
 QVector<qint64> previous;
 GLint destination=0,viewport[4]{};
 bool ready=false;
public:
 GLuint texture()const{return ready&&layer?layer->texture():0;}
 bool changed(QSize size,const QVector<qint64> &key)const {
  return !ready||!layer||layer->size()!=size||key!=previous;
 }
 bool onlyFieldChanged(QSize size,const QVector<qint64> &key,int field)const {
  if(!ready||!layer||layer->size()!=size||key.size()!=previous.size())return false;
  for(int i=0;i<key.size();i++)if(i!=field&&key[i]!=previous[i])return false;
  return true;
 }
 QPaintDevice *begin(QSize size,qreal ratio,const QVector<qint64> &key,QRect dirty={}){
  initializeOpenGLFunctions();glGetIntegerv(GL_FRAMEBUFFER_BINDING,&destination);glGetIntegerv(GL_VIEWPORT,viewport);
  if(!layer||layer->size()!=size){
   QOpenGLFramebufferObjectFormat fmt;fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);fmt.setSamples(0);
   layer=std::make_unique<QOpenGLFramebufferObject>(size,fmt);
   if(!layer->isValid())qFatal("HUD cache framebuffer failed");
   device=std::make_unique<NiroTargetPaintDevice>(size,layer->handle());
  }
  previous=key;ready=false;device->setDevicePixelRatio(ratio);layer->bind();glViewport(0,0,size.width(),size.height());
  if(dirty.isValid()){glEnable(GL_SCISSOR_TEST);glScissor(qRound(dirty.x()*ratio),size.height()-qRound((dirty.y()+dirty.height())*ratio),qRound(dirty.width()*ratio),qRound(dirty.height()*ratio));}else glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
  return device.get();
 }
 void end(){glBindFramebuffer(GL_FRAMEBUFFER,destination);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);ready=true;}
 void draw(){
  if(!ready)return;
  if(!program){
   program=std::make_unique<QOpenGLShaderProgram>();
   const char *v=R"(#version 300 es
precision highp float;
out vec2 uv;
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));uv=p;gl_Position=vec4(p*2.-1.,0.,1.);})";
   const char *f=R"(#version 300 es
precision highp float;
in vec2 uv;out vec4 color;uniform sampler2D image;
void main(){color=texture(image,uv);})";
   if(!program->addShaderFromSourceCode(QOpenGLShader::Vertex,v)||!program->addShaderFromSourceCode(QOpenGLShader::Fragment,f)||!program->link())qFatal("HUD cache shader failed");vao.create();
  }
  glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
  program->bind();vao.bind();glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,layer->texture());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  program->setUniformValue("image",0);glDrawArrays(GL_TRIANGLES,0,3);vao.release();program->release();glBindTexture(GL_TEXTURE_2D,0);glDisable(GL_BLEND);
 }
};
