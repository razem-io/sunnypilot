#pragma once
#include <algorithm>
#include <cmath>
namespace niro_solar {
struct Sun {double elevation, azimuth;};
// NOAA fractional-year approximation; UTC, east-positive longitude.
// https://gml.noaa.gov/grad/solcalc/solareqns.PDF
inline Sun position(int day,int days,double hour,double latitude,double longitude){
 constexpr double pi=3.14159265358979323846,rad=pi/180;
 double g=2*pi/days*(day-1+(hour-12)/24);
 double eq=229.18*(.000075+.001868*cos(g)-.032077*sin(g)-.014615*cos(2*g)-.040849*sin(2*g));
 double dec=.006918-.399912*cos(g)+.070257*sin(g)-.006758*cos(2*g)+.000907*sin(2*g)-.002697*cos(3*g)+.00148*sin(3*g);
 double ha=(hour*60+eq+4*longitude)/4*rad-pi,lat=latitude*rad;
 double elevation=asin(std::clamp(sin(lat)*sin(dec)+cos(lat)*cos(dec)*cos(ha),-1.,1.));
 double azimuth=atan2(sin(ha),cos(ha)*sin(lat)-tan(dec)*cos(lat))+pi;
 return {elevation/rad,fmod(azimuth/rad+360,360)};
}
inline double daylight(double elevation){double t=std::clamp((elevation+6)/12.,0.,1.);return t*t*(3-2*t);}
// Mesh coordinates: front -X, up +Y, left +Z. Heading clockwise from north.
inline void direction(Sun s,double heading,float &x,float &y,float &z){
 double a=(s.azimuth-heading)*3.141592653589793/180,e=s.elevation*3.141592653589793/180;
 x=-cos(e)*cos(a);y=sin(e);z=-cos(e)*sin(a);
}
}
