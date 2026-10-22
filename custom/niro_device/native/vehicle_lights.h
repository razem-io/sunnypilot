#pragma once
// No inferred headlight state: common CarState has no low/high-beam field.
struct NiroLights {float low=0,high=0,tail=0,brake=0,left=0,right=0,reverse=0;};
inline NiroLights niroLights(bool valid,bool left,bool right,bool braking,bool reverse,int demo,bool flash){
 NiroLights l;
 if(valid){l.left=left&&flash;l.right=right&&flash;l.brake=braking;l.reverse=reverse;}
 if(demo){l={};switch(demo){
  case 1:l.low=l.tail=1;break;
  case 2:l.low=l.high=l.tail=1;break;
  case 3:l.left=flash;break;
  case 4:l.right=flash;break;
  case 5:l.left=l.right=flash;break;
  case 6:l.brake=l.tail=1;break;
  case 7:l.reverse=l.tail=1;break;
  default:break;
 }}return l;
}
