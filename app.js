import {parseMacro,serializeMacro,normalizeMacro,duration,transitionsAt} from "./macro.js";

const $=id=>document.getElementById(id);
const state={macro:parseMacro("SWIFT1 240\n0 1 0 1\n18 1 0 0\n44 1 0 1\n47 1 0 0\n84 1 1 1\n96 1 1 0\n132 1 0 1\n149 1 0 0\n185 1 0 1\n205 1 0 0\n240 1 0 1\n264 1 0 0\n"),playhead:0,playing:false,speed:1,lastFrame:null};
const feedback=(msg,error=false)=>{ $("feedback").textContent=msg.toUpperCase();$("feedback").style.color=error?"#ff8a92":"#78cabe"; };
const total=()=>Math.max(duration(state.macro),0.01);
const fmt=t=>{const minutes=Math.floor(t/60); const seconds=Math.floor(t%60);const ms=Math.floor((t%1)*1000);return String(minutes).padStart(2,"0")+":"+String(seconds).padStart(2,"0")+"."+String(ms).padStart(3,"0");};
function rerender(){
 const m=state.macro;
 $("eventCount").textContent=m.events.length+" EVENTS";
 $("duration").textContent=duration(m).toFixed(3)+"s";
 $("presses").textContent=m.events.filter(e=>e.down).length;
 $("axisMiddle").textContent=(total()/2).toFixed(2)+"s";
 $("axisEnd").textContent=total().toFixed(2)+"s";
 $("tps").value=m.tps;
 $("eventRows").replaceChildren();
 $("empty").hidden=m.events.length>0;
 // DocumentFragment avoids HTML injection from imported files.
 const fragment=document.createDocumentFragment();
 m.events.forEach((event,i)=>{
  const row=document.createElement("tr");
  const config=[
    ["number",event.tick,1,Number.MAX_SAFE_INTEGER,"tick"],
    ["select",event.player2,null,null,"player2",[[0,"P1"],[1,"P2"]]],
    ["select",event.button,null,null,"button",[[1,"JUMP"],[2,"LEFT"],[3,"RIGHT"]]],
    ["select",event.down,null,null,"down",[[1,"↓ PRESS"],[0,"↑ RELEASE"]]]
  ];
  for(const [type,value,,max,key,options] of config){
   const cell=document.createElement("td");
   let control=document.createElement(type==="select"?"select":"input");
   if(type==="select")for(const [v,label] of options){const o=document.createElement("option");o.value=v;o.textContent=label;control.append(o);}
   else{control.type="number";control.min=0;control.max=String(max);control.step=1;}
   control.value=String(value);
   if(key==="down")control.className=value?"action-down":"action-up";
   control.setAttribute("aria-label",key+" event "+(i+1));
   control.addEventListener("change",()=>{
    const changed={...event,[key]:Number(control.value)};
    try{const events=[...m.events];events[i]=changed;state.macro=normalizeMacro({...m,events});rerender();feedback("EVENT UPDATED");}
    catch(e){rerender();feedback(e.message,true);}
   });
   cell.append(control);row.append(cell);
  }
  const last=document.createElement("td"),del=document.createElement("button");del.textContent="✕";del.setAttribute("aria-label","Delete event "+(i+1));del.onclick=()=>{m.events.splice(i,1);rerender();feedback("EVENT REMOVED");};last.append(del);row.append(last);fragment.append(row);
 });
 $("eventRows").append(fragment);
 draw();refreshPlaybackUI();
}
function draw(){
 const canvas=$("timeline"),width=Math.round(canvas.getBoundingClientRect().width),height=Math.round(canvas.getBoundingClientRect().height);
 if(width<=0||height<=0)return;
 const dpr=Math.min(window.devicePixelRatio||1,2);canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);
 const ctx=canvas.getContext("2d");ctx.scale(dpr,dpr);
 const w=width,h=height,px=t=>18+(w-36)*t/total(),top=24,bottom=h-28;
 ctx.fillStyle="#0d1019";ctx.fillRect(0,0,w,h);
 ctx.strokeStyle="#222938";ctx.lineWidth=1;
 for(let i=0;i<11;i++){let x=18+(w-36)*i/10;ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.stroke();}
 for(let i=0;i<5;i++){let y=top+(bottom-top)*i/4;ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke();}
 const bands=[top+((bottom-top)*.29),top+((bottom-top)*.72)];
 ctx.font="10px ui-monospace,monospace";ctx.fillStyle="#79869d";ctx.fillText("P1",13,bands[0]-19);ctx.fillText("P2",13,bands[1]-19);
 state.macro.events.forEach(e=>{
  const x=px(e.tick/state.macro.tps),y=bands[e.player2];
  ctx.lineWidth=2;ctx.strokeStyle=e.down?"#fc555e":"#61d8cd";ctx.beginPath();ctx.moveTo(x,y-20);ctx.lineTo(x,y+20);ctx.stroke();
  ctx.fillStyle=e.down?"#fc555e":"#61d8cd";ctx.fillRect(x-3,y-3,6,6);
 });
 ctx.strokeStyle="#ffebc0";ctx.lineWidth=1.6;ctx.beginPath();const x=px(state.playhead);ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.stroke();
 ctx.fillStyle="#ffebc0";ctx.beginPath();ctx.moveTo(x-5,0);ctx.lineTo(x+5,0);ctx.lineTo(x,7);ctx.closePath();ctx.fill();
 const held=transitionsAt(state.macro,state.playhead*state.macro.tps);
 ctx.fillStyle="#acb6c8";ctx.fillText("HELD "+[...held].filter(([,v])=>v).length,Math.max(10,w-95),h-9);
}
function refreshPlaybackUI(){
 $("timecode").textContent=fmt(state.playhead)+" / "+fmt(total());
 $("scrubber").value=Math.max(0,Math.min(1000,Math.round(state.playhead/total()*1000)));
 $("play").textContent=state.playing?"Ⅱ":"▶";
 $("play").setAttribute("aria-label",state.playing?"Pause timeline preview":"Play timeline preview");
}
function animate(ms){
 if(state.lastFrame===null)state.lastFrame=ms;
 const dt=Math.max(0,Math.min((ms-state.lastFrame)/1000,0.25));state.lastFrame=ms;
 if(state.playing){
  state.playhead+=dt*state.speed;
  if(state.playhead>=total()){state.playhead=total();state.playing=false;}
  draw();refreshPlaybackUI();
 }
 requestAnimationFrame(animate);
}
$("upload").addEventListener("change",async event=>{
 const file=event.target.files?.[0];if(!file)return;
 try{state.macro=parseMacro(await file.text());state.playing=false;state.playhead=0;$("sessionName").textContent=file.name.toUpperCase().slice(0,32);rerender();feedback("MACRO IMPORTED");}
 catch(e){feedback(e.message,true);}
 event.target.value="";
});
$("export").onclick=()=>{
 try{
  const data=serializeMacro(state.macro),blob=new Blob([data],{type:"text/plain;charset=utf-8"});
  const url=URL.createObjectURL(blob),link=document.createElement("a");link.href=url;link.download="swift-macro.swift";document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);feedback("MACRO EXPORTED");
 }catch(e){feedback(e.message,true);}
};
$("demo").onclick=()=>{state.macro=parseMacro("SWIFT1 240\n0 1 0 1\n18 1 0 0\n44 1 0 1\n47 1 0 0\n84 1 1 1\n96 1 1 0\n132 1 0 1\n149 1 0 0\n185 1 0 1\n205 1 0 0\n240 1 0 1\n264 1 0 0\n");state.playing=false;state.playhead=0;$("sessionName").textContent="DEMO.SWIFT";rerender();feedback("DEMO RESTORED");};
$("add").onclick=()=>{try{const tick=Math.round(state.playhead*state.macro.tps);state.macro=normalizeMacro({...state.macro,events:[...state.macro.events,{tick,button:1,player2:0,down:1}]});rerender();feedback("EVENT ADDED");}catch(e){feedback(e.message,true);}};
$("tps").onchange=event=>{try{state.macro=normalizeMacro({...state.macro,tps:Number(event.target.value)});rerender();feedback("TPS METADATA UPDATED: PHYSICS NOT CONVERTED");}catch(e){rerender();feedback(e.message,true);}};
$("speed").onchange=event=>{state.speed=Number(event.target.value);feedback("PREVIEW SPEED UPDATED");};
$("play").onclick=()=>{if(state.playhead>=total())state.playhead=0;state.playing=!state.playing;refreshPlaybackUI();};
$("reset").onclick=()=>{state.playing=false;state.playhead=0;refreshPlaybackUI();draw();};
$("scrubber").oninput=event=>{state.playhead=Number(event.target.value)/1000*total();draw();refreshPlaybackUI();};
$("timeline").addEventListener("click",event=>{const rect=event.currentTarget.getBoundingClientRect();state.playhead=Math.max(0,Math.min(total(),((event.clientX-rect.left-18)/(rect.width-36))*total()));draw();refreshPlaybackUI();});
window.addEventListener("resize",draw);
rerender();requestAnimationFrame(animate);
