#pragma once
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <array>
#include <cstring>
#include <memory>

// Copy the existing full widget framebuffer, never call grabFramebuffer (repaint).
// A bounded PBO ring lets rendering complete before CPU access on a later frame.
class ProjectionReadback {
 struct Slot {GLuint buffer=0;GLsync fence=nullptr;bool packed=false;};
 std::array<Slot,3> buffers{};
 std::unique_ptr<QOpenGLFramebufferObject> target;
 std::unique_ptr<QOpenGLFramebufferObject> packedTarget;
 std::unique_ptr<QOpenGLShaderProgram> packProgram;
 GLuint packVao=0,overlayTexture=0;
 qint64 overlayKey=0;
 unsigned head=0,tail=0,pending=0;
public:
 QImage capture(QOpenGLWidget *scene,bool nv12=false,const QImage &overlay={},GLuint sharedFbo=0,GLsync *sharedFence=nullptr,const QImage *upload=nullptr) {
  auto previous=QOpenGLContext::currentContext();auto surface=previous?previous->surface():nullptr;
  scene->makeCurrent();auto f=scene->context()->extraFunctions();
  GLint read=0,draw=0,pack=0,align=0,row=0,viewport[4];
  f->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);f->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
  f->glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);f->glGetIntegerv(GL_PACK_ALIGNMENT,&align);
  f->glGetIntegerv(GL_PACK_ROW_LENGTH,&row);f->glGetIntegerv(GL_VIEWPORT,viewport);
  bool scissor=f->glIsEnabled(GL_SCISSOR_TEST);f->glDisable(GL_SCISSOR_TEST);
  constexpr int w=1280,h=640,bytes=w*h*4;
  if(!target){
   QOpenGLFramebufferObjectFormat format;format.setInternalTextureFormat(GL_RGBA8);
   target=std::make_unique<QOpenGLFramebufferObject>(w,h,format);
   for(auto &slot:buffers){f->glGenBuffers(1,&slot.buffer);f->glBindBuffer(GL_PIXEL_PACK_BUFFER,slot.buffer);f->glBufferData(GL_PIXEL_PACK_BUFFER,bytes,nullptr,GL_STREAM_READ);}
  }
  QImage result;
  if(!sharedFbo&&pending){
   auto &slot=buffers[tail];auto status=f->glClientWaitSync(slot.fence,0,0);
   if(status==GL_ALREADY_SIGNALED||status==GL_CONDITION_SATISFIED){
    f->glBindBuffer(GL_PIXEL_PACK_BUFFER,slot.buffer);
    auto data=f->glMapBufferRange(GL_PIXEL_PACK_BUFFER,0,bytes,GL_MAP_READ_BIT);
    if(data){if(slot.packed==nv12){result=QImage(nv12?w/4:w,nv12?h*3/2:h,QImage::Format_RGBA8888);memcpy(result.bits(),data,result.sizeInBytes());}f->glUnmapBuffer(GL_PIXEL_PACK_BUFFER);}
    f->glDeleteSync(slot.fence);slot.fence=nullptr;tail=(tail+1)%buffers.size();--pending;
   }
  }
  if(sharedFbo||pending<buffers.size()){
   auto &slot=buffers[head];
   f->glBindFramebuffer(GL_READ_FRAMEBUFFER,scene->defaultFramebufferObject());
   f->glBindFramebuffer(GL_DRAW_FRAMEBUFFER,target->handle());
   const int sw=qRound(scene->width()*scene->devicePixelRatioF()),sh=qRound(scene->height()*scene->devicePixelRatioF());
   if(upload){GLint texture=0;f->glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);f->glBindTexture(GL_TEXTURE_2D,target->texture());f->glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,upload->constBits());f->glBindTexture(GL_TEXTURE_2D,texture);}
   else f->glBlitFramebuffer(0,0,sw,sh,0,h,w,0,GL_COLOR_BUFFER_BIT,GL_LINEAR);
   if(nv12){
    GLint program=0,vao=0,active=0,tex0=0,tex1=0;
    f->glGetIntegerv(GL_CURRENT_PROGRAM,&program);f->glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);f->glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    f->glActiveTexture(GL_TEXTURE0);f->glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex0);
    f->glActiveTexture(GL_TEXTURE1);f->glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex1);
    bool blend=f->glIsEnabled(GL_BLEND),depth=f->glIsEnabled(GL_DEPTH_TEST),dither=f->glIsEnabled(GL_DITHER),cull=f->glIsEnabled(GL_CULL_FACE);
    GLboolean mask[4];f->glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    f->glDisable(GL_BLEND);f->glDisable(GL_DEPTH_TEST);f->glDisable(GL_DITHER);f->glDisable(GL_CULL_FACE);f->glColorMask(true,true,true,true);
    if(!packProgram){
     packedTarget=std::make_unique<QOpenGLFramebufferObject>(w/4,h*3/2);
     packProgram=std::make_unique<QOpenGLShaderProgram>();
     packProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,R"(#version 300 es
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0.,1.);})");
     packProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,R"(#version 300 es
precision highp float;
uniform sampler2D picture;
uniform sampler2D controls;
uniform int sharedStorage;
out vec4 packed;
vec3 colorAt(ivec2 p){vec4 o=texelFetch(controls,p,0);return texelFetch(picture,p,0).rgb*(1.-o.a)+o.rgb;}
float luma(ivec2 p){return dot(colorAt(p),vec3(.256788,.504129,.097906))+16./255.;}
vec2 chroma(ivec2 p){vec3 c=(colorAt(p)+colorAt(p+ivec2(1,0))+colorAt(p+ivec2(0,1))+colorAt(p+ivec2(1,1)))*.25;return vec2(dot(c,vec3(-.148223,-.290993,.439216)),dot(c,vec3(.439216,-.367788,-.071427)))+vec2(128./255.);}
void main(){ivec2 p=ivec2(gl_FragCoord.xy);int x=p.x*4;
if(sharedStorage==1){if(p.y<736){if(p.y<40||p.y>=680){packed=vec4(16./255.);return;}p.y-=40;}else{p.y=p.y-736-20;if(p.y<0||p.y>=320){packed=vec4(128./255.);return;}p.y+=640;}}
if(p.y<640){packed=vec4(luma(ivec2(x,p.y)),luma(ivec2(x+1,p.y)),luma(ivec2(x+2,p.y)),luma(ivec2(x+3,p.y)));}else{int y=(p.y-640)*2;packed=vec4(chroma(ivec2(x,y)),chroma(ivec2(x+2,y)));}})");
     if(!packProgram->link())qFatal("NV12 shader: %s",qPrintable(packProgram->log()));
     f->glGenVertexArrays(1,&packVao);f->glGenTextures(1,&overlayTexture);
    }
    f->glActiveTexture(GL_TEXTURE1);f->glBindTexture(GL_TEXTURE_2D,overlayTexture);
    if(overlayKey!=overlay.cacheKey()){
     f->glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,overlay.constBits());
     f->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);f->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);overlayKey=overlay.cacheKey();
    }
    f->glActiveTexture(GL_TEXTURE0);f->glBindTexture(GL_TEXTURE_2D,target->texture());
    f->glBindFramebuffer(GL_DRAW_FRAMEBUFFER,sharedFbo?sharedFbo:packedTarget->handle());f->glViewport(0,0,w/4,sharedFbo?1120:h*3/2);
    packProgram->bind();packProgram->setUniformValue("picture",0);packProgram->setUniformValue("controls",1);packProgram->setUniformValue("sharedStorage",sharedFbo?1:0);
    f->glBindVertexArray(packVao);f->glDrawArrays(GL_TRIANGLES,0,3);
    f->glUseProgram(program);f->glBindVertexArray(vao);
    f->glActiveTexture(GL_TEXTURE0);f->glBindTexture(GL_TEXTURE_2D,tex0);f->glActiveTexture(GL_TEXTURE1);f->glBindTexture(GL_TEXTURE_2D,tex1);f->glActiveTexture(active);
    if(blend)f->glEnable(GL_BLEND);if(depth)f->glEnable(GL_DEPTH_TEST);if(dither)f->glEnable(GL_DITHER);if(cull)f->glEnable(GL_CULL_FACE);f->glColorMask(mask[0],mask[1],mask[2],mask[3]);
   }
   if(sharedFbo){*sharedFence=f->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);f->glFlush();}
   else{
   f->glBindFramebuffer(GL_READ_FRAMEBUFFER,nv12?packedTarget->handle():target->handle());
   f->glBindBuffer(GL_PIXEL_PACK_BUFFER,slot.buffer);f->glPixelStorei(GL_PACK_ALIGNMENT,4);f->glPixelStorei(GL_PACK_ROW_LENGTH,0);
   f->glReadPixels(0,0,nv12?w/4:w,nv12?h*3/2:h,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);slot.packed=nv12;
   slot.fence=f->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);f->glFlush();
   head=(head+1)%buffers.size();++pending;
   }
  }
  f->glBindBuffer(GL_PIXEL_PACK_BUFFER,pack);f->glPixelStorei(GL_PACK_ALIGNMENT,align);f->glPixelStorei(GL_PACK_ROW_LENGTH,row);
  f->glBindFramebuffer(GL_READ_FRAMEBUFFER,read);f->glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
  f->glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);if(scissor)f->glEnable(GL_SCISSOR_TEST);
  scene->doneCurrent();if(previous&&surface)previous->makeCurrent(surface);
  return result;
 }
 // Context teardown owns GL resources. This object lives as long as the UI.
};
