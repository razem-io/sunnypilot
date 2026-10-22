#pragma once
#include <algorithm>
#include <cmath>
namespace niro_visual {
inline float blend(float dt,float tau){return 1.f-std::exp(-std::max(0.f,dt)/tau);}
inline float leadOpacity(float missingSeconds){return std::clamp(3.f-missingSeconds,0.f,1.f);}
inline float retainedOpacity(float missingSeconds){return std::clamp(1.f-(missingSeconds-5.f)/2.f,0.f,1.f);}
}
