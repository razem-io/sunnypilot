#include "lane_motion.h"
#include <cassert>
int main(){assert(niroLaneDelta(0,0,3.5)==0);assert(niroLaneDelta(0,.1,3.5)<0);assert(niroLaneDelta(0,-.1,3.5)>0);assert(std::abs(niroLaneDelta(1.7,-1.7,3.5)+.1)<.001);assert(niroLaneDelta(0,1,0)==0);assert(std::abs(niroLaneDelta(0,1,3.5))<=.35f);}
