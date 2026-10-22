#include <cstdio>
#include <dlfcn.h>
#include <cstdlib>
#include "ion_buffer.h"
int main(){
  auto wl=dlopen("libwayland-client.so.0",RTLD_NOW|RTLD_GLOBAL);
  auto egl=dlopen("/usr/lib/aarch64-linux-gnu/libEGL_adreno.so",RTLD_NOW|RTLD_GLOBAL);
  auto gl=dlopen("/usr/lib/aarch64-linux-gnu/libGLESv2_adreno.so",RTLD_NOW|RTLD_GLOBAL);
  if(!wl||!egl||!gl){fprintf(stderr,"dlopen: %s\n",dlerror());return 1;}
  auto connect=(void*(*)(const char*))dlsym(wl,"wl_display_connect");
  auto disconnect=(void(*)(void*))dlsym(wl,"wl_display_disconnect");
  auto getDisplay=(void*(*)(void*))dlsym(egl,"eglGetDisplay");
  auto initialize=(unsigned(*)(void*,int*,int*))dlsym(egl,"eglInitialize");
  auto query=(const char*(*)(void*,int))dlsym(egl,"eglQueryString");
  auto terminate=(unsigned(*)(void*))dlsym(egl,"eglTerminate");
  void *native=connect(nullptr);if(!native){puts("no Wayland display");return 1;}
  void *display=getDisplay(native);int major=0,minor=0;
  if(!initialize(display,&major,&minor)){puts("EGL initialization failed");return 1;}
  printf("EGL %d.%d vendor=%s\nextensions=%s\n",major,minor,query(display,0x3053),query(display,0x3055));
  auto choose=(unsigned(*)(void*,const int*,void**,int,int*))dlsym(egl,"eglChooseConfig");
  auto createContext=(void*(*)(void*,void*,void*,const int*))dlsym(egl,"eglCreateContext");
  auto makeCurrent=(unsigned(*)(void*,void*,void*,void*))dlsym(egl,"eglMakeCurrent");
  auto proc=(void*(*)(const char*))dlsym(egl,"eglGetProcAddress");
  const int configAttrs[]={0x3033,1,0x3040,64,0x3024,8,0x3023,8,0x3022,8,0x3021,8,0x3038};
  void *config=nullptr;int count=0;choose(display,configAttrs,&config,1,&count);
  const int contextAttrs[]={0x3098,3,0x3038};void *context=createContext(display,config,nullptr,contextAttrs);
  if(!context||!makeCurrent(display,nullptr,nullptr,context)){puts("context failed");return 2;}
  auto getString=(const char*(*)(unsigned))dlsym(gl,"glGetString");
  printf("renderer=%s\n",getString(0x1F01));
  int ion=open("/dev/ion",O_RDWR);check(ion,"ion");Ion buffer(ion,2220032);
  const int imageAttrs[]={0x3057,320,0x3056,1120,0x3271,0x34324241,0x3272,buffer.fd,0x3273,0,0x3274,1280,0x3038};
  auto createImage=(void*(*)(void*,void*,unsigned,void*,const int*))proc("eglCreateImageKHR");
  auto getError=(unsigned(*)())dlsym(egl,"eglGetError");
  void *image=createImage(display,nullptr,0x3270,nullptr,imageAttrs);
  printf("ION EGLImage=%p error=%x\n",image,getError());if(!image)return 3;
  unsigned texture=0,fbo=0;
  auto genTextures=(void(*)(int,unsigned*))dlsym(gl,"glGenTextures");
  auto bindTexture=(void(*)(unsigned,unsigned))dlsym(gl,"glBindTexture");
  auto imageTarget=(void(*)(unsigned,void*))proc("glEGLImageTargetTexture2DOES");
  auto genFbos=(void(*)(int,unsigned*))dlsym(gl,"glGenFramebuffers");
  auto bindFbo=(void(*)(unsigned,unsigned))dlsym(gl,"glBindFramebuffer");
  auto attach=(void(*)(unsigned,unsigned,unsigned,unsigned,int))dlsym(gl,"glFramebufferTexture2D");
  auto status=(unsigned(*)(unsigned))dlsym(gl,"glCheckFramebufferStatus");
  genTextures(1,&texture);bindTexture(0x0DE1,texture);imageTarget(0x0DE1,image);
  genFbos(1,&fbo);bindFbo(0x8D40,fbo);attach(0x8D40,0x8CE0,0x0DE1,texture,0);
  unsigned complete=status(0x8D40);printf("ION framebuffer status=%x\n",complete);if(complete!=0x8CD5)return 4;
  auto color=(void(*)(float,float,float,float))dlsym(gl,"glClearColor");auto clear=(void(*)(unsigned))dlsym(gl,"glClear");auto finish=(void(*)())dlsym(gl,"glFinish");
  color(.5,.25,.75,.125);clear(0x4000);finish();buffer.sync(true);
  auto p=(unsigned char*)buffer.ptr;printf("GPU-written ION pixel=%u,%u,%u,%u\n",p[0],p[1],p[2],p[3]);
  if(abs(p[0]-128)>1||abs(p[1]-64)>1||abs(p[2]-191)>1||abs(p[3]-32)>1)return 5;
  puts("PASS GPU renders directly into encoder-compatible ION allocation");
  terminate(display);disconnect(native);
}
