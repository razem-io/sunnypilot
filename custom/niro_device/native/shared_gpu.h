#pragma once
#include "../projection/native/shared_frames.h"
#include "gpu_readback.h"
#include <QSocketNotifier>
#include <QLibrary>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>

// Only buffer handles and completion messages cross the process boundary.
// The encoder owns the ION allocation; GL imports it and returns ownership
// after a fence signals. A slot is never rendered while the encoder uses it.
class SharedGpuFrames : public QObject {
 struct Buffer {int fd=-1;GLuint texture=0,fbo=0;void *image=nullptr;GLsync fence=nullptr;bool busy=false;uint64_t stamp=0;};
 std::array<Buffer,SharedFrames::count> buffers;
 int server=-1,peer=-1;
 QSocketNotifier *listener=nullptr,*reader=nullptr;
 QOpenGLWidget *scene;
 ProjectionReadback &readback;
 bool initialized=false;
 QLibrary egl{"/usr/lib/aarch64-linux-gnu/libEGL_adreno.so"};
 using GetDisplay=void*(*)();using Proc=void*(*)(const char*);
 using Create=void*(*)(void*,void*,unsigned,void*,const int*);
 using Destroy=unsigned(*)(void*,void*);using ImageTarget=void(*)(unsigned,void*);
 GetDisplay display=nullptr;Create create=nullptr;Destroy destroy=nullptr;ImageTarget imageTarget=nullptr;
public:
 uint64_t sent=0,skipped=0;
 SharedGpuFrames(QOpenGLWidget *scene,ProjectionReadback &readback,QObject *parent):QObject(parent),scene(scene),readback(readback){
  server=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
  sockaddr_un address{};address.sun_family=AF_UNIX;strncpy(address.sun_path,SharedFrames::path,sizeof(address.sun_path)-1);
  unlink(address.sun_path);
  if(server<0||bind(server,(sockaddr*)&address,sizeof(address))||chmod(address.sun_path,0600)||listen(server,1))qFatal("Shared frame listener failed");
  display=reinterpret_cast<GetDisplay>(egl.resolve("eglGetCurrentDisplay"));auto proc=reinterpret_cast<Proc>(egl.resolve("eglGetProcAddress"));
  if(!display||!proc)qFatal("EGL entry points unavailable");
  create=reinterpret_cast<Create>(proc("eglCreateImageKHR"));destroy=reinterpret_cast<Destroy>(proc("eglDestroyImageKHR"));imageTarget=reinterpret_cast<ImageTarget>(proc("glEGLImageTargetTexture2DOES"));
  listener=new QSocketNotifier(server,QSocketNotifier::Read,this);
  connect(listener,&QSocketNotifier::activated,this,[this]{
   int incoming=accept4(server,nullptr,nullptr,SOCK_NONBLOCK|SOCK_CLOEXEC);if(incoming<0)return;
   if(peer>=0){close(incoming);return;}peer=incoming;reader=new QSocketNotifier(peer,QSocketNotifier::Read,this);
   connect(reader,&QSocketNotifier::activated,this,[this]{receive();});
  });
 }
 bool active()const{return initialized;}
 void receive(){
  if(!initialized){
   SharedFrames::Pool pool;char ancillary[CMSG_SPACE(sizeof(int)*SharedFrames::count)]{};
   iovec io{&pool,sizeof(pool)};msghdr msg{};msg.msg_iov=&io;msg.msg_iovlen=1;msg.msg_control=ancillary;msg.msg_controllen=sizeof(ancillary);
   ssize_t n=recvmsg(peer,&msg,MSG_CMSG_CLOEXEC);if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return;
   auto rights=CMSG_FIRSTHDR(&msg);int fds[SharedFrames::count];std::fill(std::begin(fds),std::end(fds),-1);
   if(rights&&rights->cmsg_level==SOL_SOCKET&&rights->cmsg_type==SCM_RIGHTS){size_t bytes=std::min(size_t(rights->cmsg_len-CMSG_LEN(0)),sizeof(fds));memcpy(fds,CMSG_DATA(rights),bytes);}
   bool valid=n==sizeof(pool)&&!(msg.msg_flags&(MSG_TRUNC|MSG_CTRUNC))&&pool.magicValue==SharedFrames::magic&&pool.buffers==SharedFrames::count&&pool.width==1280&&pool.height==720&&pool.stride==1280&&pool.scanlines==736&&pool.bytes>=1280*1120;
   for(int fd:fds)valid&=fd>=0;
   if(!valid){for(int fd:fds)if(fd>=0)close(fd);disconnectPeer();return;}
   scene->makeCurrent();auto f=scene->context()->extraFunctions();GLint previous=0,texture=0;f->glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);f->glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
   bool okay=true;
   for(unsigned i=0;i<SharedFrames::count;i++){
    auto &b=buffers[i];b.fd=fds[i];
    const int attrs[]={0x3057,320,0x3056,1120,0x3271,0x34324241,0x3272,b.fd,0x3273,0,0x3274,1280,0x3038};
    b.image=create(display(),nullptr,0x3270,nullptr,attrs);if(!b.image){okay=false;continue;}
    f->glGenTextures(1,&b.texture);f->glBindTexture(GL_TEXTURE_2D,b.texture);imageTarget(GL_TEXTURE_2D,b.image);
    f->glGenFramebuffers(1,&b.fbo);f->glBindFramebuffer(GL_FRAMEBUFFER,b.fbo);f->glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,b.texture,0);
    okay&=f->glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
   }
   f->glBindFramebuffer(GL_FRAMEBUFFER,previous);f->glBindTexture(GL_TEXTURE_2D,texture);scene->doneCurrent();
   if(!okay){disconnectPeer();return;}initialized=true;device()->resetInteractiveTimeout(niroBenchmarkLeaseValid()?600:60);
  }else{
   for(;;){SharedFrames::Packet p;ssize_t n=recv(peer,&p,sizeof(p),MSG_DONTWAIT);
    if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))break;
    if(n!=sizeof(p)||p.kind!=SharedFrames::release||p.index>=buffers.size()||!buffers[p.index].busy||buffers[p.index].fence){disconnectPeer();break;}
    buffers[p.index].busy=false;
   }
  }
 }
 void capture(const QImage &overlay,const QImage *upload=nullptr){
  if(!initialized)return;device()->resetInteractiveTimeout(niroBenchmarkLeaseValid()?600:60);
  scene->makeCurrent();auto f=scene->context()->extraFunctions();int available=-1;
  std::vector<unsigned> ready;
  for(unsigned i=0;i<buffers.size();i++){
   auto &b=buffers[i];if(b.fence){auto result=f->glClientWaitSync(b.fence,0,0);
    if(result==GL_ALREADY_SIGNALED||result==GL_CONDITION_SATISFIED){
     f->glDeleteSync(b.fence);b.fence=nullptr;ready.push_back(i);
    }
    else if(result==GL_WAIT_FAILED){scene->doneCurrent();disconnectPeer();return;}
   }
   if(!b.busy&&available<0)available=i;
  }
  std::sort(ready.begin(),ready.end(),[this](unsigned a,unsigned b){return buffers[a].stamp<buffers[b].stamp;});
  for(unsigned i:ready){SharedFrames::Packet p{SharedFrames::frame,i,buffers[i].stamp};
   if(send(peer,&p,sizeof(p),MSG_DONTWAIT|MSG_NOSIGNAL)!=sizeof(p)){scene->doneCurrent();disconnectPeer();return;}++sent;
  }
  scene->doneCurrent();
  if(available<0){++skipped;return;}
  auto &b=buffers[available];b.busy=true;timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);b.stamp=uint64_t(t.tv_sec)*1000000+t.tv_nsec/1000;
  readback.capture(scene,true,overlay,b.fbo,&b.fence,upload);
 }
 void disconnectPeer(){
  initialized=false;if(reader){reader->setEnabled(false);reader->deleteLater();reader=nullptr;}if(peer>=0){close(peer);peer=-1;}
  if(scene->context()&&scene->isValid()){
   scene->makeCurrent();auto f=scene->context()->extraFunctions();f->glFinish();
   for(auto &b:buffers){if(b.fence)f->glDeleteSync(b.fence);if(b.fbo)f->glDeleteFramebuffers(1,&b.fbo);if(b.texture)f->glDeleteTextures(1,&b.texture);if(b.image)destroy(display(),b.image);if(b.fd>=0)close(b.fd);b=Buffer{};}
   scene->doneCurrent();
  }
 }
 ~SharedGpuFrames(){disconnectPeer();if(server>=0)close(server);unlink(SharedFrames::path);}
};
