#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>
#include <QFile>
#include <cstdio>
class WiFiPromptWidget:public QFrame{
 Q_OBJECT
public:
 int originalClicks=0;
 WiFiPromptWidget(){auto l=new QVBoxLayout(this);l->addWidget(new QLabel("Firehose Mode"));l->addWidget(new QLabel("Upload training data"));auto b=new QPushButton("Open");l->addWidget(b);connect(b,&QPushButton::clicked,this,[this]{originalClicks++;});}
};
int main(int argc,char**argv){QApplication app(argc,argv);QFile::remove("/tmp/niro-entry-clicked");WiFiPromptWidget widget;QTimer::singleShot(1000,&app,[&]{auto button=widget.findChild<QPushButton*>("niroDashboardEntry");if(!button)qFatal("Native card was not replaced");button->click();if(widget.originalClicks||!QFile::exists("/tmp/niro-entry-clicked"))qFatal("Incorrect click routing");printf("PASS: card replaced; click opens dashboard; Firehose action disconnected\n");app.quit();});return app.exec();}
#include "test_entry.moc"
