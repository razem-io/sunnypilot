#pragma once
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <array>
#include <cstdio>
// Asynchronous queries: never block the rendering thread waiting for the GPU.
class NiroGpuTiming {
 using Gen=void(*)(GLsizei,GLuint*);using Begin=void(*)(GLenum,GLuint);using End=void(*)(GLenum);
 using Available=void(*)(GLuint,GLenum,GLuint*);using Result=void(*)(GLuint,GLenum,unsigned long long*);
 Gen gen=nullptr;Begin beginQuery=nullptr;End endQuery=nullptr;Available available=nullptr;Result result=nullptr;
 std::array<GLuint,8> ids{};std::array<bool,8> pending{};int head=0;bool active=false;
 double total=0;int samples=0;
public:
 void initialize(){
  auto *c=QOpenGLContext::currentContext();
  if(!c||!c->hasExtension("GL_EXT_disjoint_timer_query")){fprintf(stderr,"GPU_TIMER unsupported\n");return;}
  gen=reinterpret_cast<Gen>(c->getProcAddress("glGenQueriesEXT"));
  beginQuery=reinterpret_cast<Begin>(c->getProcAddress("glBeginQueryEXT"));endQuery=reinterpret_cast<End>(c->getProcAddress("glEndQueryEXT"));
  available=reinterpret_cast<Available>(c->getProcAddress("glGetQueryObjectuivEXT"));result=reinterpret_cast<Result>(c->getProcAddress("glGetQueryObjectui64vEXT"));
  if(!gen||!beginQuery||!endQuery||!available||!result){gen=nullptr;return;}
  gen(ids.size(),ids.data());fprintf(stderr,"GPU_TIMER asynchronous queries enabled\n");
 }
 void begin(){
  active=false;if(!gen)return;
  GLint disjoint=0;QOpenGLContext::currentContext()->functions()->glGetIntegerv(0x8FBB,&disjoint);
  if(disjoint){total=0;samples=0;}
  if(pending[head]){GLuint ready=0;available(ids[head],0x8867,&ready);if(!ready)return;
   unsigned long long ns=0;result(ids[head],0x8866,&ns);pending[head]=false;
   if(!disjoint&&ns<1000000000ULL){total+=ns/1e6;if(++samples==90){fprintf(stderr,"GPU_TIMER paint=%.3fms\n",total/samples);total=0;samples=0;}}
  }
  beginQuery(0x88BF,ids[head]);active=true;
 }
 void end(){if(active){endQuery(0x88BF);pending[head]=true;head=(head+1)%ids.size();active=false;}}
};
