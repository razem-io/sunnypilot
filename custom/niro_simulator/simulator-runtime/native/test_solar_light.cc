#include "solar_light.h"
#include <cassert>
int main(){
 using namespace niro_solar;
 assert(position(80,365,12,0,0).elevation>87);
 assert(position(80,365,0,0,0).elevation<-87);
 assert(position(172,365,12,51,0).elevation>60);
 assert(position(355,365,12,51,0).elevation<17);
 assert(std::abs(position(80,365,10,0,30).elevation-position(80,365,12,0,0).elevation)<1);
 assert(niro_solar::daylight(-7)==0&&niro_solar::daylight(7)==1);
 float x,y,z;direction({0,0},0,x,y,z);assert(x<-.99&&std::abs(z)<.001);
 direction({0,90},0,x,y,z);assert(z<-.99);direction({0,90},90,x,y,z);assert(x<-.99);
}
