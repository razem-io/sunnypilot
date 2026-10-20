import * as THREE from 'three';
import {GLTFLoader} from './vendor/GLTFLoader.js';
const $=s=>document.querySelector(s), host=$('#scene');
const state={speed:72,soc:78,power:18,tire:false,xray:false,labels:true};
let renderer;
try{renderer=new THREE.WebGLRenderer({antialias:true,alpha:true,preserveDrawingBuffer:true});}catch(e){$('#fallback').hidden=false;throw e;}
renderer.setPixelRatio(1);renderer.shadowMap.enabled=true;renderer.shadowMap.type=THREE.PCFSoftShadowMap;renderer.outputColorSpace=THREE.SRGBColorSpace;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=1.6;host.append(renderer.domElement);
const scene=new THREE.Scene(), camera=new THREE.PerspectiveCamera(34,1,.1,100);
scene.add(new THREE.HemisphereLight(0xc5e2f3,0x16212a,2.3));
function light(color,intensity,pos){const l=new THREE.DirectionalLight(color,intensity);l.position.set(...pos);scene.add(l);return l;}
const key=light(0xe4f3ff,4,[-4,7,5]);key.castShadow=true;key.shadow.mapSize.set(1024,1024);key.shadow.camera.left=-4;key.shadow.camera.right=4;key.shadow.camera.top=4;key.shadow.camera.bottom=-4;key.shadow.bias=-.001;
light(0x58bad5,2,[4,3,-5]);light(0xffffff,1.5,[0,5,-1]);
const floor=new THREE.Mesh(new THREE.PlaneGeometry(200,200),new THREE.ShadowMaterial({opacity:.22}));floor.rotation.x=-Math.PI/2;floor.position.y=.02;floor.receiveShadow=true;scene.add(floor);
const ring=new THREE.Mesh(new THREE.RingGeometry(2.8,2.81,100),new THREE.MeshBasicMaterial({color:0x294752,transparent:true,opacity:.4,side:THREE.DoubleSide}));ring.rotation.x=-Math.PI/2;ring.position.y=.025;ring.scale.y=.65;scene.add(ring);
const car=new THREE.Group();scene.add(car);const prototype=new THREE.Group();car.add(prototype);let imported=null;
const mat=(c,m=0,r=.5)=>new THREE.MeshStandardMaterial({color:c,metalness:m,roughness:r});
const paint=mat(0x626d77,.7,.28),glass=mat(0x11202b,.55,.16),black=mat(0x131b23,.12,.7),rubber=mat(0x12171c,0,.95),rim=mat(0xb3bdc5,.85,.23),cyan=mat(0x19b2cd,.5,.22),white=mat(0xe5f6ff,.3,.1),red=mat(0xbb2035,.35,.2);
white.emissive=new THREE.Color(0xb9dfff);white.emissiveIntensity=.5;cyan.emissive=new THREE.Color(0x007b94);cyan.emissiveIntensity=.3;
const shell=[];const wheels=[];
function mesh(g,m,parent=prototype){const o=new THREE.Mesh(g,m);o.castShadow=true;o.receiveShadow=true;parent.add(o);return o;}
function box(x,y,z,w,h,d,m,parent=prototype){const o=mesh(new THREE.BoxGeometry(w,h,d),m,parent);o.position.set(x,y,z);return o;}
function profile(points,width,m,parent=prototype){const shape=new THREE.Shape();points.forEach(([x,y],i)=>i?shape.lineTo(x,y):shape.moveTo(x,y));shape.closePath();const g=new THREE.ExtrudeGeometry(shape,{depth:width,bevelEnabled:true,bevelSegments:3,steps:1,bevelSize:.055,bevelThickness:.055});g.translate(0,0,-width/2);return mesh(g,m,parent);}
function line(points,color=0x263741,parent=prototype){const o=new THREE.Line(new THREE.BufferGeometry().setFromPoints(points.map(p=>new THREE.Vector3(...p))),new THREE.LineBasicMaterial({color}));parent.add(o);return o;}
// Stylized first-generation e-Niro proportions, independent meshes for later replacement.
shell.push(profile([[-2.15,.51],[-2.17,.95],[-1.92,1.13],[-.98,1.2],[-.45,1.54],[1.48,1.44],[2.08,1.12],[2.16,.58],[1.9,.43],[-1.94,.43]],1.74,paint));
shell.push(profile([[-1.02,1.18],[-.47,1.92],[.22,2.02],[1.22,1.93],[1.77,1.34]],1.48,paint));
for(const z of [-.803,.803]){
 shell.push(profile([[-.86,1.29],[-.39,1.84],[.15,1.88],[.15,1.32]],.012,glass));shell.at(-1).position.z=z;
 shell.push(profile([[.27,1.32],[.27,1.88],[1.12,1.82],[1.56,1.33]],.012,glass));shell.at(-1).position.z=z;
 line([[-.9,1.25,z*1.06],[1.61,1.25,z*1.06]],0xadb9be);
 for(const x of [.22,1.25]){line([[x,1.25,z*1.11],[x,.64,z*1.11]],0x354049);box(x-.12,1.2,z*1.12,.19,.035,.035,rim);}
 box(-.71,1.43,z*1.2,.29,.13,.21,paint);box(.2,.53,z*1.13,2.65,.11,.07,black);
 box(.3,.60,z*1.14,2.55,.025,.025,cyan);
 box(.42,2.04,z*.85,1.52,.055,.05,rim);
}
// Front windscreen and rear window as sloping planes.
function panel(vertices,m){const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(vertices.flat(),3));g.setIndex([0,1,2,0,2,3]);g.computeVertexNormals();const o=mesh(g,m.clone());o.material.side=THREE.DoubleSide;shell.push(o);return o;}
panel([[-1.035,1.26,-.68],[-1.035,1.26,.68],[-.47,1.88,.68],[-.47,1.88,-.68]],glass);
panel([[1.27,1.85,-.67],[1.27,1.85,.67],[1.75,1.37,.67],[1.75,1.37,-.67]],glass);
// EV closed front fascia, lower intake and characteristic blue accents.
box(-2.205,.99,0,.025,.23,1.2,black);box(-2.224,1,0,.025,.19,1.1,paint);box(-2.24,1,.31,.025,.16,.27,paint);
box(-2.23,.62,0,.03,.20,1.2,black);box(-2.26,.48,0,.02,.025,1.3,cyan);box(-2.26,.74,0,.02,.025,1.3,cyan);
for(const z of [-.65,.65]){box(-2.26,.61,z,.02,.25,.025,cyan);box(-2.13,1.13,z,.1,.12,.39,white);box(-2.20,.83,z,.03,.04,.30,cyan);box(-2.21,.71,z,.02,.1,.1,white);box(2.16,1.13,z,.035,.14,.38,red);}
// Wheel assemblies with dark aerodynamic inserts and silver spokes.
for(const x of [-1.38,1.36])for(const z of [-.91,.91]){
 const wheel=new THREE.Group();wheel.position.set(x,.46,z);prototype.add(wheel);wheels.push(wheel);
 const tire=mesh(new THREE.CylinderGeometry(.43,.43,.25,40),rubber,wheel);tire.rotation.x=Math.PI/2;
 const face=mesh(new THREE.CylinderGeometry(.32,.32,.265,40),rim,wheel);face.rotation.x=Math.PI/2;
 const inner=mesh(new THREE.CylinderGeometry(.27,.27,.28,40),black,wheel);inner.rotation.x=Math.PI/2;
 for(let i=0;i<5;i++){const angle=i*Math.PI*2/5;const spoke=box(Math.sin(angle)*.13,Math.cos(angle)*.13,Math.sign(z)*.15,.075,.28,.025,rim,wheel);spoke.rotation.z=-angle;}
 const hub=mesh(new THREE.CylinderGeometry(.075,.075,.31,20),rim,wheel);hub.rotation.x=Math.PI/2;
 const arch=mesh(new THREE.TorusGeometry(.47,.045,8,40,Math.PI),black);arch.position.set(x,.46,z);arch.rotation.z=0;
}
const pack=new THREE.Group();car.add(pack);pack.visible=false;
box(0,.46,0,2.25,.18,1.28,mat(0x126775,.65,.25),pack);
for(let x=-.9;x<=.9;x+=.3)for(let z=-.45;z<=.45;z+=.3)box(x,.58,z,.26,.06,.26,cyan,pack);
const energy=new THREE.Group();car.add(energy);energy.visible=false;
for(let i=0;i<12;i++)mesh(new THREE.SphereGeometry(.035,8,8),new THREE.MeshBasicMaterial({color:0x66edff}),energy);
const points=[{name:'VL',pos:[-1.35,.55,1.4],value:2.5},{name:'VR',pos:[-1.35,.55,-1.4],value:2.5},{name:'HL',pos:[1.35,.55,1.4],value:2.6},{name:'HR',pos:[1.35,.55,-1.4],value:2.6}];
points.forEach(p=>{p.el=document.createElement('div');p.el.className='marker';p.el.innerHTML=`<small>${p.name}</small><span>${p.value.toFixed(1).replace('.',',')} bar</span>`;$('#markers').append(p.el);});
let az=-.9,el=.36,distance=8.8,targetAz=az,targetEl=el,drag=null,last=0;
function fitDisplay(){const frame=$('#display-frame'),dashboard=$('#dashboard');const scale=Math.min(frame.clientWidth/2160,frame.clientHeight/1080);dashboard.style.transform=`scale(${scale})`;dashboard.style.left=(frame.clientWidth-2160*scale)/2+'px';dashboard.style.top=(frame.clientHeight-1080*scale)/2+'px';}
new ResizeObserver(fitDisplay).observe($('#display-frame'));fitDisplay();
function resize(){const width=host.clientWidth,height=host.clientHeight;renderer.setSize(width,height,false);camera.aspect=width/height;camera.updateProjectionMatrix();}new ResizeObserver(resize).observe(host);
renderer.domElement.addEventListener('pointerdown',e=>{drag=[e.clientX,e.clientY];renderer.domElement.setPointerCapture(e.pointerId);});
renderer.domElement.addEventListener('pointermove',e=>{if(!drag)return;targetAz-=(e.clientX-drag[0])*.008;targetEl=Math.max(.12,Math.min(1.5,targetEl+(e.clientY-drag[1])*.005));drag=[e.clientX,e.clientY];});
renderer.domElement.addEventListener('pointerup',()=>drag=null);renderer.domElement.addEventListener('pointercancel',()=>drag=null);
renderer.domElement.addEventListener('wheel',e=>{e.preventDefault();distance=Math.max(6,Math.min(13,distance+e.deltaY*.007));},{passive:false});
document.querySelectorAll('[data-view]').forEach(b=>b.onclick=()=>{document.querySelectorAll('[data-view]').forEach(x=>x.classList.toggle('active',x===b));[targetAz,targetEl]=b.dataset.view==='top'?[-Math.PI/2,1.48]:b.dataset.view==='side'?[0,.18]:[-.9,.36];distance=8.8;});
function update(){
 for(const k of ['speed','soc','power']){$('#'+k).textContent=state[k];$('#'+k+'control').value=state[k];$('#'+k+'out').textContent=state[k]+({speed:' km/h',soc:' %',power:' kW'}[k]);}
 $('.ambient b').textContent=state.speed===0?'P':'D';
 $('#range').textContent=Math.round(state.soc*3.64);$('#socfill').style.width=state.soc+'%';$('#powerfill').style.left=(Math.min(0,state.power)+60)/210*100+'%';$('#powerfill').style.width=Math.abs(state.power)/210*100+'%';
 $('#viewlabel').textContent=state.xray?'BATTERIE · SCHEMATISCH':state.tire?'REIFENDRUCK PRÜFEN':'FAHRZEUGSTATUS';$('#pressure-summary').innerHTML=state.tire?'1,8 <small>bar · VL</small>':'2,5–2,6 <small>bar</small>';$('#pressure-summary').style.color=state.tire?'#ffbf70':'';
 points.forEach((p,i)=>{p.el.classList.toggle('warning',state.tire&&i===0);p.el.querySelector('span').textContent=((state.tire&&i===0)?1.8:p.value).toFixed(1).replace('.',',')+' bar';p.el.hidden=!state.labels||state.xray;});
 pack.visible=state.xray;energy.visible=state.xray&&state.power!==0;
 shell.forEach(o=>{o.material.transparent=state.xray;o.material.opacity=state.xray?.16:1;o.material.depthWrite=!state.xray;});
 if(imported)imported.traverse(o=>{if(!o.isMesh)return;o.castShadow=!state.xray;for(const m of (Array.isArray(o.material)?o.material:[o.material])){m.userData.originalAppearance??={opacity:m.opacity,transparent:m.transparent,depthWrite:m.depthWrite};const original=m.userData.originalAppearance;m.opacity=state.xray?.12:original.opacity;m.transparent=state.xray||original.transparent;m.depthWrite=state.xray?false:original.depthWrite;}});
 $('#xray').setAttribute('aria-pressed',state.xray);$('#labels').setAttribute('aria-pressed',state.labels);
}
for(const k of ['speed','soc','power'])$('#'+k+'control').oninput=e=>{state[k]=+e.target.value;document.querySelectorAll('[data-preset]').forEach(x=>x.classList.remove('active'));update();};
$('#xray').onclick=()=>{state.xray=!state.xray;update();};$('#labels').onclick=()=>{state.labels=!state.labels;update();};
document.querySelectorAll('[data-preset]').forEach(b=>b.onclick=()=>{const p=b.dataset.preset;state.speed=p==='park'?0:p==='regen'?54:72;state.power=p==='park'?0:p==='regen'?-32:18;state.tire=p==='tire';document.querySelectorAll('[data-preset]').forEach(x=>x.classList.toggle('active',x===b));update();});
$('#fullscreen').onclick=async()=>{try{if(document.fullscreenElement)await document.exitFullscreen();else await $('#display-frame').requestFullscreen();}catch(e){$('#importstatus').textContent='Vollbild ist in diesem Browserfenster nicht verfügbar.';}};
$('#show-sketchfab').onclick=()=>{if($('#sketchfab-container').children.length)return;const frame=document.createElement('iframe');frame.title='2021 Kia Niro FE Eco Hybrid von Xeeter';frame.src='https://sketchfab.com/models/36999e25cf854948be95459eda809a10/embed?autostart=1';frame.allow='autoplay; fullscreen; xr-spatial-tracking';frame.allowFullscreen=true;$('#sketchfab-container').append(frame);};
function removeImported(){if(!imported)return;car.remove(imported);imported.traverse(o=>{if(o.isMesh){o.geometry.dispose();const materials=Array.isArray(o.material)?o.material:[o.material];materials.forEach(m=>{for(const v of Object.values(m))if(v?.isTexture)v.dispose();m.dispose();});}});imported=null;}
// Blue bumper accents follow the actual mesh surface; original asset remains untouched.
function addBlueTrim(group,model){
 group.updateMatrixWorld(true);
 const trim=new THREE.Group();trim.name='EV blue accent study';group.add(trim);trim.scale.setScalar(1/group.scale.x);
 const ray=new THREE.Raycaster(),blue=new THREE.MeshStandardMaterial({color:0x079bbd,metalness:.45,roughness:.3});
 function strip(yz){
  const pts=yz.map(([y,z])=>{ray.set(new THREE.Vector3(-3,y,z),new THREE.Vector3(1,0,0));const hit=ray.intersectObject(model,true)[0];return new THREE.Vector3(hit?hit.point.x-.015:-2.1,y,z);});
  const curve=new THREE.CatmullRomCurve3(pts);const tube=new THREE.Mesh(new THREE.TubeGeometry(curve,48,.012,8,false),blue);trim.add(tube);
 }
 strip([[.45,-.60],[.46,-.45],[.46,0],[.46,.45],[.45,.60]]);
 strip([[.30,-.57],[.27,-.40],[.27,0],[.27,.40],[.30,.57]]);
 for(const sign of [-1,1])strip([[.59,sign*.65],[.59,sign*.73],[.59,sign*.80]]);
}
function installModel(gltf,name,customize=false){
 removeImported();imported=new THREE.Group();const model=gltf.scene;imported.add(model);
 let bounds=new THREE.Box3().setFromObject(model),size=bounds.getSize(new THREE.Vector3());
 if(size.z>size.x)model.rotation.y=Math.PI/2;
 if(customize)model.rotation.y+=Math.PI;
 bounds.setFromObject(model);size=bounds.getSize(new THREE.Vector3());
 const center=bounds.getCenter(new THREE.Vector3()),scale=4.4/Math.max(size.x,.001);
 model.position.sub(center);model.position.y+=size.y/2;imported.scale.setScalar(scale);
 imported.traverse(o=>{if(!o.isMesh)return;o.castShadow=true;o.receiveShadow=true;
  if(customize)for(const m of (Array.isArray(o.material)?o.material:[o.material]))if(m.name==='Paint_Color'){m.color.set('#343a41');m.metalness=.55;m.roughness=.3;m.clearcoat=.8;m.clearcoatRoughness=.18;}
 });
 car.add(imported);if(customize)addBlueTrim(imported,model);prototype.visible=false;
 $('#modelstatus').textContent=customize?'Niro · 3D-Modell':'Importiertes Modell';
 $('#importstatus').textContent=name+(customize?' · Grauer Lack und blaue Zierleisten. Hybrid-Karosserie als Basis; EV-Front noch nicht nachgebildet.':' · lokal geladen. Ausrichtung und Sensoranker bitte prüfen.');update();
}
let modelRequest=0;
async function loadDefault(){const request=++modelRequest;$('#importstatus').textContent='Niro-Modell wird geladen …';try{const gltf=await new GLTFLoader().loadAsync('./assets/niro-original.glb');if(request!==modelRequest)return;installModel(gltf,'Niro von Xeeter',true);}catch(e){$('#importstatus').textContent='Modell konnte nicht geladen werden. Die Formstudie bleibt verfügbar: '+e.message;}}
$('#model-file').onchange=async e=>{const f=e.target.files[0];if(!f)return;if(f.size>100*1024*1024){$('#importstatus').textContent='Bitte eine optimierte GLB unter 100 MB verwenden.';return;}const request=++modelRequest;$('#importstatus').textContent='Modell wird geladen …';try{const gltf=await new GLTFLoader().parseAsync(await f.arrayBuffer(),'');if(request===modelRequest)installModel(gltf,f.name);}catch(err){$('#importstatus').textContent='GLB konnte nicht geladen werden: '+err.message;}e.target.value='';};
$('#load-niro').onclick=loadDefault;
loadDefault();
$('#resetmodel').onclick=()=>{++modelRequest;removeImported();prototype.visible=true;$('#modelstatus').textContent='e-Niro · Formstudie';$('#importstatus').textContent='Lokale Formstudie wiederhergestellt.';update();};
$('#rotate-model').onclick=()=>{if(imported)imported.rotation.y+=Math.PI/2;};
function render(t){requestAnimationFrame(render);if(document.hidden||t-last<32)return;const dt=Math.min((t-last)/1000,.1);last=t;az+=(targetAz-az)*.12;el+=(targetEl-el)*.12;camera.position.set(Math.sin(az)*Math.cos(el)*distance,.8+Math.sin(el)*distance,Math.cos(az)*Math.cos(el)*distance);camera.lookAt(0,.85,0);
 const w=host.clientWidth,h=host.clientHeight;points.forEach(p=>{const v=new THREE.Vector3(...p.pos).applyMatrix4(car.matrixWorld).project(camera);p.el.style.left=(v.x*.5+.5)*w+'px';p.el.style.top=(-v.y*.5+.5)*h-18+'px';});
 // Only energy flow moves; a fixed body pose avoids distraction in the driving concept.
 energy.children.forEach((o,i)=>{let phase=(t*.00035+i/12)%1;if(state.power<0)phase=1-phase;o.position.set(-1.45+phase*1.45,.72,0);});
 renderer.render(scene,camera);window.dashboardDebug={webgl:true,drawCalls:renderer.info.render.calls,triangles:renderer.info.render.triangles,state:{...state}};
}window.inspectScene={scene,camera,renderer,car,prototype};update();resize();requestAnimationFrame(render);
