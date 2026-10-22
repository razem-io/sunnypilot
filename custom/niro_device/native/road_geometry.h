#pragma once
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QColor>
#include <vector>
#include <memory>
// One ordered triangle stream for the flat road. Analytic side coverage keeps
// thin markings smooth; 3D projection and clipping use the same scene matrix.
class NiroRoadGeometry:protected QOpenGLFunctions {
 struct Vertex{float x,y,z,r,g,b,a,across,half;};
 std::vector<Vertex> vertices;
 std::unique_ptr<QOpenGLShaderProgram> shader;
 QOpenGLVertexArrayObject vao;GLuint buffer=0;
public:
 void clear(){vertices.clear();if(vertices.capacity()<2048)vertices.reserve(2048);}
 void quad(float x0,float left0,float right0,float x1,float left1,float right1,QColor color,float opacity,float laneOffset,float across0=0,float across1=0,float half=-1){
  auto vertex=[&](float x,float y,float across){return Vertex{-x,0,-y-laneOffset,float(color.redF()),float(color.greenF()),float(color.blueF()),float(color.alphaF())*opacity,across,half};};
  Vertex a=vertex(x0,left0,-across0),b=vertex(x1,left1,-across1),c=vertex(x1,right1,across1),d=vertex(x0,right0,across0);
  vertices.insert(vertices.end(),{a,b,c,a,c,d});
 }
 void draw(const QMatrix4x4 &matrix){
  if(vertices.empty())return;initializeOpenGLFunctions();
  if(!shader){
   shader=std::make_unique<QOpenGLShaderProgram>();
   const char *v=R"(#version 300 es
precision highp float;
layout(location=0) in vec3 position;layout(location=1) in vec4 tint;layout(location=2) in vec2 edge;
uniform mat4 matrix;out vec4 color;out vec2 coverage;
void main(){gl_Position=matrix*vec4(position,1);color=tint;coverage=edge;})";
   const char *f=R"(#version 300 es
precision highp float;
in vec4 color;in vec2 coverage;out vec4 fragmentColor;
void main(){float a=coverage.y<0.?1.:clamp((coverage.y-abs(coverage.x))/max(fwidth(coverage.x),.00001)+.5,0.,1.);fragmentColor=vec4(color.rgb,color.a*a);})";
   if(!shader->addShaderFromSourceCode(QOpenGLShader::Vertex,v)||!shader->addShaderFromSourceCode(QOpenGLShader::Fragment,f)||!shader->link())qFatal("Road geometry shader failed");
   vao.create();vao.bind();glGenBuffers(1,&buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);
   glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
   glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(3*sizeof(float)));
   glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(7*sizeof(float)));vao.release();
  }
  glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
  shader->bind();shader->setUniformValue("matrix",matrix);vao.bind();glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);glDrawArrays(GL_TRIANGLES,0,int(vertices.size()));vao.release();glBindBuffer(GL_ARRAY_BUFFER,0);shader->release();glDisable(GL_BLEND);
 }
};
