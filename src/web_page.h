// web_page.h — the onboard control page, served straight from flash.
// Kept as a raw PROGMEM string (no SPIFFS, no build step). Mirrors the 15RF
// display: live state + mode/timer controls + CO2/demand + pair.
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Orcon 15RF</title>
<style>
:root{--bg:#10141a;--card:#1a212b;--fg:#e7edf3;--mut:#8b97a7;--acc:#3da9fc;--ok:#2ecc71;--warn:#e74c3c}
*{box-sizing:border-box}body{margin:0;font:15px/1.4 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
.wrap{max-width:520px;margin:0 auto;padding:16px}
h1{font-size:18px;margin:6px 0 14px;display:flex;align-items:center;gap:8px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--warn)}.dot.on{background:var(--ok)}
.card{background:var(--card);border-radius:14px;padding:14px;margin-bottom:14px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
button{font:inherit;color:var(--fg);background:#26303c;border:1px solid #313d4c;border-radius:10px;padding:12px 8px;cursor:pointer}
button:active{transform:scale(.97)}button.sel{background:var(--acc);border-color:var(--acc);color:#04121f;font-weight:600}
button:has(svg.ic){display:flex;flex-direction:column;align-items:center;justify-content:center;gap:5px;line-height:1.15}
.ic{width:22px;height:22px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round;flex:none}
.row{display:flex;justify-content:space-between;padding:6px 2px;border-bottom:1px solid #232c38}
.row:last-child{border:0}.row b{color:var(--mut);font-weight:500}
.metrics{display:grid;grid-template-columns:1fr 1fr;gap:0 16px}
input[type=range]{width:100%}label{color:var(--mut);font-size:13px}
.small{font-size:12px;color:var(--mut)}.flt{color:var(--warn);font-weight:600}
.bar{height:6px;background:#26303c;border-radius:3px;overflow:hidden;margin-top:4px}.bar>i{display:block;height:100%;background:var(--acc)}
.hvflow{fill:none;stroke-width:3.4;stroke-linecap:round;stroke-dasharray:0.5 6;animation:hvm 1.6s linear infinite;filter:drop-shadow(0 0 2.5px rgba(0,0,0,.95))}
@keyframes hvm{to{stroke-dashoffset:-13}}
.imgw{position:relative}.imgw>img{width:100%;display:block;border-radius:11px}
.ov2{position:absolute;inset:0;width:100%;height:100%}
.jn{position:absolute;width:9px;height:9px;border-radius:50%;transform:translate(-50%,-50%);border:1.5px solid #0e1218;box-shadow:0 0 3px rgba(0,0,0,.9);z-index:5}
.jn.s{background:var(--acc)}.jn.e{background:#e9885a}
.duct{fill:none;stroke:rgba(255,255,255,.35);stroke-width:4.5;stroke-linecap:round;stroke-linejoin:round;filter:drop-shadow(0 0 2px rgba(0,0,0,.8))}
.hpin{position:absolute;transform:translate(-50%,-50%);background:rgba(10,14,20,.74);color:#fff;font:600 12px system-ui;padding:2px 8px;border-radius:20px;white-space:nowrap;border:1px solid rgba(255,255,255,.16)}
.hpin small{font-weight:400;opacity:.75;font-size:10px}.hpin.out{color:#9fd0ff}
.hstat{position:absolute;right:9px;top:9px;background:rgba(10,14,20,.74);color:#fff;font:600 11px system-ui;padding:3px 10px;border-radius:20px;border:1px solid rgba(255,255,255,.16)}
.cols{display:grid;grid-template-columns:1fr}
.col{min-width:0}
@media(min-width:760px){.wrap{max-width:900px}.cols{grid-template-columns:1fr 1fr;column-gap:16px;align-items:start}}
</style></head><body><div class="wrap">
<h1><span id="dot" class="dot"></span> Orcon 15RF <span id="mode" class="small"></span></h1>

<div class="cols"><div class="col">
<div class="card imgw" style="padding:0;overflow:hidden">
<img src="house.jpg" alt="house">
<div class="hstat" id="hvStat">&#8211;</div>
<svg class="ov2" viewBox="0 0 100 100" preserveAspectRatio="none">
<defs><marker id="harU" viewBox="0 0 10 10" refX="5" refY="4" markerWidth="4" markerHeight="4" orient="auto"><path d="M1 8L5 2L9 8" fill="none" stroke="context-stroke" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></marker>
<marker id="harD" viewBox="0 0 10 10" refX="5" refY="6" markerWidth="4" markerHeight="4" orient="auto"><path d="M1 2L5 8L9 2" fill="none" stroke="context-stroke" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></marker></defs>
<path class="duct" d="M37,33 L37,82"/><path class="duct" d="M44,82 L44,15"/>
<path class="duct" d="M37,50 L27,50"/><path class="duct" d="M37,50 L46,50"/><path class="duct" d="M37,79 L26,79"/><path class="duct" d="M40,28 L58,28"/>
<path class="duct" d="M44,70 L49,70"/><path class="duct" d="M44,46 L67,46"/><path class="duct" d="M44,78 L61,78"/>
<path class="hvflow" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,50 L27,50"/>
<path class="hvflow" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,50 L46,50"/>
<path class="hvflow" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,79 L26,79"/>
<path class="hvflow" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M40,28 L58,28"/>
<path class="hvflow" stroke="#e9885a" vector-effect="non-scaling-stroke" d="M44,70 L49,70"/>
<path class="hvflow" stroke="#e9885a" vector-effect="non-scaling-stroke" d="M44,46 L67,46"/>
<path class="hvflow" stroke="#e9885a" vector-effect="non-scaling-stroke" d="M44,78 L61,78"/>
<path class="hvflow" id="fl_sup" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,33 L37,82" marker-end="url(#harD)"/>
<path class="hvflow" id="fl_thru" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,20 L37,33" style="opacity:.15"/>
<path class="hvflow" id="fl_byp" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,20 L45,26 L37,33"/>
<path class="hvflow" id="fl_exh" stroke="#e9885a" vector-effect="non-scaling-stroke" d="M44,82 L44,15" marker-end="url(#harU)"/>
</svg>
<div class="jn s" style="left:27%;top:50%"></div>
<div class="jn s" style="left:46%;top:50%"></div>
<div class="jn s" style="left:26%;top:79%"></div>
<div class="jn s" style="left:58%;top:28%"></div>
<div class="jn e" style="left:49%;top:70%"></div>
<div class="jn e" style="left:67%;top:46%"></div>
<div class="jn e" style="left:61%;top:78%"></div>
<div class="hpin out" id="hvOut" style="left:8%;top:16%">&#8211;</div>
<div class="hpin" id="rm_attic" style="left:58%;top:23%;display:none"><span class="v">&#8211;</span> <small>attic</small></div>
<div class="hpin" id="rm_bed1" style="left:27%;top:45%;display:none"><span class="v">&#8211;</span> <small>bed</small></div>
<div class="hpin" id="rm_bath" style="left:70%;top:41%;display:none"><span class="v">&#8211;</span> <small>bath</small></div>
<div class="hpin" id="rm_bed2" style="left:44%;top:45%;display:none"><span class="v">&#8211;</span> <small>bed</small></div>
<div class="hpin" id="rm_kitchen" style="left:47%;top:65%;display:none"><span class="v">&#8211;</span> <small>kitchen</small></div>
<div class="hpin" id="rm_living" style="left:27%;top:74%;display:none"><span class="v">&#8211;</span> <small>living</small></div>
<div class="hpin" id="rm_office" style="left:87%;top:67%;display:none"><span class="v">&#8211;</span> <small>office</small></div>
</div>

<div class="card"><div class="small" style="margin-bottom:8px">Mode</div>
<div class="grid">
<button data-m="away"><svg class="ic" viewBox="0 0 24 24"><path d="M4 12l8-7 8 7"/><path d="M6 10v9h12v-9"/></svg>Away</button><button data-m="auto"><svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M9.3 15l2.7-6 2.7 6"/><path d="M10.1 13.2h3.8"/></svg>Auto</button><button data-m="low"><svg class="ic" viewBox="0 0 24 24"><path d="M3 8h10a2.5 2.5 0 1 0-2.5-2.5"/><path d="M3 12h14a2.5 2.5 0 1 1-2.5 2.5"/><path d="M3 16h7"/></svg>1</button>
<button data-m="medium"><svg class="ic" viewBox="0 0 24 24"><path d="M3 8h10a2.5 2.5 0 1 0-2.5-2.5"/><path d="M3 12h14a2.5 2.5 0 1 1-2.5 2.5"/><path d="M3 16h7"/></svg>2</button><button data-m="high"><svg class="ic" viewBox="0 0 24 24"><path d="M3 8h10a2.5 2.5 0 1 0-2.5-2.5"/><path d="M3 12h14a2.5 2.5 0 1 1-2.5 2.5"/><path d="M3 16h7"/></svg>3</button><button data-m="boost"><svg class="ic" viewBox="0 0 24 24"><path d="M6 13l6-6 6 6"/><path d="M6 18l6-6 6 6"/></svg>Boost</button>
</div></div>

<div class="card"><div class="small" style="margin-bottom:8px">Bypass</div>
<div class="grid">
<button data-bypass="open"><svg class="ic" viewBox="0 0 24 24"><path d="M12 2v20M3.3 7l17.4 10M20.7 7L3.3 17"/></svg>Open</button><button data-bypass="auto"><svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M9.3 15l2.7-6 2.7 6"/><path d="M10.1 13.2h3.8"/></svg>Auto</button><button data-bypass="close"><svg class="ic" viewBox="0 0 24 24"><path d="M12 3c2.5 3.5 4 5.5 4 8a4 4 0 0 1-8 0c0-1.6.8-2.8 1.6-3.6.8 1.4 1.7 1.6 2.4.4z"/></svg>Close</button>
</div>
<div class="row" style="margin-top:8px"><b>Current</b><span id="byp2">–</span></div>
</div>

</div><div class="col">
<div class="card"><div class="metrics">
<div class="row"><b>Supply</b><span><span id="sup">–</span>%</span></div>
<div class="row"><b>Exhaust</b><span><span id="exh">–</span>%</span></div>
<div class="row"><b>Indoor</b><span><span id="ti">–</span>°C</span></div>
<div class="row"><b>Outdoor</b><span><span id="to">–</span>°C</span></div>
<div class="row"><b>Supply T</b><span><span id="ts">–</span>°C</span></div>
<div class="row"><b>Exhaust T</b><span><span id="te">–</span>°C</span></div>
<div class="row"><b>Humidity</b><span><span id="rh">–</span>%</span></div>
<div class="row"><b>CO₂</b><span><span id="co2">–</span>ppm</span></div>
<div class="row"><b>Bypass</b><span id="byp">–</span></div>
<div class="row" id="filtrow"><b>Filter</b><span><span id="flt">–</span>%</span></div>
</div>
<div id="fault" class="flt" style="display:none;margin-top:8px">⚠ Fan reports a fault</div>
</div>

<div class="card"><div class="small" style="margin-bottom:8px">AC mode (unbalanced — reconfigures High)</div>
<div class="grid">
<button data-ac="exhaust">Exhaust out<br><span class="small">in 0 / out 100</span></button>
<button data-ac="supply">Supply in<br><span class="small">in 100 / out 0</span></button>
<button data-ac="off">Balanced<br><span class="small">50 / 50</span></button>
</div>
<div class="small" style="margin:10px 0 4px;color:var(--fg)">Custom (e.g. quiet night cooling)</div>
<label>Supply in: <span id="acsv">20</span>%</label>
<input id="acs" type="range" min="0" max="100" step="5" value="20">
<label style="margin-top:4px;display:block">Exhaust out: <span id="acev">40</span>%</label>
<input id="ace" type="range" min="0" max="100" step="5" value="40">
<label style="margin-top:6px;display:flex;align-items:center;gap:8px;font-size:13px"><input type="checkbox" id="acb" checked> Open bypass (cool outside air)</label>
<button style="width:100%;margin-top:8px" data-accustom>Apply custom</button>
<div class="small" style="margin-top:6px">One tap: sets the High preset's fans, opens the bypass, and switches to High. "Balanced" restores 50/50 + auto bypass. Remembered across power loss. (Your unit may enforce a minimum fan speed, so 0% can come out as ~10–15%.)</div>
</div>

<div class="card"><div class="small" style="margin-bottom:8px">Timed boost (high)</div>
<div class="grid">
<button data-t="15"><svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7.5V12l3 1.8"/></svg>15 min</button><button data-t="30"><svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7.5V12l3 1.8"/></svg>30 min</button><button data-t="60"><svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7.5V12l3 1.8"/></svg>60 min</button>
</div></div>

</div></div>
<button id="setBtn" style="width:100%;margin-bottom:14px;background:#1a212b;border:1px solid #313d4c">⚙ Settings &amp; advanced</button>
<div id="adv" style="display:none">

<div class="card"><div class="small" style="margin-bottom:8px">Pairing</div>
<div class="row"><b>Fan link</b><span id="pairLink">–</span></div>
<button style="width:100%;margin-top:10px" data-pair>Pair with my Orcon</button>
<div id="pairMsg" class="small" style="margin-top:8px">Power-cycle your Orcon, then tap Pair within 3 minutes.</div>
</div>

<div class="card"><div class="small" style="margin-bottom:8px">Room temperatures (Domoticz IDX)</div>
<div class="small" style="margin-bottom:8px;color:var(--mut)">Enter the Domoticz IDX of a temperature sensor per room. Each label appears on the airflow house once a value arrives over MQTT; leave 0 to hide a room.</div>
<div class="metrics">
<div class="row"><b>Attic</b><input type="number" min="0" id="ri0" style="width:120px;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px;padding:6px"></div>
<div class="row"><b>Bedroom 1</b><input type="number" min="0" id="ri1" style="width:120px;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px;padding:6px"></div>
<div class="row"><b>Bathroom</b><input type="number" min="0" id="ri2" style="width:120px;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px;padding:6px"></div>
<div class="row"><b>Bedroom 2</b><input type="number" min="0" id="ri3" style="width:120px;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px;padding:6px"></div>
<div class="row"><b>Kitchen</b><input type="number" min="0" id="ri4" style="width:120px;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px;padding:6px"></div>
<div class="row"><b>Living</b><input type="number" min="0" id="ri5" style="width:120px;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px;padding:6px"></div>
<div class="row"><b>Office</b><input type="number" min="0" id="ri6" style="width:120px;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px;padding:6px"></div>
</div>
<button id="riSave" style="width:100%;margin-top:10px">Save room IDs</button>
<div id="riMsg" class="small" style="margin-top:6px"></div>
<div id="domdiag" class="small" style="margin-top:6px;color:var(--mut)">&#8211;</div>
</div>

<div class="card"><div class="small" style="margin-bottom:8px">Settings keeper (survive Orcon power loss)</div>
<label style="display:flex;align-items:center;gap:8px;color:var(--fg);font-size:14px"><input type="checkbox" id="ar"> Auto-restore after the unit reboots</label>
<div id="svd" class="small" style="margin-top:8px">No settings saved yet.</div>
<div class="grid" style="grid-template-columns:1fr 1fr;margin-top:8px">
<button data-reapply>Re-apply saved now</button><button data-forget>Forget saved</button>
</div>
<div class="small" style="margin-top:6px">Every fan speed / bypass you set above is remembered here. With auto-restore on, the ESP re-pushes them ~20&nbsp;s after it sees the fan power-cycle (042F).</div>
</div>

<div class="card">
<div class="row"><b>Fan link</b><span id="fan">unknown</span></div>
<div class="row"><b>RSSI</b><span><span id="rssi">–</span> dBm</span></div>
<div class="row"><b>Radio</b><span id="radio">–</span></div>
<div class="row"><b>CC1101</b><span id="cc">–</span></div><button style="width:100%;margin-top:8px" data-selftest>Start TX self-test (Away&#8644;High, 2 min)</button>
<button style="width:100%;margin-top:8px" data-connect>Connect / RF-check (send RQ 0001)</button>
<button style="width:100%;margin-top:8px" data-status>Read live status (RQ 31DA)</button>
<label style="display:flex;align-items:center;gap:8px;color:var(--mut);font-size:13px;margin-top:10px"><input type="checkbox" id="pl"> Auto-learn fan from any broadcast (advanced — normally use Pair)</label>
<label style="display:flex;align-items:center;gap:8px;color:var(--mut);font-size:13px;margin-top:10px"><input type="checkbox" id="maskids"> Mask device IDs in the UI (privacy / safe screenshots)</label>
<label style="display:flex;align-items:center;gap:8px;color:var(--mut);font-size:13px;margin-top:10px"><input type="checkbox" id="showfilter"> Show filter status row</label>
<div id="ack" style="margin-top:8px;font-weight:600"></div>
<div id="tst" class="small" style="margin-top:8px"></div>
<div class="small" style="margin-top:10px">Send raw frame to fan:</div>
<div style="display:flex;gap:6px;margin-top:4px">
<select id="rty" style="flex:0 0 64px;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px"><option>I</option><option>RQ</option><option>W</option></select>
<input id="rco" placeholder="code 0001" style="flex:1;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px;padding:8px">
</div>
<input id="rpl" placeholder="payload hex e.g 00A000010064" style="width:100%;margin-top:6px;background:#26303c;color:var(--fg);border:1px solid #313d4c;border-radius:8px;padding:8px">
<button style="width:100%;margin-top:6px" data-raw>Send raw</button>
</div>
<p class="small">Power-cycle the HRC400, then press Pair within 3 minutes.</p>
<p class="small"><a href="/wifi" style="color:var(--acc)">WiFi &amp; MQTT setup &rarr;</a></p>
<p class="small"><a href="/log" style="color:var(--acc)">RF log &amp; device ID / clone &rarr;</a></p>
<p class="small"><a href="/debug" style="color:var(--acc)">RF debug &amp; 31DA calibration &rarr;</a></p>
<p class="small"><a href="/update" style="color:var(--acc)">Update firmware (OTA upload) &rarr;</a></p>
</div><!-- /adv -->
</div>
<script>
const $=s=>document.querySelector(s);
function fmtId(id,mask){if(!id)return id||'';if(!mask)return id;const p=(''+id).split(':');if(p.length!==2||p[1].length<4)return id;return p[0]+':'+p[1].slice(0,2)+'••'+p[1].slice(-2);}
$('#setBtn').onclick=()=>{const a=$('#adv'),v=a.style.display==='none';a.style.display=v?'block':'none';$('#setBtn').textContent=v?'⚙ Hide settings':'⚙ Settings & advanced';};
function send(body){return fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})}
function pollSoon(){setTimeout(()=>send({status:1}).then(refresh),3000);}  // re-read the unit a few s after a command
function markMode(m){document.querySelectorAll('[data-m]').forEach(x=>x.classList.toggle('sel',x.dataset.m===m));}
document.querySelectorAll('[data-m]').forEach(b=>b.onclick=()=>{markMode(b.dataset.m);send({mode:b.dataset.m}).then(()=>{refresh();pollSoon();});});
document.querySelectorAll('[data-t]').forEach(b=>b.onclick=()=>send({timer:+b.dataset.t}));
document.querySelectorAll('[data-bypass]').forEach(b=>b.onclick=()=>send({bypass:b.dataset.bypass}));
document.querySelector('[data-status]').onclick=()=>send({status:1});
document.querySelectorAll('[data-ac]').forEach(b=>b.onclick=()=>{if(confirm('AC mode "'+b.dataset.ac+'": reconfigure High and switch to it?'))send({ac_mode:b.dataset.ac}).then(()=>{setTimeout(refresh,600);pollSoon();});});
$('#acs').oninput=e=>$('#acsv').textContent=e.target.value;
$('#ace').oninput=e=>$('#acev').textContent=e.target.value;
document.querySelector('[data-accustom]').onclick=()=>{if(confirm('Set High to supply '+$('#acs').value+'% / exhaust '+$('#ace').value+'% and switch to it?')){markMode('high');send({ac_sup:+$('#acs').value,ac_exh:+$('#ace').value,ac_byp:$('#acb').checked?1:0}).then(()=>{setTimeout(refresh,600);pollSoon();});}};
$('#ar').onchange=e=>send({autorestore:e.target.checked?1:0});
document.querySelector('[data-reapply]').onclick=()=>send({reapply:1});
document.querySelector('[data-forget]').onclick=()=>{if(confirm('Forget all saved settings?'))send({forget:1}).then(()=>setTimeout(refresh,300));};
document.querySelectorAll('[data-pair]').forEach(b=>b.onclick=()=>{send({pair:1}).then(()=>setTimeout(refresh,400));});
if($('#pl'))$('#pl').onchange=e=>send({plearn:e.target.checked?1:0});
if($('#maskids'))$('#maskids').onchange=e=>send({maskids:e.target.checked?1:0}).then(()=>setTimeout(refresh,300));
if($('#showfilter'))$('#showfilter').onchange=e=>send({showfilter:e.target.checked?1:0}).then(()=>setTimeout(refresh,300));
if($('#riSave'))$('#riSave').onclick=()=>{var a=[];for(var i=0;i<7;i++){var el=$('#ri'+i);a.push(el&&el.value?+el.value:0);}send({roomidx:a}).then(()=>{if($('#riMsg'))$('#riMsg').textContent='Saved.';setTimeout(refresh,400);});};
document.querySelector('[data-selftest]').onclick=()=>send({selftest:1});
document.querySelector('[data-connect]').onclick=()=>send({connect:1});
document.querySelector('[data-raw]').onclick=()=>send({raw_type:$('#rty').value,raw_code:$('#rco').value.trim(),raw_pl:$('#rpl').value.replace(/\s+/g,'')});
function num(x){return (x===null||x===undefined)?'–':x}
function refresh(){fetch('/api/state').then(r=>r.json()).then(s=>{
 $('#mode').textContent=s.mode||'';
 {var act=(s.cmd_mode&&s.cmd_mode!='unknown')?s.cmd_mode:s.mode;
  document.querySelectorAll('[data-m]').forEach(b=>b.classList.toggle('sel',b.dataset.m===act));}
 $('#sup').textContent=num(s.supply);$('#exh').textContent=num(s.exhaust);
 $('#ti').textContent=num(s.t_indoor);$('#to').textContent=num(s.t_outdoor);
 $('#ts').textContent=num(s.t_supply);$('#te').textContent=num(s.t_exhaust);
 $('#rh').textContent=num(s.humidity);$('#co2').textContent=num(s.co2);
 if($('#hvOut')){var f=x=>(x==null||x==='')?'–':(Math.round(x*10)/10),
   dur=p=>{p=Math.max(0,Math.min(100,p||0));return p<3?'6s':(2.6-p*0.02).toFixed(2)+'s';};
   $('#hvOut').innerHTML=f(s.t_outdoor)+'° <small>outside</small>';
   $('#hvStat').textContent=(s.mode||'–').toUpperCase()+' · bypass '+(s.bypass?'open':'closed');
   if($('#fl_sup'))$('#fl_sup').style.animationDuration=dur(s.supply);
   if($('#fl_exh'))$('#fl_exh').style.animationDuration=dur(s.exhaust);
   if($('#fl_thru'))$('#fl_thru').style.opacity=s.bypass?'.15':'1';
   if($('#fl_byp'))$('#fl_byp').style.opacity=s.bypass?'1':'.15';
   var rm=s.rooms||{};['attic','bed1','bath','bed2','kitchen','living','office'].forEach(function(k){var el=document.getElementById('rm_'+k);if(!el)return;if(rm[k]!=null){el.style.display='';el.querySelector('.v').textContent=f(rm[k])+'°';}else{el.style.display='none';}});}
 $('#byp').textContent=s.bypass?'open':'closed';if($('#byp2'))$('#byp2').textContent=s.bypass?'open':'closed';
  if($('#ar'))$('#ar').checked=!!s.autorestore;
  if($('#svd')&&s.saved){const nm=['Low sup','Low exh','Med sup','Med exh','High sup','High exh','Boost'];
    let t=s.saved.map((v,i)=>v>=0?nm[i]+' '+v+'%':null).filter(Boolean);
    if(s.byp_saved>=0)t.push('Bypass '+(s.byp_saved==200?'open':s.byp_saved==0?'close':'auto'));
    $('#svd').textContent=t.length?('Saved: '+t.join(', ')):'No settings saved yet.';}
  if(!window.acInit&&s.saved){window.acInit=1;   // seed the custom sliders from the saved High preset
    if(s.saved[4]>=0){$('#acs').value=s.saved[4];$('#acsv').textContent=s.saved[4];}
    if(s.saved[5]>=0){$('#ace').value=s.saved[5];$('#acev').textContent=s.saved[5];}}
 $('#flt').textContent=(s.filter===255?'–':num(s.filter));
 $('#fault').style.display=s.fault?'block':'none';
 $('#fan').textContent=s.fan_known?fmtId(s.fan_id,s.mask_ids):'unknown';
 if($('#pairLink')){
   $('#pairLink').textContent=s.fan_known?fmtId(s.fan_id,s.mask_ids):'not paired';
   var pm=$('#pairMsg');
   if(s.pair==='searching')pm.innerHTML='🔍 Searching… power-cycle your Orcon to put it in pairing mode. '+(s.pair_left||0)+'s left';
   else if(s.pair==='paired')pm.innerHTML='<span style="color:var(--ok)">✓ Paired with '+fmtId(s.fan_id,s.mask_ids)+'</span>';
   else if(s.pair==='timeout')pm.innerHTML='<span style="color:var(--warn)">No fan found.</span> Power-cycle the Orcon and tap Pair again.';
   else pm.textContent='Power-cycle your Orcon, then tap Pair within 3 minutes.';
 }
 if($('#pl'))$('#pl').checked=!!s.plearn;
 if($('#maskids'))$('#maskids').checked=!!s.mask_ids;
 if($('#showfilter'))$('#showfilter').checked=!!s.show_filter;
 if(!window.riInit&&s.room_idx){window.riInit=1;for(var ri=0;ri<7;ri++){var rel=$('#ri'+ri);if(rel)rel.value=s.room_idx[ri]||'';}}
 if($('#domdiag'))$('#domdiag').textContent='domoticz/out: '+(s.dom_rx||0)+' msgs'+(s.dom_last?(' · last idx '+s.dom_last):'');
 if($('#filtrow'))$('#filtrow').style.display=(s.show_filter===false)?'none':'';
 $('#rssi').textContent=num(s.rssi);$('#radio').textContent=s.radio||'';
 ('cc1101' in s)?($('#cc').textContent=(s.cc1101===0||s.cc1101===255)?('not found ('+s.cc1101+')'):('0x'+s.cc1101.toString(16)+' OK')):($('#cc').textContent='n/a');
 $('#dot').className='dot'+(s.online?' on':'');
 document.querySelectorAll('[data-m]').forEach(b=>b.classList.toggle('sel',b.dataset.m===s.mode));
 const fa=s.fan_age;
 let t='Fan reports <b>'+(s.mode||'?')+'</b>'+(fa>=0?' · heard '+fa+'s ago':' · not heard yet');
 if(s.selftest)t='⏳ SELF-TEST: commanding <b>'+s.cmd_mode+'</b> &rarr; fan reports <b>'+(s.mode||'?')+'</b>'+(fa>=0?' ('+fa+'s)':'');
 $('#tst').innerHTML=t;
 $('#ack').innerHTML=s.tx_ack?'<span style="color:var(--ok)">&#10003; UNIT REPLIED TO US</span>':'';
})}
refresh();setInterval(refresh,2000);
</script></body></html>)HTML";

// --- WiFi / MQTT setup portal (served at /wifi) ------------------------------
static const char SETUP_HTML[] PROGMEM = R"SETUP(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Orcon 15RF — Setup</title>
<style>
:root{--bg:#10141a;--card:#1a212b;--fg:#e7edf3;--mut:#8b97a7;--acc:#3da9fc;--ok:#2ecc71}
*{box-sizing:border-box}body{margin:0;font:15px/1.4 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
.wrap{max-width:480px;margin:0 auto;padding:16px}h1{font-size:18px;margin:6px 0 14px}
.card{background:var(--card);border-radius:14px;padding:14px;margin-bottom:14px}
label{display:block;color:var(--mut);font-size:13px;margin:10px 0 4px}
input,select,button{font:inherit;color:var(--fg);background:#26303c;border:1px solid #313d4c;border-radius:10px;padding:11px;width:100%}
button{cursor:pointer;margin-top:14px}button.go{background:var(--acc);border-color:var(--acc);color:#04121f;font-weight:600}
.row{display:flex;gap:8px}.row>*{flex:1}.small{font-size:12px;color:var(--mut)}
#msg{margin-top:10px}.ok{color:var(--ok)}
</style></head><body><div class="wrap">
<h1>WiFi &amp; MQTT setup</h1>
<div class="card">
<label>Network</label>
<div class="row">
<select id="ssid"><option value="">— scan or type below —</option></select>
<button id="rescan" style="flex:0 0 84px;margin-top:0">Scan</button>
</div>
<label>SSID (or type a hidden one)</label>
<input id="ssidManual" placeholder="leave blank to use selection above">
<label>WiFi password (leave blank to keep current)</label>
<input id="pass" type="password" placeholder="blank = keep current password">
<label><input type="checkbox" id="show" style="width:auto;margin-right:6px">show password</label>
</div>
<div class="card">
<label>MQTT broker host (optional)</label>
<input id="mqtt" placeholder="e.g. 192.168.1.10  (blank = keep current)">
<div id="curcfg" class="small" style="margin-top:6px"></div>
</div>
<button class="go" id="save">Save &amp; reboot</button>
<div id="msg" class="small"></div>
<p class="small"><a href="/" style="color:var(--acc)">&larr; back to controls</a></p>
</div>
<script>
const $=s=>document.querySelector(s);
$('#show').onchange=e=>$('#pass').type=e.target.checked?'text':'password';
fetch('/api/netcfg').then(r=>r.json()).then(c=>{if(c.mqtt)$('#mqtt').value=c.mqtt;if($('#curcfg'))$('#curcfg').textContent='Current: WiFi “'+(c.ssid||'—')+'”, MQTT '+(c.mqtt||'not set');}).catch(()=>{});
function scan(){
 $('#rescan').textContent='…';
 fetch('/api/scan').then(r=>r.json()).then(list=>{
  const sel=$('#ssid');sel.innerHTML='<option value="">— pick a network —</option>';
  list.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
   const o=document.createElement('option');o.value=n.ssid;
   o.textContent=n.ssid+'  ('+n.rssi+' dBm'+(n.lock?' 🔒':'')+')';sel.appendChild(o);
  });$('#rescan').textContent='Scan';
 }).catch(()=>{$('#rescan').textContent='Scan';});
}
$('#rescan').onclick=scan;
$('#save').onclick=()=>{
 const ssid=$('#ssidManual').value.trim()||$('#ssid').value;
 const mqtt=$('#mqtt').value.trim();
 if(!ssid && !mqtt){$('#msg').textContent='Enter a network and/or an MQTT host.';return;}
 $('#msg').textContent='Saving…';
 fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
  body:JSON.stringify({ssid:ssid,pass:$('#pass').value,mqtt:mqtt})})
 .then(r=>r.json()).then(()=>{
  $('#msg').className='ok small';
  $('#msg').innerHTML=ssid?('Saved. Rebooting and joining <b>'+ssid+'</b>.'):'Saved MQTT host. Rebooting — WiFi unchanged.';
 }).catch(()=>{$('#msg').textContent='Save failed — try again.';});
};
scan();
</script></body></html>)SETUP";

// --- RF log + device-ID / clone page (served at /log) ------------------------
static const char LOG_HTML[] PROGMEM = R"LOG(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Orcon 15RF — RF log</title>
<style>
:root{--bg:#10141a;--card:#1a212b;--fg:#e7edf3;--mut:#8b97a7;--acc:#3da9fc;--rx:#2ecc71;--tx:#f5b041}
*{box-sizing:border-box}body{margin:0;font:14px/1.4 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
.wrap{max-width:760px;margin:0 auto;padding:16px}h1{font-size:18px;margin:6px 0 12px}
.card{background:var(--card);border-radius:14px;padding:14px;margin-bottom:14px}
label{display:block;color:var(--mut);font-size:12px;margin:8px 0 4px}
input,button{font:inherit;color:var(--fg);background:#26303c;border:1px solid #313d4c;border-radius:9px;padding:9px}
button{cursor:pointer}button.go{background:var(--acc);border-color:var(--acc);color:#04121f;font-weight:600}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:end}.small{font-size:12px;color:var(--mut)}
pre{background:#0c1016;border-radius:10px;padding:10px;overflow:auto;max-height:60vh;font:12px/1.45 ui-monospace,Menlo,Consolas,monospace;margin:0}
.rx{color:var(--rx)}.tx{color:var(--tx)}.tag{display:inline-block;width:14px}
b{color:var(--fg)}
</style></head><body><div class="wrap">
<h1>RF log &amp; device identity</h1>

<div class="card">
<div class="small">This unit sends as <b id="usid">–</b> &nbsp; → &nbsp; fan <b id="fanid">–</b></div>
<div id="seen" class="small" style="margin-top:8px"></div>
<div class="row" style="margin-top:10px">
<div><label>This device ID (cc:serial)</label><input id="us" placeholder="29:012345" style="width:150px"></div>
<div><label>Fan ID (cc:serial)</label><input id="fan" placeholder="32:178990" style="width:150px"></div>
<button class="go" id="apply" style="margin-top:18px">Apply &amp; save</button>
<button id="clone" style="margin-top:18px">Use detected</button>
</div>
<div id="idmsg" class="small" style="margin-top:8px"></div>
<p class="small">Cloning: press a button on your existing remote — its ID appears under "Detected".
Put that as <b>This device ID</b> and your fan as <b>Fan ID</b>, Apply, and the fan obeys without pairing.</p>
</div>

<div class="card">
<div class="row" style="justify-content:space-between">
<div><span class="rx">■</span> received &nbsp; <span class="tx">■</span> sent</div>
<label class="small"><input type="checkbox" id="auto" checked style="width:auto"> auto-refresh</label>
</div>
<pre id="log">loading…</pre>
</div>
<p class="small"><a href="/" style="color:var(--acc)">&larr; back to controls</a></p>
</div>
<script>
const $=s=>document.querySelector(s);
let seenR='',seenF='';
function st(){fetch('/api/state').then(r=>r.json()).then(s=>{
  $('#usid').textContent=s.us_id;$('#fanid').textContent=s.fan_id;
  if(!$('#us').value)$('#us').value=s.us_id;
  if(!$('#fan').value&&s.fan_id&&!s.fan_id.startsWith('63'))$('#fan').value=s.fan_id;
})}
function fmtAge(t,now){let a=Math.max(0,(now-t)/1000);return a<99?a.toFixed(0)+'s':(a/60).toFixed(0)+'m'}
function load(){fetch('/api/log').then(r=>r.json()).then(j=>{
  $('#seen').innerHTML='Detected: remote <b>'+(seenR||'-')+'</b>, fan <b>'+(seenF||'-')+'</b>';
  const now=Math.max(...j.log.map(e=>e.t),0);
  $('#log').innerHTML=j.log.slice().reverse().map(e=>{
    const c=e.d==='R'?'rx':'tx';const r=e.d==='R'?(' '+e.r+'dBm'):'';
    return '<span class="'+c+'"><span class="tag">'+(e.d==='R'?'←':'→')+'</span>'+
           fmtAge(e.t,now).padStart(4)+r.padStart(7)+'  '+e.s+'</span>';
  }).join('\n')||'(nothing yet)';
})}
$('#clone').onclick=()=>{if(seenR)$('#us').value=seenR;if(seenF)$('#fan').value=seenF;};
$('#apply').onclick=()=>{
  const pa=s=>{const m=(s||'').split(':');return m.length===2?[parseInt(m[0],10),parseInt(m[1],10)]:null};
  const u=pa($('#us').value),f=pa($('#fan').value);
  if(!u){$('#idmsg').textContent='Enter device ID as cc:serial, e.g. 29:012345';return;}
  const body={us_class:u[0],us_serial:u[1]};
  if(f){body.fan_class=f[0];body.fan_serial=f[1];}
  fetch('/api/ids',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
   .then(r=>r.json()).then(()=>{$('#idmsg').textContent='Saved. Now controlling as '+$('#us').value+'.';st();});
};
st();load();setInterval(()=>{if($('#auto').checked)load();},1500);setInterval(st,4000);
</script></body></html>)LOG";

// --- Compact widget for embedding (Domoticz iframe block, etc.) -------------
static const char WIDGET_HTML[] PROGMEM = R"WIDGET(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Orcon</title>
<style>
:root{--bg:#161b22;--fg:#e7edf3;--mut:#8b97a7;--acc:#3da9fc;--ok:#2ecc71;--warn:#e74c3c}
*{box-sizing:border-box}html,body{margin:0}
body{font:14px/1.35 system-ui,sans-serif;background:var(--bg);color:var(--fg);padding:10px}
.hd{display:flex;align-items:center;gap:7px;font-weight:600;margin-bottom:8px}
.dot{width:9px;height:9px;border-radius:50%;background:var(--warn)}.dot.on{background:var(--ok)}
.mode{margin-left:auto;font-size:13px;color:var(--acc);text-transform:capitalize}
.g{display:grid;grid-template-columns:1fr 1fr;gap:2px 14px;margin-bottom:8px}
.g div{display:flex;justify-content:space-between;border-bottom:1px solid #232c38;padding:3px 0}
.g b{color:var(--mut);font-weight:500}
.btns{display:grid;grid-template-columns:repeat(3,1fr);gap:5px;margin-bottom:5px}
button{font:inherit;color:var(--fg);background:#26303c;border:1px solid #313d4c;border-radius:8px;padding:8px 4px;cursor:pointer}
.btns button{display:flex;flex-direction:column;align-items:center;gap:3px;line-height:1.1}
.ic{width:18px;height:18px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round;flex:none}
button:active{transform:scale(.96)}button.sel{background:var(--acc);border-color:var(--acc);color:#04121f;font-weight:600}
.lbl{font-size:11px;color:var(--mut);margin:6px 0 2px}
a{color:var(--mut);font-size:11px;text-decoration:none}
</style></head><body>
<div class="hd"><span id="dot" class="dot"></span>Orcon HRC<span id="mode" class="mode">–</span></div>
<div class="g">
<div><b>Indoor</b><span><span id="ti">–</span>&deg;</span></div>
<div><b>Outdoor</b><span><span id="to">–</span>&deg;</span></div>
<div><b>Supply</b><span><span id="sup">–</span>%</span></div>
<div><b>Exhaust</b><span><span id="exh">–</span>%</span></div>
<div><b>Bypass</b><span id="byp">–</span></div>
<div><b>CO&#8322;</b><span><span id="co2">–</span></span></div>
</div>
<div class="lbl">Mode</div>
<div class="btns">
<button data-m="away"><svg class="ic" viewBox="0 0 24 24"><path d="M4 12l8-7 8 7"/><path d="M6 10v9h12v-9"/></svg>Away</button><button data-m="auto"><svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M9.3 15l2.7-6 2.7 6"/><path d="M10.1 13.2h3.8"/></svg>Auto</button><button data-m="low"><svg class="ic" viewBox="0 0 24 24"><path d="M3 8h10a2.5 2.5 0 1 0-2.5-2.5"/><path d="M3 12h14a2.5 2.5 0 1 1-2.5 2.5"/><path d="M3 16h7"/></svg>1</button>
<button data-m="medium"><svg class="ic" viewBox="0 0 24 24"><path d="M3 8h10a2.5 2.5 0 1 0-2.5-2.5"/><path d="M3 12h14a2.5 2.5 0 1 1-2.5 2.5"/><path d="M3 16h7"/></svg>2</button><button data-m="high"><svg class="ic" viewBox="0 0 24 24"><path d="M3 8h10a2.5 2.5 0 1 0-2.5-2.5"/><path d="M3 12h14a2.5 2.5 0 1 1-2.5 2.5"/><path d="M3 16h7"/></svg>3</button><button data-m="boost"><svg class="ic" viewBox="0 0 24 24"><path d="M6 13l6-6 6 6"/><path d="M6 18l6-6 6 6"/></svg>Boost</button>
</div>
<div class="lbl">Bypass</div>
<div class="btns">
<button data-b="open"><svg class="ic" viewBox="0 0 24 24"><path d="M12 2v20M3.3 7l17.4 10M20.7 7L3.3 17"/></svg>Open</button><button data-b="auto"><svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M9.3 15l2.7-6 2.7 6"/><path d="M10.1 13.2h3.8"/></svg>Auto</button><button data-b="close"><svg class="ic" viewBox="0 0 24 24"><path d="M12 3c2.5 3.5 4 5.5 4 8a4 4 0 0 1-8 0c0-1.6.8-2.8 1.6-3.6.8 1.4 1.7 1.6 2.4.4z"/></svg>Close</button>
</div>
<a href="/" target="_top">full control &rarr;</a>
<script>
const $=s=>document.querySelector(s);
function send(b){return fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)})}
document.querySelectorAll('[data-m]').forEach(b=>b.onclick=()=>send({mode:b.dataset.m}).then(()=>setTimeout(refresh,400)));
document.querySelectorAll('[data-b]').forEach(b=>b.onclick=()=>send({bypass:b.dataset.b}).then(()=>setTimeout(refresh,400)));
function refresh(){fetch('/api/state').then(r=>r.json()).then(s=>{
  $('#dot').className='dot'+(s.online?' on':'');
  $('#mode').textContent=s.mode;
  $('#ti').textContent=s.t_indoor||'–';$('#to').textContent=s.t_outdoor||'–';
  $('#sup').textContent=s.supply;$('#exh').textContent=s.exhaust;
  $('#byp').textContent=s.bypass?'open':'closed';
  $('#co2').textContent=s.co2?s.co2+'ppm':'–';
  document.querySelectorAll('[data-m]').forEach(b=>b.classList.toggle('sel',b.dataset.m===s.mode));
});}
refresh();setInterval(refresh,5000);
</script></body></html>)WIDGET";

// --- /debug : live RF debug + 31DA byte-ruler calibration -------------------
static const char DEBUG_HTML[] PROGMEM = R"DBG(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Orcon RF debug</title>
<style>
:root{--bg:#0e1116;--card:#1a212b;--fg:#e7edf3;--mut:#8b97a7;--acc:#3da9fc;--ok:#2ecc71;--warn:#e74c3c}
*{box-sizing:border-box}body{margin:0;font:13px/1.4 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
.wrap{max-width:780px;margin:0 auto;padding:14px}
h1{font-size:17px;margin:4px 0 12px}h2{font-size:13px;color:var(--mut);margin:16px 0 6px;text-transform:uppercase;letter-spacing:.04em}
.card{background:var(--card);border-radius:12px;padding:12px;margin-bottom:12px}
.kv{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:4px 14px}
.kv div{display:flex;justify-content:space-between;border-bottom:1px solid #232c38;padding:3px 0}
.kv b{color:var(--mut);font-weight:500}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums}
td,th{padding:3px 5px;border-bottom:1px solid #232c38;text-align:left}th{color:var(--mut);font-weight:500}
.mono{font-family:ui-monospace,Menlo,Consolas,monospace}
.ruler td{text-align:center;padding:2px 3px;border:1px solid #232c38}
.ruler .idx{color:var(--mut);font-size:11px}.ruler .use{background:#1f3242;color:var(--acc);font-weight:600}
.hl{color:var(--acc)}.warn{color:var(--warn)}.ok{color:var(--ok)}
a{color:var(--acc);text-decoration:none}.small{color:var(--mut);font-size:12px}
button{font:inherit;color:var(--fg);background:#26303c;border:1px solid #313d4c;border-radius:8px;padding:7px 12px;cursor:pointer}
</style></head><body><div class="wrap">
<h1>Orcon RF debug &amp; calibration</h1>
<p class="small"><a href="/">&larr; control</a> &nbsp;·&nbsp; <a href="/log">full RF log</a> &nbsp;·&nbsp; auto-refresh 3 s &nbsp; <button onclick="tick()">refresh now</button></p>

<div class="card"><h2 style="margin-top:0">Link / radio</h2>
<div class="kv">
<div><b>Radio</b><span id="radio">–</span></div>
<div><b>CC1101</b><span id="cc">–</span></div>
<div><b>RSSI</b><span><span id="rssi">–</span> dBm</span></div>
<div><b>Online</b><span id="online">–</span></div>
<div><b>RX frames</b><span id="rxn">0</span></div>
<div><b>TX frames</b><span id="txn">0</span></div>
<div><b>Fan last seen</b><span id="fanage">–</span></div>
<div><b>TX ack</b><span id="ack">–</span></div>
<div><b>Us id</b><span id="usid">–</span></div>
<div><b>Fan id</b><span id="fanid">–</span></div>
</div>
<div class="small mono" style="margin-top:8px">last RX&nbsp; <span id="lrx" class="hl">–</span></div>
<div class="small mono">last TX&nbsp; <span id="ltx">–</span></div>
</div>

<div class="card"><h2 style="margin-top:0">Frame decoders &amp; byte rulers</h2>
<div id="decoders"><div class="small">waiting for frames…</div></div>
<div class="small" style="margin-top:10px">Blue = bytes this firmware reads. Compare “decoded” to the unit’s own display; if any is wrong, tell me the code, byte index and real value and I’ll fix the offset.</div>
</div>

<div class="card"><h2 style="margin-top:0">Message codes seen (this session)</h2>
<table id="codes"><thead><tr><th>code</th><th>count</th><th>last</th><th>latest payload</th></tr></thead><tbody></tbody></table>
</div>

<script>
const $=s=>document.querySelector(s);
const hb=h=>{let a=[];for(let i=0;i+1<h.length;i+=2)a.push(parseInt(h.substr(i,2),16));return a;};
const t16=(b,i)=>{if(b.length<i+2)return null;let v=(b[i]<<8)|b[i+1];if(v&0x8000)v-=0x10000;if(v==0x7FFF||v==-32768)return null;return v/100;};
let R31='';
const MODES=['away','low','medium','high','auto','?','boost','?'];
const tt=(v,u)=>v==null?'—':v+u;
const DEC={
 '31DA':{n:'fan status',use:[3,4,5,6,7,8,9,10,11,12,13,14,17,19,20],f:b=>[
   ['CO2','3-4',(((b[3]<<8)|b[4])==0x7FFF)?'none':(((b[3]<<8)|b[4])+' ppm')],
   ['Indoor RH','5',(b[5]||0)+'%'],
   ['Outdoor RH','6',(b[6]||0)+'%'],
   ['Exhaust T','7-8',tt(t16(b,7),'°C')],
   ['Supply T','9-10',tt(t16(b,9),'°C')],
   ['Indoor T','11-12',tt(t16(b,11),'°C')],
   ['Outdoor T','13-14',tt(t16(b,13),'°C')],
   ['Bypass','17',(b[17]==0?'closed':(b[17]==0xEF||b[17]==0xFF?'auto':(b[17]/2)+'% open'))],
   ['Exhaust fan','19',((b[19]||0)/2)+'%'],
   ['Supply fan','20',((b[20]||0)/2)+'%']]},
 '12A0':{n:'humidity / indoor',use:[1,2,3],f:b=>[
   ['Humidity','1',(b[1]||0)+'%'],
   ['Indoor T','2-3',tt(t16(b,2),'°C')]]},
 '1298':{n:'CO2',use:[1,2],f:b=>[
   ['CO2','1-2',(((b[1]||0)<<8)|(b[2]||0))+' ppm']]},
 '10D0':{n:'filter',use:[1],f:b=>[
   ['Filter left','1',(b[1]||0)+'%']]},
 '31D9':{n:'fan mode',use:[2],f:b=>[
   ['Mode','2',MODES[b[2]]||('?'+b[2])]]},
 '31E0':{n:'vent demand',use:[6],f:b=>[
   ['Demand','6',(b[6]||0)+'%']]}
};
function ruler(b,use){let idx='<tr>',hex='<tr>';const U={};(use||[]).forEach(i=>U[i]=1);
 b.forEach((v,i)=>{idx+='<td class="idx">'+i+'</td>';hex+='<td class="'+(U[i]?'use':'')+' mono">'+v.toString(16).padStart(2,'0').toUpperCase()+'</td>';});
 return '<table class="ruler">'+idx+'</tr>'+hex+'</tr></table>';}
function renderDecoders(map,now){let html='';
 Object.keys(DEC).forEach(c=>{const e=map[c];if(!e||!e.pl||e.pl.length<2)return;const b=hb(e.pl);
  const rows=DEC[c].f(b).map(r=>'<tr><td>'+r[0]+'</td><td class="mono">'+r[1]+'</td><td class="hl">'+r[2]+'</td></tr>').join('');
  html+='<div style="margin-bottom:16px"><div class="mono"><b class="hl">'+c+'</b> &middot; '+DEC[c].n+' &middot; '+b.length+' bytes &middot; '+Math.round((now-e.t)/1000)+'s ago</div>'
   +'<div style="overflow-x:auto;margin:6px 0">'+ruler(b,DEC[c].use)+'</div>'
   +'<table><thead><tr><th>field</th><th>byte(s)</th><th>decoded</th></tr></thead><tbody>'+rows+'</tbody></table></div>';});
 $('#decoders').innerHTML=html||'<div class="small">waiting for frames… press “Read live status” near the unit.</div>';}
function tick(){
 fetch('/api/state').then(r=>r.json()).then(s=>{
  $('#radio').textContent=s.radio;$('#cc').textContent=s.cc1101||'n/a';
  $('#rssi').textContent=s.rssi;$('#online').innerHTML=s.online?'<span class=ok>yes</span>':'<span class=warn>no</span>';
  $('#rxn').textContent=s.rx_n;$('#txn').textContent=s.tx_n;
  $('#fanage').textContent=(s.fan_age<0?'never':s.fan_age+'s ago');
  $('#ack').innerHTML=s.tx_ack?'<span class=ok>unit replied</span>':'<span class=warn>none</span>';
  $('#usid').textContent=s.us_id;$('#fanid').textContent=s.fan_id;
  $('#lrx').textContent=s.last_rx||'–';$('#ltx').textContent=s.last_tx||'–';
  R31=s.raw31da||'';
 });
 fetch('/api/log').then(r=>r.json()).then(j=>{
  const m={};(j.log||[]).forEach(e=>{const p=e.s.trim().split(/\s+/);const c=p[5],pl=p[7]||'';
   if(!c)return;m[c]=m[c]||{n:0,t:0,pl:''};m[c].n++;m[c].t=e.t;m[c].pl=pl;m[c].d=e.d;});
  const now=Math.max(0,...(j.log||[]).map(e=>e.t));
  const rows=Object.keys(m).sort().map(c=>{const x=m[c];
   return '<tr><td class="mono hl">'+c+'</td><td>'+x.n+'</td><td>'+Math.round((now-x.t)/1000)+'s</td><td class="mono small">'+(x.d=='T'?'→ ':'← ')+x.pl+'</td></tr>';});
  $('#codes').querySelector('tbody').innerHTML=rows.join('')||'<tr><td colspan=4 class=small>no frames logged yet</td></tr>';
  if((!m['31DA']||!m['31DA'].pl)&&R31.length>=4)m['31DA']={pl:R31,t:now};
  renderDecoders(m,now);
 });
}
tick();setInterval(tick,3000);
</script></body></html>)DBG";

// --- /update : browser firmware upload (Update.h, no CLI needed) ------------
static const char UPDATE_HTML[] PROGMEM = R"UPD(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Orcon firmware update</title>
<style>
:root{--bg:#10141a;--card:#1a212b;--fg:#e7edf3;--mut:#8b97a7;--acc:#3da9fc;--ok:#2ecc71;--warn:#e74c3c}
*{box-sizing:border-box}body{margin:0;font:15px/1.45 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
.wrap{max-width:520px;margin:0 auto;padding:16px}h1{font-size:18px;margin:6px 0 14px}
.card{background:var(--card);border-radius:14px;padding:16px;margin-bottom:14px}
input[type=file]{width:100%;color:var(--mut);margin-bottom:12px}
button{width:100%;font:inherit;color:#04121f;background:var(--acc);border:0;border-radius:10px;padding:13px;font-weight:600;cursor:pointer}
.barwrap{height:10px;background:#26303c;border-radius:5px;overflow:hidden;margin-top:14px}
#bar{height:100%;width:0;background:var(--acc);transition:width .2s}
.small{font-size:12.5px;color:var(--mut)}a{color:var(--acc);text-decoration:none}
</style></head><body><div class="wrap">
<h1>Firmware update (OTA)</h1>
<div class="card">
<input type="file" id="fw" accept=".bin">
<button id="go">Flash firmware</button>
<div class="barwrap"><div id="bar"></div></div>
<div id="msg" class="small" style="margin-top:10px">Pick the compiled <span style="color:var(--fg)">firmware.bin</span> and flash. The device reboots into it automatically.</div>
</div>
<div class="card small">
The build output is at<br><span style="color:var(--fg)">.pio/build/heltec_v3_cc1101_ota/firmware.bin</span><br><br>
Settings, WiFi and saved fan presets are kept across an update.<br><br>
<a href="/">&larr; back to control</a>
</div>
<script>
const $=s=>document.getElementById(s);
$('go').onclick=()=>{
 const f=$('fw'); if(!f.files.length){alert('Pick a firmware .bin first');return;}
 const file=f.files[0];
 if(!confirm('Flash '+file.name+' ('+((file.size/1024)|0)+' KB)? The device will reboot.'))return;
 const fd=new FormData(); fd.append('update',file);
 const x=new XMLHttpRequest(); x.open('POST','/update');
 x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);$('bar').style.width=p+'%';$('msg').textContent='Uploading '+p+'%';}};
 x.onload=()=>{let ok=false;try{ok=JSON.parse(x.responseText).ok==1;}catch(e){}
   $('msg').textContent=ok?'Flashed — rebooting, this page reloads in a few seconds…':'Update failed (bad file or out of space).';
   $('bar').style.background=ok?'var(--ok)':'var(--warn)';
   if(ok)setTimeout(()=>location.href='/',9000);};
 x.onerror=()=>{$('msg').textContent='Upload error — connection dropped.';};
 $('msg').textContent='Starting upload…';
 x.send(fd);
};
</script></body></html>)UPD";

// --- /widget.html : standalone live airflow scene (embed in Domoticz/HA) -----
static const char FLOW_HTML[] PROGMEM = R"FLOW(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Orcon airflow</title>
<style>
:root{--fg:#e7edf3;--mut:#8b97a7;--acc:#3da9fc;--exh:#e9885a}
*{box-sizing:border-box}html,body{margin:0;background:transparent}
body{font:14px system-ui,sans-serif;color:var(--fg);padding:6px}
.widget{display:flex;gap:12px;width:100%;max-width:1100px;align-items:stretch;background:transparent}
.left{position:relative;flex:1;min-width:0}
.left>img{width:100%;display:block;border-radius:12px}
.title{position:absolute;left:10px;top:9px;background:rgba(10,14,20,.74);color:#fff;font:600 12px system-ui;padding:3px 10px;border-radius:20px;border:1px solid rgba(255,255,255,.16)}
.info{position:absolute;right:9px;top:9px;display:flex;flex-direction:column;align-items:flex-end;gap:5px}
.ip{background:rgba(10,14,20,.74);color:#fff;font:600 12px system-ui;padding:3px 10px;border-radius:20px;white-space:nowrap;border:1px solid rgba(255,255,255,.16)}
.ip.mode{background:rgba(61,169,252,.92);color:#04121f;border-color:transparent}
.ip small{font-weight:400;opacity:.72;font-size:10px}
.pin{position:absolute;transform:translate(-50%,-50%);background:rgba(10,14,20,.74);color:#fff;font:600 12px system-ui;padding:2px 8px;border-radius:20px;white-space:nowrap;border:1px solid rgba(255,255,255,.16)}
.pin small{font-weight:400;opacity:.75;font-size:10px}.out{color:#9fd0ff}
.jn{position:absolute;width:9px;height:9px;border-radius:50%;transform:translate(-50%,-50%);border:1.5px solid #0e1218;box-shadow:0 0 3px rgba(0,0,0,.9);z-index:5}
.jn.s{background:var(--acc)}.jn.e{background:var(--exh)}
.hvflow{fill:none;stroke-width:3.4;stroke-linecap:round;stroke-dasharray:0.5 6;animation:hvm 1.6s linear infinite;filter:drop-shadow(0 0 2.5px rgba(0,0,0,.95))}
@keyframes hvm{to{stroke-dashoffset:-13}}
.duct{fill:none;stroke:rgba(255,255,255,.35);stroke-width:4.5;stroke-linecap:round;stroke-linejoin:round;filter:drop-shadow(0 0 2px rgba(0,0,0,.8))}
.ov{position:absolute;inset:0;width:100%;height:100%}
.ctrl{width:190px;display:flex;flex-direction:column;justify-content:space-between}
.sec{display:flex;flex-direction:column}
.clbl{font:600 10px system-ui;color:var(--mut);margin:0 0 4px;text-transform:uppercase;letter-spacing:.05em}
.g3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:5px}
.g3 button{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:4px;min-height:56px;font:600 12px system-ui;color:var(--fg);background:#26303c;border:1px solid #313d4c;border-radius:9px;cursor:pointer}
.g3 button:active{transform:scale(.96)}.g3 button.on{background:var(--acc);border-color:var(--acc);color:#04121f}
.bic{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.stepc{display:flex;align-items:center;gap:6px;background:#1a212b;border:1px solid #313d4c;border-radius:9px;padding:7px 9px;font:600 12px system-ui}
.sup{color:#7cc4ff}.exhc{color:#f0a877}.stepc b{margin-left:auto;min-width:26px;text-align:right}
.ctl{background:rgba(255,255,255,.16);border:none;color:#fff;width:24px;height:24px;border-radius:7px;font:700 13px system-ui;cursor:pointer;line-height:1;padding:0}
.ctl:hover{background:rgba(255,255,255,.3)}.ctl:active{transform:scale(.9)}
</style></head><body>
<div class="widget">
<div class="left">
<img src="house.jpg" alt="house">
<div class="title">&#127968; Orcon HRC</div>
<div class="info">
<div class="ip mode" id="w_mode">&#8211;</div>
<div class="ip" id="w_byp">bypass &#8211;</div>
<div class="ip" id="w_rh">Moist &#8211;%</div>
<div class="ip" id="w_co2">CO&#8322; &#8211;</div>
<div class="ip" id="w_flt">filter &#8211;%</div>
</div>
<svg class="ov" viewBox="0 0 100 100" preserveAspectRatio="none">
<defs><marker id="arU" viewBox="0 0 10 10" refX="5" refY="4" markerWidth="4" markerHeight="4" orient="auto"><path d="M1 8L5 2L9 8" fill="none" stroke="context-stroke" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></marker>
<marker id="arD" viewBox="0 0 10 10" refX="5" refY="6" markerWidth="4" markerHeight="4" orient="auto"><path d="M1 2L5 8L9 2" fill="none" stroke="context-stroke" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></marker></defs>
<path class="duct" d="M37,33 L37,82"/><path class="duct" d="M44,82 L44,15"/>
<path class="duct" d="M37,50 L27,50"/><path class="duct" d="M37,50 L46,50"/><path class="duct" d="M37,79 L26,79"/><path class="duct" d="M40,28 L58,28"/>
<path class="duct" d="M44,70 L49,70"/><path class="duct" d="M44,46 L67,46"/><path class="duct" d="M44,78 L61,78"/>
<path class="hvflow" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,50 L27,50"/>
<path class="hvflow" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,50 L46,50"/>
<path class="hvflow" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,79 L26,79"/>
<path class="hvflow" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M40,28 L58,28"/>
<path class="hvflow" stroke="#e9885a" vector-effect="non-scaling-stroke" d="M44,70 L49,70"/>
<path class="hvflow" stroke="#e9885a" vector-effect="non-scaling-stroke" d="M44,46 L67,46"/>
<path class="hvflow" stroke="#e9885a" vector-effect="non-scaling-stroke" d="M44,78 L61,78"/>
<path class="hvflow" id="fl_sup" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,33 L37,82" marker-end="url(#arD)"/>
<path class="hvflow" id="fl_thru" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,20 L37,33" style="opacity:.15"/>
<path class="hvflow" id="fl_byp" stroke="#3da9fc" vector-effect="non-scaling-stroke" d="M37,20 L45,26 L37,33"/>
<path class="hvflow" id="fl_exh" stroke="#e9885a" vector-effect="non-scaling-stroke" d="M44,82 L44,15" marker-end="url(#arU)"/>
</svg>
<div class="jn s" style="left:27%;top:50%"></div>
<div class="jn s" style="left:46%;top:50%"></div>
<div class="jn s" style="left:26%;top:79%"></div>
<div class="jn s" style="left:58%;top:28%"></div>
<div class="jn e" style="left:49%;top:70%"></div>
<div class="jn e" style="left:67%;top:46%"></div>
<div class="jn e" style="left:61%;top:78%"></div>
<div class="pin out" id="t_out" style="left:8%;top:16%">&#8211;</div>
<div class="pin" id="rm_attic" style="left:58%;top:23%;display:none"><span class="v">&#8211;</span> <small>attic</small></div>
<div class="pin" id="rm_bed1" style="left:27%;top:45%;display:none"><span class="v">&#8211;</span> <small>bed</small></div>
<div class="pin" id="rm_bath" style="left:70%;top:41%;display:none"><span class="v">&#8211;</span> <small>bath</small></div>
<div class="pin" id="rm_bed2" style="left:44%;top:45%;display:none"><span class="v">&#8211;</span> <small>bed</small></div>
<div class="pin" id="rm_kitchen" style="left:47%;top:65%;display:none"><span class="v">&#8211;</span> <small>kitchen</small></div>
<div class="pin" id="rm_living" style="left:27%;top:74%;display:none"><span class="v">&#8211;</span> <small>living</small></div>
<div class="pin" id="rm_office" style="left:87%;top:67%;display:none"><span class="v">&#8211;</span> <small>office</small></div>
</div>
<div class="ctrl">
<div class="sec"><div class="clbl">Mode</div>
<div class="g3">
<button data-m="away"><svg class="bic" viewBox="0 0 24 24"><path d="M4 12l8-7 8 7"/><path d="M6 10v9h12v-9"/></svg>Away</button><button data-m="auto"><svg class="bic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M9.3 15l2.7-6 2.7 6"/><path d="M10.1 13.2h3.8"/></svg>Auto</button><button data-m="low"><svg class="bic" viewBox="0 0 24 24"><path d="M3 8h10a2.5 2.5 0 1 0-2.5-2.5"/><path d="M3 12h14a2.5 2.5 0 1 1-2.5 2.5"/><path d="M3 16h7"/></svg>1</button>
<button data-m="medium"><svg class="bic" viewBox="0 0 24 24"><path d="M3 8h10a2.5 2.5 0 1 0-2.5-2.5"/><path d="M3 12h14a2.5 2.5 0 1 1-2.5 2.5"/><path d="M3 16h7"/></svg>2</button><button data-m="high"><svg class="bic" viewBox="0 0 24 24"><path d="M3 8h10a2.5 2.5 0 1 0-2.5-2.5"/><path d="M3 12h14a2.5 2.5 0 1 1-2.5 2.5"/><path d="M3 16h7"/></svg>3</button><button data-m="boost"><svg class="bic" viewBox="0 0 24 24"><path d="M6 13l6-6 6 6"/><path d="M6 18l6-6 6 6"/></svg>Boost</button>
</div></div>
<div class="sec"><div class="clbl">Bypass</div>
<div class="g3">
<button data-b="open"><svg class="bic" viewBox="0 0 24 24"><path d="M12 2v20M3.3 7l17.4 10M20.7 7L3.3 17"/></svg>Open</button><button data-b="auto"><svg class="bic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M9.3 15l2.7-6 2.7 6"/><path d="M10.1 13.2h3.8"/></svg>Auto</button><button data-b="close"><svg class="bic" viewBox="0 0 24 24"><path d="M12 3c2.5 3.5 4 5.5 4 8a4 4 0 0 1-8 0c0-1.6.8-2.8 1.6-3.6.8 1.4 1.7 1.6 2.4.4z"/></svg>Close</button>
</div></div>
<div class="sec"><div class="clbl">Balance</div>
<div class="stepc"><span class="sup">supply</span><button class="ctl" data-w="s" data-step="-5">&#9662;</button><b id="supv">&#8211;</b>%<button class="ctl" data-w="s" data-step="5">&#9652;</button></div>
<div class="stepc" style="margin-top:5px"><span class="exhc">exhaust</span><button class="ctl" data-w="e" data-step="-5">&#9662;</button><b id="exhv">&#8211;</b>%<button class="ctl" data-w="e" data-step="5">&#9652;</button></div></div>
<div class="sec"><div class="clbl">Boost</div>
<div class="g3">
<button data-t="15"><svg class="bic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7.5V12l3 1.8"/></svg>15m</button><button data-t="30"><svg class="bic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7.5V12l3 1.8"/></svg>30m</button><button data-t="60"><svg class="bic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7.5V12l3 1.8"/></svg>60m</button>
</div></div>
</div>
</div>
<script>
const $=s=>document.querySelector(s);
const num=x=>(x==null||x===undefined)?'–':x;
const f=x=>(x==null||x==='')?'–':(Math.round(x*10)/10);
const dur=p=>{p=Math.max(0,Math.min(100,p||0));return p<3?'6s':(2.6-p*0.02).toFixed(2)+'s';};
function send(b){return fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)})}
document.querySelectorAll('[data-m]').forEach(b=>b.onclick=()=>{document.querySelectorAll('[data-m]').forEach(x=>x.classList.remove('on'));b.classList.add('on');send({mode:b.dataset.m}).then(()=>setTimeout(up,500));});
document.querySelectorAll('[data-b]').forEach(b=>b.onclick=()=>send({bypass:b.dataset.b}).then(()=>setTimeout(up,500)));
document.querySelectorAll('[data-t]').forEach(b=>b.onclick=()=>send({timer:+b.dataset.t}));
var acS=50,acE=50,acT=null,curByp=0;
function stepAc(w,d){if(w=='s'){acS=Math.max(0,Math.min(100,acS+d));$('#supv').textContent=acS;}else{acE=Math.max(0,Math.min(100,acE+d));$('#exhv').textContent=acE;}clearTimeout(acT);acT=setTimeout(function(){send({ac_sup:acS,ac_exh:acE,ac_byp:curByp});acT=null;},800);}
document.querySelectorAll('.ctl').forEach(b=>b.onclick=()=>stepAc(b.dataset.w,+b.dataset.step));
const ROOMS=['attic','bed1','bath','bed2','kitchen','living','office'];
function up(){fetch('/api/state').then(r=>r.json()).then(s=>{
 curByp=s.bypass?1:0;
 $('#w_mode').textContent=(s.mode||'–').toUpperCase();
 $('#w_byp').innerHTML='bypass <b>'+(s.bypass?'open':'closed')+'</b>';
 $('#w_rh').innerHTML='Moist <b>'+num(s.humidity)+'%</b>';
 $('#w_co2').innerHTML='CO₂ <b>'+num(s.co2)+'</b> <small>ppm</small>';
 $('#w_flt').innerHTML='filter <b>'+(s.filter===255?'–':num(s.filter))+'%</b>';
 $('#t_out').innerHTML=f(s.t_outdoor)+'° <small>outside</small>';
 if(acT===null){acS=+s.supply||0;acE=+s.exhaust||0;$('#supv').textContent=num(s.supply);$('#exhv').textContent=num(s.exhaust);}
 var m=(s.cmd_mode&&s.cmd_mode!='unknown')?s.cmd_mode:s.mode;
 document.querySelectorAll('[data-m]').forEach(b=>b.classList.toggle('on',b.dataset.m===m));
 var rm=s.rooms||{};
 ROOMS.forEach(function(k){var el=document.getElementById('rm_'+k);if(!el)return;if(rm[k]!=null){el.style.display='';el.querySelector('.v').textContent=f(rm[k])+'°';}else{el.style.display='none';}});
 if($('#fl_sup'))$('#fl_sup').style.animationDuration=dur(s.supply);
 if($('#fl_exh'))$('#fl_exh').style.animationDuration=dur(s.exhaust);
 if($('#fl_thru'))$('#fl_thru').style.opacity=s.bypass?'.15':'1';
 if($('#fl_byp'))$('#fl_byp').style.opacity=s.bypass?'1':'.15';
});}
up();setInterval(up,4000);
</script></body></html>)FLOW";
