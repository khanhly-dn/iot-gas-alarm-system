/*
 * ============================================================
 *   FIRE ALERT SYSTEM -- ESP32 + MQ-2
 *   Web Dashboard | OLED | Buzzer | Telegram Bot
 *   FIX: Dung WebServer thay ESPAsyncWebServer => tranh crash tcp_alloc
 * ============================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>           // << Dung cai nay, KHONG dung ESPAsyncWebServer
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// -- WIFI -----------------------------------------------------
#define WIFI_SSID   "Khanh"
#define WIFI_PASS   "@133057k"

// -- TELEGRAM -------------------------------------------------
#define BOT_TOKEN   "8498800393:AAGET_3CtPYjKuQGpH-jYMN2ySgHQsZCSxs"
#define CHAT_ID     "7689211086"

// -- PINS -----------------------------------------------------
#define PIN_MQ2_AO  34
#define PIN_MQ2_DO  35
#define PIN_LED      2
#define PIN_BUZZER   5

// -- OLED -----------------------------------------------------
#define OLED_W      128
#define OLED_H       64
#define OLED_ADDR   0x3C
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);

// -- THRESHOLD ------------------------------------------------
#define SMOKE_THRESHOLD   1000
#define DANGER_THRESHOLD  2500

// -- OBJECTS --------------------------------------------------
WebServer server(80);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// -- STATE ----------------------------------------------------
int    smokeValue   = 0;
bool   alertActive  = false;
bool   prevAlert    = false;
bool   silenced     = false;
String alertLevel   = "SAFE";
int    maxSmoke     = 0;
unsigned long uptimeStart = 0;

unsigned long lastRead      = 0;
unsigned long lastTelegram  = 0;
unsigned long lastBuzz      = 0;
bool buzzState = false;

const unsigned long READ_INTERVAL     = 500;
const unsigned long TELEGRAM_COOLDOWN = 60000;

// -- LOG ------------------------------------------------------
#define LOG_SIZE 20
struct LogEntry { unsigned long ts; int val; String level; };
LogEntry eventLog[LOG_SIZE];
int logHead  = 0;
int logCount = 0;

void addLog(int val, String level) {
  eventLog[logHead] = { millis(), val, level };
  logHead = (logHead + 1) % LOG_SIZE;
  if (logCount < LOG_SIZE) logCount++;
}

// ============================================================
//   HTML DASHBOARD
// ============================================================
const char INDEX_HTML[] PROGMEM = R"HTMLEOF(<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Fire Alert System</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Rajdhani:wght@500;600;700&family=Share+Tech+Mono&display=swap');
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0a0e1a;color:#c9d6e3;font-family:'Rajdhani',sans-serif;min-height:100vh;padding:16px}
body::before{content:'';position:fixed;inset:0;background-image:linear-gradient(rgba(0,180,216,.04) 1px,transparent 1px),linear-gradient(90deg,rgba(0,180,216,.04) 1px,transparent 1px);background-size:40px 40px;pointer-events:none;z-index:0}
.wrap{position:relative;z-index:1;max-width:920px;margin:0 auto}
header{display:flex;align-items:center;gap:14px;margin-bottom:22px;padding-bottom:18px;border-bottom:1px solid #1e2a3a}
.logo{width:46px;height:46px;background:linear-gradient(135deg,#ff6b35,#ff1744);border-radius:11px;display:flex;align-items:center;justify-content:center;font-size:22px;flex-shrink:0;box-shadow:0 0 18px rgba(255,23,68,.35)}
header h1{font-size:1.5rem;font-weight:700;letter-spacing:2px;text-transform:uppercase;color:#fff}
header p{font-size:.78rem;color:#4a6080;font-family:'Share Tech Mono',monospace;margin-top:2px}
.dot{width:10px;height:10px;background:#00e676;border-radius:50%;margin-left:auto;box-shadow:0 0 8px #00e676;animation:pd 2s ease-in-out infinite}
@keyframes pd{0%,100%{opacity:1}50%{opacity:.3}}
#banner{border-radius:15px;padding:18px 22px;display:flex;align-items:center;gap:15px;margin-bottom:20px;border:1px solid;transition:all .4s;position:relative;overflow:hidden}
#banner::before{content:'';position:absolute;inset:0;background:currentColor;opacity:.07}
#banner.safe{color:#00e676;border-color:rgba(0,230,118,.3);background:rgba(0,230,118,.04)}
#banner.warning{color:#ffd600;border-color:rgba(255,214,0,.3);background:rgba(255,214,0,.04)}
#banner.danger{color:#ff1744;border-color:rgba(255,23,68,.3);background:rgba(255,23,68,.05);animation:df .6s ease-in-out infinite alternate}
@keyframes df{from{background:rgba(255,23,68,.05)}to{background:rgba(255,23,68,.12)}}
.bi{font-size:2.2rem}
.bt{font-size:1.4rem;font-weight:700;letter-spacing:3px;text-transform:uppercase;color:currentColor}
.bs{font-size:.8rem;color:#4a6080;margin-top:3px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:15px;margin-bottom:15px}
@media(max-width:560px){.grid{grid-template-columns:1fr}}
.card{background:#111827;border:1px solid #1e2a3a;border-radius:14px;padding:18px}
.lbl{font-size:.7rem;letter-spacing:2px;text-transform:uppercase;color:#4a6080;margin-bottom:10px;font-family:'Share Tech Mono',monospace}
.num{font-family:'Share Tech Mono',monospace;font-size:2.6rem;font-weight:700;color:#00e676;line-height:1;transition:color .4s}
.unit{font-size:.8rem;color:#4a6080;margin-top:4px}
.bar-t{width:100%;height:9px;background:#1e2a3a;border-radius:99px;margin-top:13px;overflow:hidden}
.bar-f{height:100%;border-radius:99px;width:0;background:#00e676;transition:width .5s,background .5s}
.dbadge{display:inline-block;padding:9px 18px;border-radius:9px;font-family:'Share Tech Mono',monospace;font-size:1.3rem;font-weight:700;letter-spacing:2px;margin-top:9px;transition:all .4s}
.dbadge.clear{background:rgba(0,230,118,.1);color:#00e676;border:1px solid rgba(0,230,118,.3)}
.dbadge.detected{background:rgba(255,23,68,.13);color:#ff1744;border:1px solid rgba(255,23,68,.4)}
.thr{margin-top:12px;font-size:.72rem;color:#4a6080;font-family:'Share Tech Mono',monospace;line-height:2}
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:11px;margin-bottom:15px}
@media(max-width:440px){.stats{grid-template-columns:1fr 1fr}}
.stat{background:#111827;border:1px solid #1e2a3a;border-radius:11px;padding:13px 15px}
.sv{font-family:'Share Tech Mono',monospace;font-size:1.15rem;color:#00b4d8;font-weight:700}
.sl{font-size:.68rem;color:#4a6080;letter-spacing:1px;text-transform:uppercase;margin-top:3px}
.btns{display:flex;gap:10px;margin-bottom:15px;flex-wrap:wrap}
.btn{flex:1;min-width:90px;padding:11px 0;border:none;border-radius:10px;font-family:'Rajdhani',sans-serif;font-size:.92rem;font-weight:700;letter-spacing:1.5px;text-transform:uppercase;cursor:pointer;transition:transform .15s,filter .15s}
.btn:hover{transform:translateY(-2px);filter:brightness(1.15)}
.btn:active{transform:translateY(0)}
.b1{background:linear-gradient(135deg,#ff6b35,#ff1744);color:#fff;box-shadow:0 4px 14px rgba(255,23,68,.3)}
.b2{background:linear-gradient(135deg,#f59e0b,#d97706);color:#fff;box-shadow:0 4px 14px rgba(245,158,11,.25)}
.b3{background:linear-gradient(135deg,#0ea5e9,#0284c7);color:#fff;box-shadow:0 4px 14px rgba(14,165,233,.25)}
#toast{position:fixed;bottom:22px;right:22px;background:#1e2a3a;border:1px solid #00b4d8;color:#00b4d8;padding:11px 18px;border-radius:10px;font-family:'Share Tech Mono',monospace;font-size:.85rem;opacity:0;transform:translateY(10px);transition:all .3s;pointer-events:none;z-index:999}
#toast.show{opacity:1;transform:translateY(0)}
.log-card{background:#111827;border:1px solid #1e2a3a;border-radius:14px;padding:18px;margin-bottom:15px}
#log-body{max-height:190px;overflow-y:auto}
#log-body::-webkit-scrollbar{width:4px}
#log-body::-webkit-scrollbar-track{background:#1e2a3a}
#log-body::-webkit-scrollbar-thumb{background:#4a6080;border-radius:2px}
.lr{display:grid;grid-template-columns:70px 1fr 80px;gap:10px;align-items:center;padding:6px 9px;border-radius:7px;font-family:'Share Tech Mono',monospace;font-size:.74rem;margin-bottom:3px}
.lr:hover{background:rgba(255,255,255,.03)}
.lts{color:#4a6080}.lval{color:#c9d6e3}
.llv{font-weight:700;text-align:right}
.lr.SAFE .llv{color:#00e676}.lr.WARNING .llv{color:#ffd600}.lr.DANGER .llv{color:#ff1744}
.no-log{color:#4a6080;font-size:.8rem;font-family:'Share Tech Mono',monospace;padding:8px}
footer{text-align:center;color:#4a6080;font-size:.7rem;font-family:'Share Tech Mono',monospace;padding-top:15px;border-top:1px solid #1e2a3a;letter-spacing:1px}
</style>
</head>
<body>
<div class="wrap">
<header>
  <div class="logo">&#128293;</div>
  <div>
    <h1>Fire Alert System</h1>
    <p id="ipinfo">ESP32 &middot; MQ-2 &middot; IoT Monitor</p>
  </div>
  <div class="dot" id="dot"></div>
</header>

<div id="banner" class="safe">
  <div class="bi" id="bi">&#9989;</div>
  <div>
    <div class="bt" id="bt">AN TOAN</div>
    <div class="bs" id="bs">He thong hoat dong binh thuong</div>
  </div>
</div>

<div class="grid">
  <div class="card">
    <div class="lbl">&#9632; Nong do khoi / gas (ADC)</div>
    <div class="num" id="smk">0</div>
    <div class="unit">Gia tri 0 - 4095</div>
    <div class="bar-t"><div class="bar-f" id="bf"></div></div>
  </div>
  <div class="card">
    <div class="lbl">&#9632; Cam bien so (DO pin)</div>
    <div class="dbadge clear" id="db">KHONG CO</div>
    <div class="thr">Nguong WARNING : 1000<br>Nguong DANGER&nbsp;&nbsp;: 2500</div>
  </div>
</div>

<div class="stats">
  <div class="stat"><div class="sv" id="up">0m 0s</div><div class="sl">Uptime</div></div>
  <div class="stat"><div class="sv" id="mx">0</div><div class="sl">Max ADC</div></div>
  <div class="stat"><div class="sv" id="rs">--</div><div class="sl">WiFi RSSI</div></div>
</div>

<div class="btns">
  <button class="btn b1" onclick="doTest()">&#9889; Test Alert</button>
  <button class="btn b2" onclick="doSilence()">&#128277; Im Lang</button>
  <button class="btn b3" onclick="doReset()">&#128260; Reset</button>
</div>

<div class="log-card">
  <div class="lbl">&#9632; Nhat ky su kien</div>
  <div id="log-body"><div class="no-log">Chua co su kien...</div></div>
</div>

<footer>ESP32 FIRE ALERT &middot; MQ-2 SENSOR &middot; OLED + TELEGRAM &middot; v2.1</footer>
</div>
<div id="toast"></div>

<script>
var prevLv='';
function toast(msg,col){
  var t=document.getElementById('toast');
  t.textContent=msg;
  t.style.borderColor=col||'#00b4d8';
  t.style.color=col||'#00b4d8';
  t.classList.add('show');
  setTimeout(function(){t.classList.remove('show');},2500);
}
function fmtUp(ms){
  var s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);
  if(h>0)return h+'h '+(m%60)+'m';
  return m+'m '+(s%60)+'s';
}
function applyLevel(lv){
  var bn=document.getElementById('banner');
  var bi=document.getElementById('bi');
  var bt=document.getElementById('bt');
  var bs=document.getElementById('bs');
  bn.className='';
  if(lv==='DANGER'){
    bn.className='danger';
    bi.innerHTML='&#128680;';
    bt.textContent='NGUY HIEM - CHAY!';
    bs.textContent='Thoat khoi khu vuc ngay! Goi 114!';
  }else if(lv==='WARNING'){
    bn.className='warning';
    bi.innerHTML='&#9888;';
    bt.textContent='CANH BAO KHOI / GAS';
    bs.textContent='Phat hien khoi hoac gas - kiem tra ngay!';
  }else{
    bn.className='safe';
    bi.innerHTML='&#9989;';
    bt.textContent='AN TOAN';
    bs.textContent='He thong hoat dong binh thuong';
  }
}
function updateUI(d){
  if(d.level!==prevLv){
    if(d.level==='DANGER') toast('NGUY HIEM! PHAT HIEN CHAY!','#ff1744');
    else if(d.level==='WARNING') toast('Canh bao khoi/gas!','#ffd600');
    else if(prevLv) toast('Da an toan tro lai','#00e676');
    prevLv=d.level;
  }
  applyLevel(d.level);
  var col=d.level==='DANGER'?'#ff1744':d.level==='WARNING'?'#ffd600':'#00e676';
  var smk=document.getElementById('smk');
  smk.textContent=d.smoke;
  smk.style.color=col;
  var pct=Math.min(d.smoke/4095*100,100);
  var bf=document.getElementById('bf');
  bf.style.width=pct+'%';
  bf.style.background=col;
  var db=document.getElementById('db');
  if(d.digital){
    db.textContent='PHAT HIEN!';
    db.className='dbadge detected';
  }else{
    db.textContent='KHONG CO';
    db.className='dbadge clear';
  }
  document.getElementById('up').textContent=fmtUp(d.uptime||0);
  document.getElementById('mx').textContent=d.maxSmoke||0;
  var dot=document.getElementById('dot');
  dot.style.background='#00e676';
  dot.style.boxShadow='0 0 8px #00e676';
  if(d.logs&&d.logs.length>0){
    var html='';
    d.logs.forEach(function(e){
      var s=Math.floor(e.ts/1000),m=Math.floor(s/60);
      var ts=m+'m'+(s%60<10?'0':'')+(s%60)+'s';
      html+='<div class="lr '+e.level+'"><span class="lts">'+ts+'</span><span class="lval">ADC: '+e.val+'</span><span class="llv">'+e.level+'</span></div>';
    });
    document.getElementById('log-body').innerHTML=html;
  }
}
function connErr(){
  var dot=document.getElementById('dot');
  dot.style.background='#ff1744';
  dot.style.boxShadow='0 0 8px #ff1744';
  toast('Mat ket noi ESP32!','#ff1744');
}
function doTest(){fetch('/test').then(function(){toast('Test da kich hoat!','#ff6b35');}).catch(connErr);}
function doSilence(){fetch('/silence').then(function(){toast('Buzzer da tat!','#f59e0b');}).catch(connErr);}
function doReset(){fetch('/reset').then(function(){toast('He thong da reset!','#0ea5e9');prevLv='';}).catch(connErr);}
fetch('/info').then(function(r){return r.json();}).then(function(d){
  document.getElementById('ipinfo').textContent='IP: '+d.ip+' | RSSI: '+d.rssi+' dBm';
  document.getElementById('rs').textContent=d.rssi+' dBm';
}).catch(function(){});
setInterval(function(){
  fetch('/status').then(function(r){return r.json();}).then(updateUI).catch(connErr);
},1000);
</script>
</body>
</html>)HTMLEOF";

// -- OLED -----------------------------------------------------
void oledSafe(int val) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0,0);  oled.println("FIRE ALERT SYSTEM");
  oled.drawLine(0,10,128,10,SSD1306_WHITE);
  oled.setCursor(0,16); oled.println("Status : SAFE");
  oled.setCursor(0,30); oled.print("Smoke  : "); oled.println(val);
  oled.setCursor(0,50); oled.println("System OK");
  oled.display();
}

void oledWarning(int val) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0,0);  oled.println("!! WARNING !!");
  oled.drawLine(0,10,128,10,SSD1306_WHITE);
  oled.setCursor(0,16); oled.println("Smoke detected!");
  oled.setCursor(0,28); oled.print("ADC: "); oled.println(val);
  oled.setCursor(0,42); oled.println("Check area now!");
  oled.display();
}

void oledDanger(int val) {
  static bool blink = false;
  blink = !blink;
  oled.clearDisplay();
  if (blink) {
    oled.fillRect(0,0,128,14,SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
  } else {
    oled.setTextColor(SSD1306_WHITE);
  }
  oled.setTextSize(1);
  oled.setCursor(2,3);  oled.println("!! FIRE DANGER !!");
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(10,18); oled.println("EVACUATE");
  oled.setTextSize(1);
  oled.setCursor(0,42); oled.print("ADC: "); oled.println(val);
  oled.setCursor(0,54); oled.println("Call 114 NOW!");
  oled.display();
}

// -- BUZZER ---------------------------------------------------
void buzzAlert(String level) {
  if (silenced) return;
  unsigned long now = millis();
  unsigned long interval = (level == "DANGER") ? 150 : 500;
  if (now - lastBuzz >= interval) {
    lastBuzz  = now;
    buzzState = !buzzState;
    digitalWrite(PIN_BUZZER, buzzState ? HIGH : LOW);
  }
}

void buzzOff() {
  buzzState = false;
  digitalWrite(PIN_BUZZER, LOW);
}

// -- TELEGRAM -------------------------------------------------
void sendTelegram(String msg) {
  unsigned long now = millis();
  if (now - lastTelegram < TELEGRAM_COOLDOWN) return;
  lastTelegram = now;
  secured_client.setInsecure();
  bot.sendMessage(CHAT_ID, msg, "");
}

// -- WIFI -----------------------------------------------------
void connectWiFi() {
  Serial.print("Connecting WiFi: ");
  Serial.println(WIFI_SSID);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(10,18); oled.println("Connecting WiFi...");
  oled.setCursor(10,32); oled.println(WIFI_SSID);
  oled.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 60) {
      Serial.println("\nWiFi timeout! Restarting...");
      ESP.restart();
    }
  }
  Serial.print("\nConnected! IP: ");
  Serial.println(WiFi.localIP());
  oled.clearDisplay();
  oled.setCursor(5,14);  oled.println("WiFi Connected!");
  oled.setCursor(0,28);  oled.println(WiFi.localIP().toString());
  oled.setCursor(0,44);  oled.println("Web server OK");
  oled.display();
  delay(1500);
}

// -- BUILD STATUS JSON ----------------------------------------
String buildStatus() {
  bool digital = (digitalRead(PIN_MQ2_DO) == LOW);
  unsigned long up = millis() - uptimeStart;
  String j = "{";
  j += "\"smoke\":" + String(smokeValue) + ",";
  j += "\"digital\":" + String(digital?"true":"false") + ",";
  j += "\"alert\":" + String(alertActive?"true":"false") + ",";
  j += "\"level\":\"" + alertLevel + "\",";
  j += "\"uptime\":" + String(up) + ",";
  j += "\"maxSmoke\":" + String(maxSmoke) + ",";
  j += "\"logs\":[";
  bool first = true;
  for (int i = 0; i < LOG_SIZE; i++) {
    int idx = (logHead - 1 - i + LOG_SIZE) % LOG_SIZE;
    if (eventLog[idx].ts == 0) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"ts\":" + String(eventLog[idx].ts) +
         ",\"val\":" + String(eventLog[idx].val) +
         ",\"level\":\"" + eventLog[idx].level + "\"}";
  }
  j += "]}";
  return j;
}

// -- SETUP ----------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED,    OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_MQ2_DO, INPUT);
  digitalWrite(PIN_LED,    LOW);
  digitalWrite(PIN_BUZZER, LOW);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found!");
  }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(10,20); oled.println("Fire Alert System");
  oled.setCursor(30,36); oled.println("Starting...");
  oled.display();
  delay(1000);

  connectWiFi();
  uptimeStart = millis();

  // ---- ROUTES ------------------------------------------------
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/info", HTTP_GET, []() {
    String j = "{\"ip\":\"" + WiFi.localIP().toString() + "\",";
    j += "\"ssid\":\"" + String(WIFI_SSID) + "\",";
    j += "\"rssi\":" + String(WiFi.RSSI()) + "}";
    server.send(200, "application/json", j);
  });

  server.on("/status", HTTP_GET, []() {
    server.send(200, "application/json", buildStatus());
  });

  server.on("/test", HTTP_GET, []() {
    alertActive = true;
    alertLevel  = "DANGER";
    silenced    = false;
    digitalWrite(PIN_LED, HIGH);
    addLog(9999, "DANGER");
    server.send(200, "text/plain", "Test triggered");
  });

  server.on("/silence", HTTP_GET, []() {
    silenced = true;
    buzzOff();
    server.send(200, "text/plain", "Silenced");
  });

  server.on("/reset", HTTP_GET, []() {
    alertActive = false;
    silenced    = false;
    alertLevel  = "SAFE";
    maxSmoke    = 0;
    logHead     = 0;
    logCount    = 0;
    memset(eventLog, 0, sizeof(eventLog));
    digitalWrite(PIN_LED, LOW);
    buzzOff();
    server.send(200, "text/plain", "Reset OK");
  });

  server.begin();
  Serial.print("Web server started! Open: http://");
  Serial.println(WiFi.localIP());
}

// -- LOOP -----------------------------------------------------
void loop() {
  server.handleClient();  // << Quan trong: xu ly request HTTP

  unsigned long now = millis();
  if (now - lastRead >= READ_INTERVAL) {
    lastRead   = now;
    smokeValue = analogRead(PIN_MQ2_AO);
    if (smokeValue > maxSmoke) maxSmoke = smokeValue;

    String prevLevel = alertLevel;
    if (smokeValue >= DANGER_THRESHOLD) {
      alertLevel = "DANGER";  alertActive = true;
    } else if (smokeValue >= SMOKE_THRESHOLD) {
      alertLevel = "WARNING"; alertActive = true;
    } else {
      alertLevel = "SAFE";   alertActive = false; silenced = false;
    }

    if (alertLevel != prevLevel) addLog(smokeValue, alertLevel);

    digitalWrite(PIN_LED, alertActive ? HIGH : LOW);
    if (alertActive) buzzAlert(alertLevel);
    else             buzzOff();

    if      (alertLevel == "DANGER")  oledDanger(smokeValue);
    else if (alertLevel == "WARNING") oledWarning(smokeValue);
    else                              oledSafe(smokeValue);

    if (alertActive && !prevAlert) {
      String msg = "";
      if (alertLevel == "DANGER") {
        msg = "NGUY HIEM CHAY!\nADC: " + String(smokeValue) +
              "\nThoat khoi khu vuc ngay!\nGoi 114";
      } else {
        msg = "Phat hien khoi/gas!\nADC: " + String(smokeValue) +
              "\nKiem tra khu vuc ngay!";
      }
      sendTelegram(msg);
    }
    prevAlert = alertActive;

    Serial.print("Smoke: "); Serial.print(smokeValue);
    Serial.print(" | Level: "); Serial.println(alertLevel);
  }
}
