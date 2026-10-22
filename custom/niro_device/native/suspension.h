#pragma once
#include <algorithm>
#include <cmath>
// Visual mass/spring/damper; substeps keep it stable during uneven render timing.
struct NiroSpring {
 float value=0,velocity=0;
 void reset(){value=velocity=0;}
 float step(float target,float dt,float limit){
  if(!std::isfinite(target))target=0;
  target=std::clamp(target,-limit,limit);
  int n=std::max(1,int(std::ceil(dt/.008f)));float h=dt/n;
  for(int i=0;i<n;i++){
   velocity+=(64.f*(target-value)-10.4f*velocity)*h;
   value+=velocity*h;
   if(std::abs(value)>limit){value=std::clamp(value,-limit,limit);velocity=0;}
  }
  return value;
 }
};
