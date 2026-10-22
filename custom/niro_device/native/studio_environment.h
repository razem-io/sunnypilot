#pragma once
#include <QOpenGLFunctions>
#include <QVector3D>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
// Static analytic studio environment, tabulated once in linear HDR. The live
// camera panorama and directional sunlight remain evaluated dynamically.
class NiroStudioEnvironment:protected QOpenGLFunctions {
 std::array<GLuint,2> maps{};
 static float smooth(float a,float b,float x){float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
 static QVector3D mix(QVector3D a,QVector3D b,float t){return a*(1-t)+b*t;}
public:
 void initialize(){
  initializeOpenGLFunctions();glGenTextures(2,maps.data());constexpr int size=128;
  const auto box=QVector3D(-.4f,1,.5f).normalized(),strip=QVector3D(.5f,.7f,-.7f).normalized();
  for(int layer=0;layer<2;layer++){
   glBindTexture(GL_TEXTURE_CUBE_MAP,maps[layer]);
   glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
   for(int face=0;face<6;face++){
    std::vector<float> data(size*size*4);
    for(int y=0;y<size;y++)for(int x=0;x<size;x++){
     float u=2*(x+.5f)/size-1,v=2*(y+.5f)/size-1;
     std::array<QVector3D,6> dirs={QVector3D(1,-v,-u),QVector3D(-1,-v,u),QVector3D(u,1,v),QVector3D(u,-1,-v),QVector3D(u,-v,1),QVector3D(-u,-v,-1)};
     const auto R=dirs[face].normalized();QVector3D color;
     if(layer==0){
      float horizon=.04f+.025f*std::sin(R.x()*4+R.z()*2);
      auto sky=mix({.48f,.52f,.60f},{.025f,.035f,.055f},smooth(.18f,.85f,R.y()));
      color=mix({.035f,.032f,.030f},sky,smooth(horizon-.035f,horizon+.10f,R.y()));
      float h=(R.y()-horizon)/.065f;color*=1-.68f*std::exp(-h*h);
      float cloud=(R.y()-.38f-.08f*std::sin(R.x()*3))/.14f;color+=QVector3D(.25f,.25f,.24f)*std::exp(-cloud*cloud);
     } else {
      float a=std::pow(std::max(QVector3D::dotProduct(R,box),0.f),18.f),b=std::pow(std::max(QVector3D::dotProduct(R,strip),0.f),35.f);
      color=QVector3D(.9f,.93f,.98f)*a*.75f+QVector3D(.85f,.90f,.96f)*b*.45f;
     }
     int i=(y*size+x)*4;data[i]=color.x();data[i+1]=color.y();data[i+2]=color.z();data[i+3]=1;
    }
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA16F,size,size,0,GL_RGBA,GL_FLOAT,data.data());
   }
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP,0);
 }
 void bind(){glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_CUBE_MAP,maps[0]);glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_CUBE_MAP,maps[1]);glActiveTexture(GL_TEXTURE0);}
 void release(){glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_CUBE_MAP,0);glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_CUBE_MAP,0);glActiveTexture(GL_TEXTURE0);}
};
