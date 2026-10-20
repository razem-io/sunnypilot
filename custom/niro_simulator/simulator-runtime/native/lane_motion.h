#pragma once
#include <cmath>
#include <algorithm>
// Ego-frame lane-center movement, unwrapped when the model changes lane indices.
inline float niroLaneDelta(float previous,float current,float width){
 float d=current-previous;if(width<2||width>5||!std::isfinite(d))return 0;
 if(d>width*.5f)d-=width;else if(d<-width*.5f)d+=width;
 return std::clamp(-d,-.35f,.35f);
}
