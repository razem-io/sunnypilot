#pragma once
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <vector>
// Calibrated rays meet one image plane to preserve the camera image proportions.
// This is a visual backdrop, not measured scene depth.
class RoadCamera : protected QOpenGLFunctions_3_3_Core {
 QOpenGLShaderProgram shader;QOpenGLVertexArrayObject vao;GLuint buffer=0;int vertexCount=0;
public:
 static constexpr float imageScale=1.48f;
 void initialize(){initializeOpenGLFunctions();vao.create();vao.bind();
 const char *vs=R"(#version 330 core
 layout(location=0) in vec2 imagePoint;uniform mat4 worldToClip;uniform mat3 roadFromCamera;
 uniform float imageScale;uniform vec2 imageSize;uniform float cameraHeight;uniform float lateralOffset;out vec2 imageUVPoint;
 void main(){
 vec3 ray=roadFromCamera*vec3(imagePoint*imageSize,1.0);
 // One common image plane keeps vehicles and image edges straight.
 // It is a visual backdrop, not a depth-reconstructed road surface.
 float distance=85.0/max(ray.x,.001);
 vec3 ground=ray*distance;float elevation=cameraHeight-ground.z;
 imageUVPoint=imagePoint;
 gl_Position=worldToClip*vec4(-ground.x,elevation,-ground.y-lateralOffset,1.0);
 gl_Position.xy*=imageScale;})";
 const char *fs=R"(#version 330 core
 in vec2 imageUVPoint;uniform sampler2D imageY;uniform sampler2D imageUV;
 uniform float opacity;uniform sampler2D annotations;uniform bool showAnnotations;out vec4 color;
 void main(){vec2 uv=imageUVPoint;
 float radius=length((uv-vec2(.5))/vec2(.69,.47));
 float edge=smoothstep(0.0,.10,uv.x)*smoothstep(0.0,.10,1.0-uv.x);
 float fade=edge*smoothstep(0.0,.16,uv.y)*smoothstep(0.0,.17,1.0-uv.y)*(1.0-smoothstep(.65,1.0,radius));
 float y=texture(imageY,uv).r;vec2 c=texture(imageUV,uv).rg-.5;
 vec3 rgb=vec3(y+1.402*c.y,y-.344*c.x-.714*c.y,y+1.772*c.x);
 if(showAnnotations){vec4 overlay=texture(annotations,uv);rgb=mix(rgb,overlay.rgb,overlay.a);}
 color=vec4(rgb,fade*opacity);})";
 if(!shader.addShaderFromSourceCode(QOpenGLShader::Vertex,vs)||!shader.addShaderFromSourceCode(QOpenGLShader::Fragment,fs)||!shader.link())qFatal("Road camera shader failed");
 std::vector<float> vertices;
 constexpr int n=48;
 for(int y=0;y<n;y++)for(int x=0;x<n;x++){
  float u=float(x)/n,v=float(y)/n,du=1.f/n;
  for(auto p:{QVector2D(u,v),QVector2D(u+du,v),QVector2D(u+du,v+du),QVector2D(u,v),QVector2D(u+du,v+du),QVector2D(u,v+du)}){vertices.push_back(p.x());vertices.push_back(p.y());}
 }
 vertexCount=vertices.size()/2;
 glGenBuffers(1,&buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);vao.release();
 }
 void draw(const QMatrix4x4 &view,const QMatrix3x3 &calibration,float height,float offset,GLuint y,GLuint uv,int width,int rows,float opacity,GLuint overlay=0){
 vao.bind();glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);shader.bind();
 shader.setUniformValue("imageScale",imageScale);shader.setUniformValue("worldToClip",view);shader.setUniformValue("roadFromCamera",calibration);shader.setUniformValue("cameraHeight",height);shader.setUniformValue("lateralOffset",offset);shader.setUniformValue("imageSize",QVector2D(width,rows));shader.setUniformValue("opacity",opacity);shader.setUniformValue("imageY",0);shader.setUniformValue("imageUV",1);
 shader.setUniformValue("annotations",2);shader.setUniformValue("showAnnotations",overlay!=0);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,overlay);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,y);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,uv);glDrawArrays(GL_TRIANGLES,0,vertexCount);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,0);shader.release();vao.release();
 }
};
