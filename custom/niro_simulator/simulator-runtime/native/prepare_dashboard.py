"""Generate a desktop-only adapter; leave comma deployment source untouched."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root.parent/'sunnypilot/custom/niro_dashboard/dashboard.cpp').read_text()
s=s[:s.index('int main(')]
s=s.replace('#include <qpa/qplatformnativeinterface.h>','#include <QOpenGLVertexArrayObject>\n#include <QProcessEnvironment>')
s=s.replace('#include <wayland-client.h>','')
a=s.index('static void landscape(');b=s.index('struct Mesh',a);s=s[:a]+s[b:]
s=s.replace('QOpenGLShaderProgram shader;','QOpenGLVertexArrayObject vao; QOpenGLShaderProgram shader;')
s=s.replace('public:\n void openDashboard()', 'public:\n std::function<void(bool)> selectView;\n void closeDashboard(){requested=false;permitted=false;if(selectView)selectView(false);}\n std::function<void(QString)> statusChanged;\n void openDashboard()')
s=s.replace('setFixedSize(2160,1080);','setMinimumSize(1080,540);')
s=s.replace('setWindowFlags(Qt::Window|Qt::FramelessWindowHint);setCursor(Qt::BlankCursor);','')
s=s.replace('QString("/dev/shm/niro-dashboard-%1")','QString("/tmp/niro-desktop-dashboard-%1")')
a=s.index('received.start();bridge.setWorkingDirectory');b=s.index('connect(&timer',a)
s=s[:a]+'''received.start();bridge.setWorkingDirectory(qEnvironmentVariable("NIRO_RUNTIME"));bridge.start(qEnvironmentVariable("NIRO_PYTHON"),{qEnvironmentVariable("NIRO_RUNTIME")+"/native/bridge.py"});
 '''+s[b:]
s=s.replace('if(f.open(QIODevice::WriteOnly))f.write(status.toUtf8());','if(f.open(QIODevice::WriteOnly))f.write(status.toUtf8());')
s=s.replace('"/data/local/niro-dashboard/state"','qEnvironmentVariable("NIRO_RUNTIME")+"/native-state"')
s=s.replace('if(!ok){permitted=false;if(frames>0){fprintf(stderr,"VIEW: retiring graphics process for safe handoff\\n");fflush(stderr);bridge.kill();bridge.waitForFinished(100);::_exit(42);}return;}permitted=true;', 'if(statusChanged)statusChanged(data.value("reason").toString()); if(selectView)selectView(ok); if(!ok){permitted=false;return;}permitted=true;')
s=s.replace('landscape(this,true);','show();')
s=s.replace('initializeOpenGLFunctions();','initializeOpenGLFunctions();vao.create();vao.bind();')
s=s.replace('const char *vertex=R"(\n', 'const char *vertex=R"(#version 330 core\n').replace('attribute vec3','in vec3').replace('attribute vec2','in vec2')
s=s.replace('uniform mat4 model; varying vec3 n; varying vec3 world; varying vec2 tex;', 'uniform mat4 model; out vec3 n; out vec3 world; out vec2 tex;')
s=s.replace('const char *fragment=R"(\n   precision highp float;', 'const char *fragment=R"(#version 330 core\n   out vec4 fragmentColor;').replace('varying vec3','in vec3').replace('varying vec2','in vec2').replace('texture2D(','texture(').replace('gl_FragColor','fragmentColor')
s=s.replace('glViewport(0,0,2160,1080);','vao.bind();glViewport(0,0,width()*devicePixelRatioF(),height()*devicePixelRatioF());')
s=s.replace('QPainter p(this);p.setRenderHint', 'vao.release();QPainter p(this);p.scale(width()/2160.0,height()/1080.0);p.setRenderHint')
s=s.replace('bool demo=data.value("parked").toBool();','bool demo=false;')
s=s.replace('data.value("parked").toBool()?"PARKEN":"MANUELL"','"REPLAY · LOKAL"')
s=s.replace('"/data/local/niro-dashboard/preview.png"','qEnvironmentVariable("NIRO_RUNTIME")+"/native-dashboard-preview.png"')
s=s.replace('bridge.kill();bridge.waitForFinished(100);::_exit(0);','if(selectView)selectView(false);return;')
s=s.replace('pressAt(e->localPos())','pressAt(QPointF(e->localPos().x()*2160/width(),e->localPos().y()*1080/height()))')
s=s.replace('moveAt(e->localPos())','moveAt(QPointF(e->localPos().x()*2160/width(),e->localPos().y()*1080/height()))')
(root/'native/dashboard_widget.h').write_text('// Generated desktop adaptation. See prepare_dashboard.py.\n'+s)
