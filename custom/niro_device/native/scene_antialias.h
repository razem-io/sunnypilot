#pragma once
#include "benchmark_flags.h"
#include <QOpenGLFramebufferObject>
#include <QOpenGLPaintDevice>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <memory>
class NiroTargetPaintDevice:public QOpenGLPaintDevice {
 GLuint target;
public:
 NiroTargetPaintDevice(QSize size,GLuint fbo):QOpenGLPaintDevice(size),target(fbo){}
 void ensureActiveTarget()override{QOpenGLContext::currentContext()->functions()->glBindFramebuffer(GL_FRAMEBUFFER,target);}
};
// Edge antialiasing before the HUD: fonts/icons retain their original resolution.
class NiroSceneAntialias:protected QOpenGLFunctions {
 std::unique_ptr<QOpenGLFramebufferObject> scene;
 std::unique_ptr<QOpenGLShaderProgram> program;
 QOpenGLVertexArrayObject vao;GLint destination=0,destinationViewport[4]{};
public:
 GLuint begin(QSize size){
  initializeOpenGLFunctions();glGetIntegerv(GL_FRAMEBUFFER_BINDING,&destination);glGetIntegerv(GL_VIEWPORT,destinationViewport);
  if(!scene||scene->size()!=size){QOpenGLFramebufferObjectFormat fmt;fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);fmt.setSamples(0);scene=std::make_unique<QOpenGLFramebufferObject>(size,fmt);}
  if(!program){
   program=std::make_unique<QOpenGLShaderProgram>();
   const char *vertex=R"(#version 300 es
precision highp float;
out vec2 uv;
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));uv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)";
   const char *fragment=R"(#version 300 es
precision highp float;
in vec2 uv;out vec4 color;uniform sampler2D image;uniform sampler2D hud;uniform bool combineHud;uniform vec2 pixel;
float luma(vec3 c){return dot(c,vec3(.299,.587,.114));}
vec3 filteredScene(){
 vec3 center=texture(image,uv).rgb;
 float m=luma(center),nw=luma(texture(image,uv+vec2(-1,-1)*pixel).rgb),ne=luma(texture(image,uv+vec2(1,-1)*pixel).rgb);
 float sw=luma(texture(image,uv+vec2(-1,1)*pixel).rgb),se=luma(texture(image,uv+vec2(1,1)*pixel).rgb);
 float lo=min(m,min(min(nw,ne),min(sw,se))),hi=max(m,max(max(nw,ne),max(sw,se)));
 if(hi-lo<max(.03125,hi*.125)){return center;}
 vec2 dir=vec2(-((nw+ne)-(sw+se)),(nw+sw)-(ne+se));
 float reduce=max((nw+ne+sw+se)*.03125,.0078125);
 dir=clamp(dir/(min(abs(dir.x),abs(dir.y))+reduce),vec2(-8),vec2(8))*pixel;
 vec3 a=.5*(texture(image,uv-dir/6.).rgb+texture(image,uv+dir/6.).rgb);
 vec3 b=a*.5+.25*(texture(image,uv-dir*.5).rgb+texture(image,uv+dir*.5).rgb);
 float lb=luma(b);return lb<lo||lb>hi?a:b;
}
void main(){vec3 base=filteredScene();vec4 overlay=combineHud?texture(hud,uv):vec4(0);color=vec4(base*(1.-overlay.a)+overlay.rgb,1);}
)";
   if(!program->addShaderFromSourceCode(QOpenGLShader::Vertex,vertex)||!program->addShaderFromSourceCode(QOpenGLShader::Fragment,fragment)||!program->link())qFatal("Scene antialias shader failed");vao.create();
  }
  if(!scene->isValid())qFatal("Scene antialias target failed");scene->bind();if(niroRenderOption("poc-tile-clear",true)){glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);}return scene->handle();
 }
 void discardDepth(){
  if(!niroBenchmarkFlag("poc-discard-depth"))return;
  const GLenum attachments[]={GL_DEPTH_ATTACHMENT,GL_STENCIL_ATTACHMENT};
  QOpenGLContext::currentContext()->extraFunctions()->glInvalidateFramebuffer(GL_FRAMEBUFFER,2,attachments);
 }
 void finish(GLuint hudTexture=0){
  glBindFramebuffer(GL_FRAMEBUFFER,destination);glViewport(destinationViewport[0],destinationViewport[1],destinationViewport[2],destinationViewport[3]);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
  program->bind();vao.bind();glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,scene->texture());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);program->setUniformValue("image",0);program->setUniformValue("combineHud",hudTexture!=0);program->setUniformValue("hud",1);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,hudTexture);program->setUniformValue("pixel",QVector2D(1.f/scene->width(),1.f/scene->height()));glDrawArrays(GL_TRIANGLES,0,3);vao.release();program->release();glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,0);
 }
};
