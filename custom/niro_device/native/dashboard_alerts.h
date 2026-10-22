#pragma once
#include "benchmark_flags.h"
#include "selfdrive/ui/qt/onroad/alerts.h"

// Use the original alert selection, including missing/selfdrive-timeout alerts.
// An empty overlay must not repaint the entire dashboard at every UI update.
class NiroDashboardAlerts final : public OnroadAlerts {
 qint64 benchmarkUntil=0;
public:
 explicit NiroDashboardAlerts(QWidget *parent=nullptr):OnroadAlerts(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  hide();
 }
 void showBenchmarkAlert(){if(niroBenchmarkFlag("poc-ui-check"))benchmarkUntil=QDateTime::currentMSecsSinceEpoch()+2200;}
 void syncState(const UIState &s) {
  const Alert next=benchmarkUntil>QDateTime::currentMSecsSinceEpoch()?Alert{"DISPLAY TEST · WARNUNG","Nur ein visueller Test", "niroDisplayTest",cereal::SelfdriveState::AlertSize::FULL,cereal::SelfdriveState::AlertStatus::CRITICAL}:s.scene.started?getAlert(*s.sm,s.scene.started_frame):Alert{};
  const bool changed=!alert.equal(next)||alert.size!=next.size||alert.status!=next.status;
  alert=next;
  if(alert.size==cereal::SelfdriveState::AlertSize::NONE) {
   if(!isHidden())hide();
  } else if(isHidden()) {
   show();raise();
  } else if(changed) {
   update();
  }
 }
};
