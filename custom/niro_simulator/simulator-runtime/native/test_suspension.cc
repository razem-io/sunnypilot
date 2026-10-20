#include "suspension.h"
#include <cassert>
int main(){NiroSpring a,b;for(int i=0;i<60;i++)a.step(1,1.f/60,2);for(int i=0;i<30;i++)b.step(1,1.f/30,2);assert(std::abs(a.value-b.value)<.02);assert(a.value>.95&&a.value<1.1);for(int i=0;i<300;i++)a.step(0,1.f/60,2);assert(std::abs(a.value)<.001);a.step(100,.1,2);assert(std::abs(a.value)<=2);a.reset();assert(a.value==0&&a.velocity==0);}
