#pragma once
#include <QTimer>
#include <ctime>
#include <cmath>
#include <functional>
// Benchmark-only absolute cadence, anchored to hardware presentation timestamps.
// Never blocks the GUI thread and never changes display or device clocks.
class NiroPhasePacer {
 QTimer timer;
 std::function<void()> repaint;
 qint64 phase=0,period=16830000,lead=12000000;
 static qint64 now(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return qint64(t.tv_sec)*1000000000LL+t.tv_nsec;}
 void arm(){
  const auto current=now();
  if(!phase){timer.start(16);return;}
  const auto count=std::max<qint64>(1,(current+lead-phase)/period+1);
  const auto deadline=phase+count*period-lead;
  timer.start(int(std::max<qint64>(1,(deadline-current+999999)/1000000)));
 }
public:
 void start(QObject *owner,int leadMs,std::function<void()> callback){lead=qint64(leadMs)*1000000;repaint=std::move(callback);timer.setSingleShot(true);timer.setTimerType(Qt::PreciseTimer);QObject::connect(&timer,&QTimer::timeout,owner,[this]{if(repaint)repaint();arm();});arm();}
 void presented(uint64_t stamp){
  // Reject another clock domain or malformed timestamps.
  if(std::llabs(now()-qint64(stamp))>1000000000LL)return;
  if(phase&&stamp>uint64_t(phase)){
   auto dt=qint64(stamp)-phase;auto ticks=std::max<qint64>(1,std::llround(double(dt)/period));auto measured=dt/ticks;
   if(measured>15000000&&measured<18000000)period=(period*15+measured)/16;
  }
  phase=stamp;
 }
};
