// Bounded hardware-encoder feasibility measurement. No camera/CAN/UI changes.
// Uses the public V4L2/ION interface also used by openpilot's V4LEncoder.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <limits>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <dlfcn.h>
#include "shared_frames.h"
#include "v4l2-controls.h"
#include <linux/videodev2.h>
#include <linux/ion.h>
#include "msm_ion.h"

static long long now() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
#include "ion_buffer.h"
static void queue(int fd, unsigned type, unsigned i, Ion &b, long long ts=0) {
  v4l2_plane p{};p.length=b.size;p.bytesused=b.size;p.m.userptr=(unsigned long)b.ptr;p.reserved[0]=b.fd;
  v4l2_buffer v{};v.type=type;v.index=i;v.memory=V4L2_MEMORY_USERPTR;v.m.planes=&p;v.length=1;
  v.flags=V4L2_BUF_FLAG_TIMESTAMP_COPY;v.timestamp.tv_sec=ts/1000000;v.timestamp.tv_usec=ts%1000000;
  call(fd,VIDIOC_QBUF,&v,"QBUF");
}
static void request(int fd,unsigned type,unsigned count) {
  v4l2_requestbuffers r{};r.type=type;r.memory=V4L2_MEMORY_USERPTR;r.count=count;
  call(fd,VIDIOC_REQBUFS,&r,"REQBUFS");
}
int main(int argc,char **argv) try {
  const int fps=argc>1?atoi(argv[1]):60, seconds=argc>2?atoi(argv[2]):10;
  if(fps<1||fps>120||seconds<0||seconds>600)throw std::runtime_error("fps 1..120, seconds 0 (continuous) or 1..600");
  const int width=1280,height=720;
  const bool rgb=argc>4 && std::string(argv[4])=="rgb";
  const bool gpu=argc>5 && std::string(argv[5])=="gpu";
  const bool http=gpu||(argc>5 && std::string(argv[5])=="http");
  const bool shared=argc>5 && std::string(argv[5])=="shared";
  const bool live=http||shared;
  if(seconds==0&&!shared)throw std::runtime_error("Continuous encoding requires shared GPU frames");
  if(live&&rgb)throw std::runtime_error("RGB encoder output is not validated; use NV12");
  int fd=open("/dev/v4l/by-path/platform-aa00000.qcom_vidc-video-index1",O_RDWR|O_NONBLOCK);
  check(fd,"open encoder");
  v4l2_capability cap{};call(fd,VIDIOC_QUERYCAP,&cap,"QUERYCAP");
  fprintf(stderr,"encoder=%s card=%s target=%dx%d@%d\n",cap.driver,cap.card,width,height,fps);
  for(unsigned type:{V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}) {
    for(unsigned i=0;;i++){v4l2_fmtdesc d{};d.type=type;d.index=i;
      if(ioctl(fd,VIDIOC_ENUM_FMT,&d)<0)break;
      fprintf(stderr,"format type=%u %.4s %s\n",type,(char*)&d.pixelformat,d.description);}
  }
  v4l2_format out{};out.type=V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  out.fmt.pix_mp.width=width;out.fmt.pix_mp.height=height;out.fmt.pix_mp.pixelformat=V4L2_PIX_FMT_H264;
  call(fd,VIDIOC_S_FMT,&out,"S_FMT capture");
  v4l2_streamparm sp{};sp.type=V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  sp.parm.output.timeperframe.numerator=1;sp.parm.output.timeperframe.denominator=fps;
  call(fd,VIDIOC_S_PARM,&sp,"S_PARM");
  v4l2_format in{};in.type=V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  in.fmt.pix_mp.width=width;in.fmt.pix_mp.height=height;in.fmt.pix_mp.pixelformat=rgb?V4L2_PIX_FMT_RGB32:V4L2_PIX_FMT_NV12;
  in.fmt.pix_mp.colorspace=V4L2_COLORSPACE_470_SYSTEM_BG;
  call(fd,VIDIOC_S_FMT,&in,"S_FMT input");
  fprintf(stderr,"input bytes=%u stride=%u height=%u output bytes=%u\n",in.fmt.pix_mp.plane_fmt[0].sizeimage,
    in.fmt.pix_mp.plane_fmt[0].bytesperline,in.fmt.pix_mp.height,out.fmt.pix_mp.plane_fmt[0].sizeimage);
  v4l2_control ctrls[]={
    {V4L2_CID_MPEG_VIDEO_BITRATE,8000000},
    {V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES,fps-1},
    {V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES,0},
    {V4L2_CID_MPEG_VIDEO_HEADER_MODE,V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE},
    {V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL,V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_CFR},
    {V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY,V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_DISABLE},
    {V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD,1},
    {V4L2_CID_MPEG_VIDEO_H264_PROFILE,V4L2_MPEG_VIDEO_H264_PROFILE_HIGH},
    {V4L2_CID_MPEG_VIDEO_H264_LEVEL,V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN},
    {V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC},
    {V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL,V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0},
    {V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,0},
    {V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA,0},
    {V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA,0},
    {V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE,0}
  };
  for(auto c:ctrls)call(fd,VIDIOC_S_CTRL,&c,"S_CTRL");
  if(getenv("AA_QCOM_POWER_SAVE")){
    v4l2_control c{V4L2_CID_MPEG_VIDC_VIDEO_PERF_MODE,V4L2_MPEG_VIDC_VIDEO_PERF_POWER_SAVE};
    call(fd,VIDIOC_S_CTRL,&c,"explicit encoder power-save control");
  }
  if(rgb){v4l2_control c{V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC,V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_ENABLE};call(fd,VIDIOC_S_CTRL,&c,"RGB CSC");}
  request(fd,out.type,6);request(fd,in.type,7);
  call(fd,VIDIOC_STREAMON,&out.type,"STREAMON capture");
  call(fd,VIDIOC_STREAMON,&in.type,"STREAMON input");
  int ion=open("/dev/ion",O_RDWR|O_NONBLOCK);check(ion,"open ion");
  std::vector<std::unique_ptr<Ion>> ins,outs;
  std::vector<unsigned> free;
  for(unsigned i=0;i<6;i++) {outs.emplace_back(new Ion(ion,out.fmt.pix_mp.plane_fmt[0].sizeimage));queue(fd,out.type,i,*outs.back());}
  for(unsigned i=0;i<7;i++) {ins.emplace_back(new Ion(ion,in.fmt.pix_mp.plane_fmt[0].sizeimage));free.push_back(i);}
  bool framed=argc>3&&std::string(argv[3])=="-";
  FILE *file=framed?stdout:argc>3?fopen(argv[3],"wb"):nullptr;
  if(argc>3&&!file)throw std::runtime_error("open output file");
  int source=-1;size_t offset=0;std::vector<unsigned char> pixels(gpu?1280*640*3/2:1280*640*4),header;
  using Convert=int (*)(const unsigned char*,int,unsigned char*,int,unsigned char*,int,int,int);
  Convert convert=nullptr;
  if(shared){
    source=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0);check(source,"shared socket");
    sockaddr_un address{};address.sun_family=AF_UNIX;strncpy(address.sun_path,SharedFrames::path,sizeof(address.sun_path)-1);
    check(connect(source,(sockaddr*)&address,sizeof(address)),"connect shared UI");
    SharedFrames::Pool pool;pool.bytes=in.fmt.pix_mp.plane_fmt[0].sizeimage;
    char ancillary[CMSG_SPACE(sizeof(int)*SharedFrames::count)]{};iovec io{&pool,sizeof(pool)};
    msghdr msg{};msg.msg_iov=&io;msg.msg_iovlen=1;msg.msg_control=ancillary;msg.msg_controllen=sizeof(ancillary);
    auto rights=CMSG_FIRSTHDR(&msg);rights->cmsg_level=SOL_SOCKET;rights->cmsg_type=SCM_RIGHTS;rights->cmsg_len=CMSG_LEN(sizeof(int)*SharedFrames::count);
    int fds[SharedFrames::count];for(unsigned i=0;i<SharedFrames::count;i++){ins[i]->sync(false);fds[i]=ins[i]->fd;}memcpy(CMSG_DATA(rights),fds,sizeof(fds));
    if(sendmsg(source,&msg,MSG_NOSIGNAL)!=sizeof(pool))throw std::runtime_error("share pool");
    fcntl(source,F_SETFL,O_NONBLOCK);
  }
  if(http){
    auto lib=dlopen("/data/openpilot/custom/niro_device/release/libyuv.so.0",RTLD_NOW);
    if(!lib)throw std::runtime_error(dlerror());
    convert=reinterpret_cast<Convert>(dlsym(lib,"ABGRToNV12"));
    if(!convert)throw std::runtime_error("ABGRToNV12 unavailable");
    source=socket(AF_INET,SOCK_STREAM,0);check(source,"socket");
    timeval timeout{3,0};setsockopt(source,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(8779);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    check(connect(source,(sockaddr*)&address,sizeof(address)),"connect UI");
    const char *req=gpu?"GET /nv12 HTTP/1.0\r\nHost: localhost\r\n\r\n":"GET /rgba HTTP/1.0\r\nHost: localhost\r\n\r\n";
    if(send(source,req,strlen(req),MSG_NOSIGNAL)!=(ssize_t)strlen(req))throw std::runtime_error("send request");
    std::string response;char ch;
    while(response.size()<4096&&response.find("\r\n\r\n")==std::string::npos){if(recv(source,&ch,1,0)!=1)throw std::runtime_error("HTTP header");response+=ch;}
    if(response.find("200 OK")==std::string::npos)throw std::runtime_error("UI HTTP error");
    fcntl(source,F_SETFL,O_NONBLOCK);
  }
  int sent=0,received=0,headers=0;size_t bytes=0;
  std::vector<double> latency;long long start=now(),finish=seconds?start+seconds*1000000LL:std::numeric_limits<long long>::max()/2,last=start;
  rusage before{};getrusage(RUSAGE_SELF,&before);
  const size_t stride=((width*(rgb?4:1))+127)&~127, scanlines=(height+31)&~31;
  while(now()<finish+3000000 && (live?(now()<finish||received<sent):received<fps*seconds)) {
    long long current=now();
    if(shared&&current<finish){
      SharedFrames::Packet p;ssize_t n=recv(source,&p,sizeof(p),0);
      if(n==0)throw std::runtime_error("shared UI closed");
      if(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)check(n,"shared frame");
      if(n>0){if(n!=sizeof(p)||p.kind!=SharedFrames::frame||p.index>=ins.size())throw std::runtime_error("invalid shared frame");
        queue(fd,in.type,p.index,*ins[p.index],p.timestampUs);sent++;}
    }
    if(http&&offset<pixels.size()&&current<finish&&!free.empty()){
      ssize_t n=recv(source,pixels.data()+offset,pixels.size()-offset,0);
      if(n==0)throw std::runtime_error("UI stream closed");
      if(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)check(n,"read UI");
      if(n>0)offset+=n;
    }
    if(!shared && !free.empty() && (http?(offset==pixels.size()&&current<finish):(sent<fps*seconds&&current>=start+sent*1000000LL/fps))) {
      unsigned idx=free.back();free.pop_back();Ion &b=*ins[idx];
      memset(b.ptr,rgb?0:16,stride*scanlines);memset((char*)b.ptr+stride*scanlines,128,b.size-stride*scanlines);
      // Unique moving bar and changing luma; never repeat a submitted source frame.
      int x=(sent*7)%(width-64);
      if(http){
        if(gpu){memcpy((char*)b.ptr+40*stride,pixels.data(),width*640);memcpy((char*)b.ptr+scanlines*stride+20*stride,pixels.data()+width*640,width*320);}
        else check(convert(pixels.data(),width*4,(unsigned char*)b.ptr+40*stride,stride,
          (unsigned char*)b.ptr+scanlines*stride+20*stride,stride,width,640),"RGBA to NV12");offset=0;
      }else for(int y=80;y<640;y++){
        if(rgb){for(int col=x;col<x+64;col++){auto pixel=(unsigned char*)b.ptr+y*stride+col*4;pixel[0]=255;pixel[1]=sent%160;pixel[2]=0;pixel[3]=255;}}
        else memset((char*)b.ptr+y*stride+x,60+sent%160,64);
      }
      b.sync(false);queue(fd,in.type,idx,b,now());sent++;
    }
    for(unsigned type:{out.type,in.type})for(;;) {
      v4l2_plane p{};v4l2_buffer v{};v.type=type;v.memory=V4L2_MEMORY_USERPTR;v.m.planes=&p;v.length=1;
      int rc=ioctl(fd,VIDIOC_DQBUF,&v);if(rc<0&&errno==EAGAIN)break;check(rc,"DQBUF");
      if(type==in.type){
        if(shared){SharedFrames::Packet p{SharedFrames::release,v.index,0};if(send(source,&p,sizeof(p),MSG_NOSIGNAL)!=sizeof(p))throw std::runtime_error("release shared frame");}
        else free.push_back(v.index);
      }
      else {
        outs.at(v.index)->sync(true);
        auto data=(unsigned char*)outs[v.index]->ptr;
        if(framed){
          if(v.flags&0x00020000)header.assign(data,data+p.bytesused);
          else if(p.bytesused){
            auto size=htonl(uint32_t(header.size()+p.bytesused));
            if(fwrite(&size,1,4,file)!=4||fwrite(header.data(),1,header.size(),file)!=header.size()||fwrite(data,1,p.bytesused,file)!=p.bytesused||fflush(file))throw std::runtime_error("write framed bitstream");
          }
        }else if(file && fwrite(data,1,p.bytesused,file)!=p.bytesused)throw std::runtime_error("write bitstream");
        bytes+=p.bytesused;
        if(v.flags&0x00020000)headers++;
        else if(p.bytesused){received++;latency.push_back((now()-v.timestamp.tv_sec*1000000LL-v.timestamp.tv_usec)/1000.0);
          if(!seconds&&latency.size()>240)latency.erase(latency.begin(),latency.begin()+120);}
        queue(fd,type,v.index,*outs[v.index]);
      }
    }
    if(current-last>=2000000){
      // The sender normally stops the bounded helper before its own deadline.
      // Report latency while running too, so shutdown cannot hide the evidence.
      const size_t count=std::min(size_t(120),latency.size());
      std::vector<double> recent(latency.end()-count,latency.end());std::sort(recent.begin(),recent.end());
      fprintf(stderr,"progress sent=%d encoded=%d elapsed=%.1fs latency_p50_ms=%.2f latency_p95_ms=%.2f\n",sent,received,(current-start)/1e6,count?recent[count/2]:0,count?recent[count*95/100]:0);last=current;
    }
    // No spinning on POLLOUT, which can remain asserted while an input slot is free.
    pollfd pfds[2]={{fd,POLLIN,0},{source,short(((http&&!free.empty()&&offset<pixels.size())||shared)&&current<finish?POLLIN:0),0}};
    // Shared frames and completed encodes both wake poll. No 1 kHz busy wakeups.
    poll(pfds,live?2:1,shared?16:1);
  }
  const double elapsed=(now()-start)/1e6;
  rusage after{};getrusage(RUSAGE_SELF,&after);
  auto cpu=[](rusage &r){return r.ru_utime.tv_sec+r.ru_utime.tv_usec/1e6+r.ru_stime.tv_sec+r.ru_stime.tv_usec/1e6;};
  std::sort(latency.begin(),latency.end());
  fprintf(stderr,"RESULT sent=%d encoded=%d headers=%d elapsed=%.3f fps=%.3f cpu_one_core_pct=%.2f latency_p50_ms=%.2f latency_p95_ms=%.2f bytes=%zu\n",
    sent,received,headers,elapsed,received/elapsed,100*(cpu(after)-cpu(before))/elapsed,
    latency.empty()?0:latency[latency.size()/2],latency.empty()?0:latency[latency.size()*95/100],bytes);
  call(fd,VIDIOC_STREAMOFF,&in.type,"STREAMOFF input");call(fd,VIDIOC_STREAMOFF,&out.type,"STREAMOFF capture");
  request(fd,in.type,0);request(fd,out.type,0);close(fd);ins.clear();outs.clear();close(ion);if(file)fclose(file);
  if(source>=0)close(source);
  return (live?received==sent:received==fps*seconds)?0:2;
}catch(const std::exception &e){fprintf(stderr,"ERROR %s\n",e.what());return 1;}
