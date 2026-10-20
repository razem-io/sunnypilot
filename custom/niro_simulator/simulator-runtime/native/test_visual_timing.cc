#include "visual_timing.h"
#include <cassert>
#include <cmath>
int main(){
 using namespace niro_visual;
 assert(retainedOpacity(0)==1);assert(retainedOpacity(4.99f)==1);assert(retainedOpacity(5)==1);
 assert(std::abs(retainedOpacity(6)-.5f)<.00001f);assert(retainedOpacity(7)==0);assert(retainedOpacity(9)==0);
 assert(leadOpacity(0)==1);assert(leadOpacity(2)==1);assert(leadOpacity(2.5f)==.5f);assert(leadOpacity(3)==0);
 float a=0,b=0;for(int i=0;i<30;i++)a+=(1-a)*blend(1.f/30,.65f);for(int i=0;i<60;i++)b+=(1-b)*blend(1.f/60,.65f);
 assert(std::abs(a-b)<.00001f);assert(a>0&&a<1);assert(blend(0,.65f)==0);
}
