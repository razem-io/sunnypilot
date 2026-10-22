#pragma once
#include <QGuiApplication>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>
#include <functional>
#include <cstdio>
// Request one scene update per compositor frame; never wait in the GUI thread.
class NiroFramePacer {
 wl_callback *pending=nullptr;
 std::function<void()> update;
 static void done(void *data,wl_callback *callback,uint32_t stamp){
  auto *self=static_cast<NiroFramePacer*>(data);
  wl_callback_destroy(callback);self->pending=nullptr;
  if(self->update)self->update();
 }
public:
 ~NiroFramePacer(){if(pending)wl_callback_destroy(pending);}
 void request(QWindow *window,std::function<void()> next){
  if(pending||!window)return;
  auto *native=QGuiApplication::platformNativeInterface();
  auto *surface=static_cast<wl_surface*>(native->nativeResourceForWindow("surface",window));
  if(!surface)return;
  update=std::move(next);pending=wl_surface_frame(surface);
  static const wl_callback_listener listener={done};
  wl_callback_add_listener(pending,&listener,this);
 }
};
