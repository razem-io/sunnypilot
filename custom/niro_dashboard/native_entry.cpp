// Native Qt extension for the exact WiFiPromptWidget in sunnypilot 0.10.1.
// Source reference: 737a6c4236e843034680c951005b38d15815363f,
// selfdrive/ui/qt/widgets/wifi.cc. No binary offsets or driving hooks.
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <cstdio>
static void installEntry(){
 auto timer=new QTimer(qApp);auto attempts=new int(0);
 QObject::connect(timer,&QTimer::timeout,qApp,[timer,attempts]{
  for(auto widget:QApplication::allWidgets()){
   if(QByteArray(widget->metaObject()->className())!="WiFiPromptWidget")continue;
   auto labels=widget->findChildren<QLabel*>(QString(),Qt::FindDirectChildrenOnly);
   auto buttons=widget->findChildren<QPushButton*>(QString(),Qt::FindDirectChildrenOnly);
   if(labels.size()!=2||buttons.size()!=1)continue;
   auto button=buttons.front();
   // Change only the entry card; the Firehose settings panel remains available.
   labels[0]->setText("Niro Dashboard");labels[1]->setText("Fahrzeugansicht und Sensordaten öffnen.");
   QObject::disconnect(button,SIGNAL(clicked(bool)),nullptr,nullptr);
   button->setText("Dashboard öffnen");button->setObjectName("niroDashboardEntry");
   QObject::connect(button,&QPushButton::clicked,widget,[]{
    if(qEnvironmentVariableIsSet("NIRO_ENTRY_TEST")){QFile f("/tmp/niro-entry-clicked");if(f.open(QIODevice::WriteOnly))f.write("clicked");return;}
    QProcess::startDetached("/data/openpilot/custom/niro_dashboard/open-dashboard.sh",{});
   });
   fprintf(stderr,"NIRO_ENTRY: native Firehose card replaced\n");fflush(stderr);
   if(!qEnvironmentVariableIsSet("NIRO_ENTRY_TEST"))QTimer::singleShot(1500,widget,[widget]{widget->window()->grab().save("/data/local/niro-dashboard/native-entry.png");});
   timer->stop();timer->deleteLater();delete attempts;return;
  }
  if(++*attempts>=20){fprintf(stderr,"NIRO_ENTRY: matching card not found; original UI unchanged\n");timer->stop();timer->deleteLater();delete attempts;}
 });timer->start(250);
}
Q_COREAPP_STARTUP_FUNCTION(installEntry)
