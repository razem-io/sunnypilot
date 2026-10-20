#pragma once
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <QThread>
#include "msgq/visionipc/visionipc_client.h"

// Reduce driver frames to one scalar. No cabin image is displayed or retained.
// Driver IR/auto exposure is not a visible-colour or calibrated lux measurement.
class CabinLight : public QThread {
 std::atomic<float> level{0};
 std::atomic<long long> received{0};
 static long long now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 void run() override {
  while(!isInterruptionRequested()){
   auto streams=VisionIpcClient::getAvailableStreams("camerad",false);
   if(!streams.count(VISION_STREAM_DRIVER)){msleep(250);continue;}
   VisionIpcClient client("camerad",VISION_STREAM_DRIVER,true);
   if(!client.connect(false)){msleep(250);continue;}
   auto last=now();bool reported=false;
   while(!isInterruptionRequested()){
    auto *frame=client.recv(nullptr,100);
    if(!frame){if(now()-last>2000)break;continue;}
    last=now();
    if(!frame->y||frame->width<32||frame->height<24)continue;
    float sum=0;int samples=0;
    for(size_t y=frame->height/8;y<frame->height*7/8;y+=frame->height/24)
     for(size_t x=frame->width/8;x<frame->width*7/8;x+=frame->width/32){sum+=frame->y[y*frame->stride+x]/255.f;samples++;}
    level.store(sum/std::max(1,samples));received.store(last);
    if(!reported){fprintf(stderr,"CABIN: driver frames reduced to ambient brightness\n");fflush(stderr);reported=true;}
    msleep(100); // at most 10 small reductions per second
   }
  }
 }
public:
 ~CabinLight(){requestInterruption();wait();}
 float value() const{return now()-received.load()<750?level.load():0.f;}
 bool fresh() const{return now()-received.load()<750;}
};
