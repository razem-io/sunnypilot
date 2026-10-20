from pathlib import Path
import ast
root=Path(__file__).resolve().parents[1];s=root/'source'
def lists(path):
    found={}
    def value(node):
        if isinstance(node,ast.Name):return found[node.id]
        if isinstance(node,ast.BinOp):return value(node.left)+value(node.right)
        return ast.literal_eval(node)
    for n in ast.parse(path.read_text()).body:
        if isinstance(n,ast.Assign):
            try:found[n.targets[0].id]=value(n.value)
            except Exception:pass
    return found
base=lists(s/'selfdrive/ui/SConscript');sp=lists(s/'selfdrive/ui/sunnypilot/SConscript')
# Base assignments containing SP variables are evaluated with these known lists.
env={**sp,'sp_widgets_src':sp['sp_widgets_src'],'sp_qt_src':sp['sp_qt_src']}
for n in ast.parse((s/'selfdrive/ui/SConscript').read_text()).body:
    if isinstance(n,ast.Assign) and isinstance(n.targets[0],ast.Name) and n.targets[0].id in ('widgets_src','qt_src'):
        env[n.targets[0].id]=eval(compile(ast.Expression(n.value),'sources','eval'),{'__builtins__':{}},env)
files=env['widgets_src']+env['qt_src']+['qt/api.cc','qt/util.cc']+sp['sp_qt_util']
files=[str(s/f[1:]) if f.startswith('#') else str(s/'selfdrive/ui'/f) for f in files]
(root/'native/sources.cmake').write_text('set(NATIVE_UI_SOURCES\n'+ '\n'.join(files)+'\n)\n')
