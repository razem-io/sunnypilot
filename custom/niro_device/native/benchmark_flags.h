#pragma once
#include <QFile>
#include <QDateTime>
inline bool niroBenchmarkLeaseValid(){
 QFile lease("/dev/shm/niro-performance-bench");if(!lease.open(QIODevice::ReadOnly))return false;
 bool ok=false;const qint64 expiry=QString::fromUtf8(lease.read(64)).trimmed().toLongLong(&ok);
 const qint64 now=QDateTime::currentSecsSinceEpoch();return ok&&expiry>now&&expiry-now<=900;
}
inline bool niroBenchmarkFlag(const char *name){
 static const bool leased=niroBenchmarkLeaseValid();
 return leased&&QFile::exists(QString("/data/local/niro-device-candidate/")+name);
}

// Production defaults are compiled in. A bounded benchmark lease instead uses
// explicit flags, so A/B tests and tiny models never change the saved profile.
inline bool niroRenderOption(const char *name,bool normalDefault){
 static const bool benchmark=niroBenchmarkLeaseValid();
 return benchmark&&!niroBenchmarkFlag("poc-production")?niroBenchmarkFlag(name):normalDefault;
}
