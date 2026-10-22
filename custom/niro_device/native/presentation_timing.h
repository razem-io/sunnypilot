#pragma once
/* Protocol attribution (Weston presentation_timing.xml):
Copyright © 2013-2014 Collabora, Ltd.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice (including the next
    paragraph) shall be included in all copies or substantial portions of the
    Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/
#include <QGuiApplication>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>
#include <vector>
#include <set>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <functional>
// Legacy Weston v1 presentation protocol, as shipped on AGNOS.
// Wire signatures follow Weston 1.9 protocol/presentation_timing.xml.
// Does not commit or alter the surface. Optional benchmark callback can request
// the next repaint; without it this is strictly an observer.
namespace niro_present {
inline const wl_interface *syncTypes[]={&wl_output_interface};
inline const wl_message feedbackEvents[]={{"sync_output","o",syncTypes},{"presented","uuuuuuu",nullptr},{"discarded","",nullptr}};
inline const wl_interface feedbackInterface={"presentation_feedback",1,0,nullptr,3,feedbackEvents};
inline const wl_interface *requestTypes[]={&wl_surface_interface,&feedbackInterface};
inline const wl_message requests[]={{"destroy","",nullptr},{"feedback","on",requestTypes}};
inline const wl_message events[]={{"clock_id","u",nullptr}};
inline const wl_interface interface={"presentation",1,2,requests,1,events};
}
class NiroPresentationTiming {
 struct Ticket{NiroPresentationTiming *owner;wl_proxy *proxy;};
 wl_registry *registry=nullptr;wl_proxy *presentation=nullptr;bool attempted=false;
 std::function<void()> nextFrame;
 std::function<void(uint64_t)> presentedFrame;
 std::set<Ticket*> pending;uint64_t last=0;std::vector<double> intervals;unsigned discarded=0,duplicates=0;
 static void clockId(void*,wl_proxy*,uint32_t id){fprintf(stderr,"PRESENT clock=%u\n",id);}
 static void global(void *data,wl_registry *registry,uint32_t name,const char *iface,uint32_t){
  auto *self=static_cast<NiroPresentationTiming*>(data);
  if(std::strcmp(iface,"presentation")||self->presentation)return;
  self->presentation=static_cast<wl_proxy*>(wl_registry_bind(registry,name,&niro_present::interface,1));
  static const struct{void(*clock)(void*,wl_proxy*,uint32_t);} listener={clockId};
  wl_proxy_add_listener(self->presentation,reinterpret_cast<void(**)(void)>(const_cast<void*>(static_cast<const void*>(&listener))),self);
 }
 static void removed(void*,wl_registry*,uint32_t){}
 static void sync(void*,wl_proxy*,wl_output*){}
 static void finish(Ticket *ticket){auto *self=ticket->owner;self->pending.erase(ticket);wl_proxy_destroy(ticket->proxy);delete ticket;}
 static void shown(void *data,wl_proxy*,uint32_t hi,uint32_t lo,uint32_t ns,uint32_t refresh,uint32_t,uint32_t,uint32_t flags){
  auto *ticket=static_cast<Ticket*>(data);auto *self=ticket->owner;const uint64_t stamp=((uint64_t(hi)<<32)|lo)*1000000000ULL+ns;
  if(self->last&&stamp>self->last){self->intervals.push_back((stamp-self->last)/1e6);}
  else if(stamp==self->last)self->duplicates++;
  if(stamp>self->last&&self->presentedFrame)self->presentedFrame(stamp);
  self->last=std::max(stamp,self->last);
  if(self->intervals.size()>=120){auto sorted=self->intervals;std::sort(sorted.begin(),sorted.end());double total=0;for(auto dt:sorted)total+=dt;
   fprintf(stderr,"PRESENT fps=%.2f p50=%.3fms p95=%.3fms max=%.3fms discarded=%u duplicates=%u refresh=%.2fHz flags=%u\n",sorted.size()*1000./total,sorted[sorted.size()/2],sorted[sorted.size()*95/100],sorted.back(),self->discarded,self->duplicates,refresh?1e9/refresh:0,flags);
   self->intervals.clear();self->discarded=self->duplicates=0;
  }finish(ticket);if(self->nextFrame)self->nextFrame();
 }
 static void dropped(void *data,wl_proxy*){auto *ticket=static_cast<Ticket*>(data);auto *self=ticket->owner;self->discarded++;finish(ticket);if(self->nextFrame)self->nextFrame();}
public:
 void onPresented(std::function<void(uint64_t)> callback){presentedFrame=std::move(callback);}
 void onFeedback(std::function<void()> callback){nextFrame=std::move(callback);}
 ~NiroPresentationTiming(){for(auto *ticket:pending){wl_proxy_destroy(ticket->proxy);delete ticket;}if(presentation){wl_proxy_marshal(presentation,0);wl_proxy_destroy(presentation);}if(registry)wl_registry_destroy(registry);}
 void request(QWindow *window){
  auto *native=QGuiApplication::platformNativeInterface();if(!native||!window)return;
  if(!attempted){attempted=true;auto *display=static_cast<wl_display*>(native->nativeResourceForIntegration("display"));if(!display)return;registry=wl_display_get_registry(display);static const wl_registry_listener listener={global,removed};wl_registry_add_listener(registry,&listener,this);}
  if(!presentation||pending.size()>=8)return;
  auto *surface=static_cast<wl_surface*>(native->nativeResourceForWindow("surface",window));if(!surface)return;
  auto *proxy=wl_proxy_marshal_constructor(presentation,1,&niro_present::feedbackInterface,surface,nullptr);if(!proxy)return;
  auto *ticket=new Ticket{this,proxy};pending.insert(ticket);
  static const struct{void(*sync)(void*,wl_proxy*,wl_output*);void(*presented)(void*,wl_proxy*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);void(*discarded)(void*,wl_proxy*);} listener={sync,shown,dropped};
  wl_proxy_add_listener(proxy,reinterpret_cast<void(**)(void)>(const_cast<void*>(static_cast<const void*>(&listener))),ticket);
 }
};
