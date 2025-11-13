#include <WiFi.h>
#include <WiFiClientSecure.h>   
#include <WebServer.h>
#include <Preferences.h>
#include <pgmspace.h>
#include <PubSubClient.h>
#include <time.h> 
#include "config.h"   // ✅ Importa todas las definiciones

// CA raíz de Let's Encrypt (ISRG Root X1) para test.mosquitto.org
// fuente: Let's Encrypt (isrgrootx1.pem) :contentReference[oaicite:0]{index=0}
static const char MQTT_MOSQ_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEAzCCAuugAwIBAgIUBY1hlCGvdj4NhBXkZ/uLUZNILAwwDQYJKoZIhvcNAQEL
BQAwgZAxCzAJBgNVBAYTAkdCMRcwFQYDVQQIDA5Vbml0ZWQgS2luZ2RvbTEOMAwG
A1UEBwwFRGVyYnkxEjAQBgNVBAoMCU1vc3F1aXR0bzELMAkGA1UECwwCQ0ExFjAU
BgNVBAMMDW1vc3F1aXR0by5vcmcxHzAdBgkqhkiG9w0BCQEWEHJvZ2VyQGF0Y2hv
by5vcmcwHhcNMjAwNjA5MTEwNjM5WhcNMzAwNjA3MTEwNjM5WjCBkDELMAkGA1UE
BhMCR0IxFzAVBgNVBAgMDlVuaXRlZCBLaW5nZG9tMQ4wDAYDVQQHDAVEZXJieTES
MBAGA1UECgwJTW9zcXVpdHRvMQswCQYDVQQLDAJDQTEWMBQGA1UEAwwNbW9zcXVp
dHRvLm9yZzEfMB0GCSqGSIb3DQEJARYQcm9nZXJAYXRjaG9vLm9yZzCCASIwDQYJ
KoZIhvcNAQEBBQADggEPADCCAQoCggEBAME0HKmIzfTOwkKLT3THHe+ObdizamPg
UZmD64Tf3zJdNeYGYn4CEXbyP6fy3tWc8S2boW6dzrH8SdFf9uo320GJA9B7U1FW
Te3xda/Lm3JFfaHjkWw7jBwcauQZjpGINHapHRlpiCZsquAthOgxW9SgDgYlGzEA
s06pkEFiMw+qDfLo/sxFKB6vQlFekMeCymjLCbNwPJyqyhFmPWwio/PDMruBTzPH
3cioBnrJWKXc3OjXdLGFJOfj7pP0j/dr2LH72eSvv3PQQFl90CZPFhrCUcRHSSxo
E6yjGOdnz7f6PveLIB574kQORwt8ePn0yidrTC1ictikED3nHYhMUOUCAwEAAaNT
MFEwHQYDVR0OBBYEFPVV6xBUFPiGKDyo5V3+Hbh4N9YSMB8GA1UdIwQYMBaAFPVV
6xBUFPiGKDyo5V3+Hbh4N9YSMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL
BQADggEBAGa9kS21N70ThM6/Hj9D7mbVxKLBjVWe2TPsGfbl3rEDfZ+OKRZ2j6AC
6r7jb4TZO3dzF2p6dgbrlU71Y/4K0TdzIjRj3cQ3KSm41JvUQ0hZ/c04iGDg/xWf
+pp58nfPAYwuerruPNWmlStWAXf0UTqRtg4hQDWBuUFDJTuWuuBvEXudz74eh/wK
sMwfu1HFvjy5Z0iMDU8PUDepjVolOCue9ashlS4EB5IECdSR2TItnAIiIwimx839
LdUdRudafMu5T5Xma182OC0/u/xRlEm+tvKGGmfFcN0piqVl8OrSPBgIlb+1IKJE
m/XriWr/Cq4h/JfB7NTsezVslgkBaoU=
-----END CERTIFICATE-----
)EOF";



// ====== NTP para que TLS tenga fecha válida ======
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = -5 * 3600;   // Colombia (UTC-5)
const int   DST_OFFSET_SEC = 0;

//========================= Persistencia (NVS) ===============================
Preferences prefs;
String savedSSID = "";
String savedPASS = "";

//========================= Servidor Web ====================================
WebServer server(80);

//========================= PWM =============================================
const int FREQ = PWM_FREQ;
const int RESOLUTION = PWM_RESOLUTION;
const int DUTY_100 = 255;
const int DUTY_10 = (DUTY_100 * 10) / 100;
const int DUTY_20 = (DUTY_100 * 20) / 100;
const int DUTY_30 = (DUTY_100 * 30) / 100;
const int DUTY_40 = (DUTY_100 * 40) / 100;
const int DUTY_50 = (DUTY_100 * 50) / 100;
const int DUTY_60 = (DUTY_100 * 60) / 100;
const int DUTY_70 = (DUTY_100 * 70) / 100;
const int DUTY_80 = (DUTY_100 * 80) / 100;
const int DUTY_90 = (DUTY_100 * 90) / 100;

//========================= Variables globales ===============================
const int MIN_DUTY_MOVE = 110;
const int KICK_MS       = 60;
int dutySel   = DUTY_100;
int dutyGiros = DUTY_50;
unsigned long cmdEndMillis = 0;

//========================= MQTT ============================================
WiFiClientSecure netClient;   
PubSubClient mqtt(netClient);
unsigned long mqttLastRetryMs = 0;
// ========================= Ultrasonido ====================================
unsigned long lastDistMs = 0;   // control de tiempo
float ultimaDistancia = 0.0;    // valor más reciente



//========================= Prototipos ======================================
String htmlControl(); // UI control con provisión embebida
String htmlConfig();  // UI provisión clásica (opcional)
String htmlResult(const String& title, const String& msg);
String jsonStatus();
void loadSavedCreds();
void saveCreds(const String& ssid, const String& pass);
bool ssidVisible(const String& target);
bool tryConnectSTA(const String& ssid, const String& pass, uint32_t timeoutMs = 20000);
String encTypeToStr(wifi_auth_mode_t t);
int clamp(int v, int lo, int hi);

// Motores
inline void pwmWritePin(int pin, int duty) { ledcWrite(pin, duty); }
inline int effectiveDuty(int d) { if (d<=0) return 0; return (d<MIN_DUTY_MOVE)?MIN_DUTY_MOVE:d; }
inline void setEnableDutyRaw(int dutyA, int dutyB){ ledcWrite(ENA_PIN, dutyA); ledcWrite(ENB_PIN, dutyB); }
inline void setEnableDuty(int dutyA, int dutyB){ setEnableDutyRaw(effectiveDuty(dutyA), effectiveDuty(dutyB)); }
void stopMotores(){
  digitalWrite(IN1_PIN, LOW); digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW); digitalWrite(IN4_PIN, LOW);
  setEnableDutyRaw(0,0);
  cmdEndMillis = 0;
  Serial.println("[CMD] Q: STOP");
}
void kickIfNeeded(){ if (KICK_MS>0){ setEnableDutyRaw(DUTY_100, DUTY_100); delay(KICK_MS); } }
void adelanteAmbos(){
  digitalWrite(IN1_PIN, LOW);  digitalWrite(IN2_PIN, HIGH);
  digitalWrite(IN3_PIN, LOW);  digitalWrite(IN4_PIN, HIGH);
  kickIfNeeded(); setEnableDuty(dutySel, dutySel);
}
void atrasAmbos(){
  digitalWrite(IN1_PIN, HIGH); digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, HIGH); digitalWrite(IN4_PIN, LOW);
  kickIfNeeded(); setEnableDuty(dutySel, dutySel);
}
void giroDerecha(){
  digitalWrite(IN1_PIN, LOW);  digitalWrite(IN2_PIN, HIGH);
  digitalWrite(IN3_PIN, LOW);  digitalWrite(IN4_PIN, HIGH);
  kickIfNeeded(); setEnableDuty(dutyGiros, dutySel);
}
void giroIzquierda(){
  digitalWrite(IN1_PIN, LOW);  digitalWrite(IN2_PIN, HIGH);
  digitalWrite(IN3_PIN, LOW);  digitalWrite(IN4_PIN, HIGH);
  kickIfNeeded(); setEnableDuty(dutySel, dutyGiros);
}
void setDutyByNumber(int v){
  switch(v){
    case 0: dutySel=0; break;
    case 10: dutySel=DUTY_10; break;  case 20: dutySel=DUTY_20; break;
    case 30: dutySel=DUTY_30; break;  case 40: dutySel=DUTY_40; break;
    case 50: dutySel=DUTY_50; break;  case 60: dutySel=DUTY_60; break;
    case 70: dutySel=DUTY_70; break;  case 80: dutySel=DUTY_80; break;
    case 90: dutySel=DUTY_90; break;  case 100: dutySel=DUTY_100; break;
    default: Serial.print("[VEL] Invalida: "); Serial.println(v); return;
  }
  dutyGiros = max(dutySel/2, MIN_DUTY_MOVE);
  Serial.printf("[VEL] dutySel=%d/255, dutyGiros=%d\n", dutySel, dutyGiros);
}

// ========================= SENSOR HC-SR04 ================================
float leerDistanciaCM() {
  return random(10, 400) + 0.5; // simula valores entre 10 y 400 cm
}



//========================= HTML (embebido) =================================
// --- CONTROL con provisión embebida (igual que antes, omitido aquí por brevedad) ---
static const char CONTROL_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width,initial-scale=1" />
<title>Carro • Control</title>
<style>
  :root{ --bg:#0f172a; --card:#0b1220; --edge:#1f2937; --muted:#94a3b8; --txt:#e5e7eb; --accent:#22d3ee; --danger:#ef4444; }
  *{ box-sizing:border-box; } body{ margin:0; background:linear-gradient(120deg,#0f172a,#0b1323 55%,#0f172a); color:var(--txt); font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu; }
  .wrap{ max-width:720px; margin:5vh auto; padding:16px; } .card{ background:var(--card); border:1px solid rgba(255,255,255,.06); border-radius:18px; padding:18px; box-shadow:0 10px 30px rgba(0,0,0,.35); }
  h1{ margin:.2rem 0 .6rem; font-size:1.35rem; } .sub{ color:var(--muted); margin:0 0 .8rem; }
  .row{ display:flex; gap:12px; align-items:center; flex-wrap:wrap; margin:.4rem 0 .8rem; } .pill{ padding:8px 12px; border-radius:999px; border:1px solid var(--edge); background:#101827; font-size:.9rem; }
  .controls{ display:flex; gap:10px; align-items:center; flex-wrap:wrap; } input[type="range"]{ width:180px; } pre{ background:#0a0f1c; border:1px solid var(--edge); border-radius:12px; padding:10px; white-space:pre-wrap; }
  .ctl{ display:grid; gap:10px; grid-template-columns:repeat(3,1fr); margin-top:.6rem; } @media (max-width:520px){ .ctl{ grid-template-columns:repeat(3,1fr); } }
  button.btn{ border:1px solid var(--edge); background:#111827; color:var(--txt); border-radius:14px; padding:16px; font-weight:700; cursor:pointer; transition:transform .06s ease, background .2s ease, border-color .2s ease; }
  button.btn:hover{ background:#182234; border-color:#2a3a4f; } button.btn:active{ transform:scale(.98); }
  .up{ grid-column:2; } .left{ grid-column:1; } .right{ grid-column:3; } .down{ grid-column:2; } .stop{ grid-column:2; background:#7f1d1d; border-color:#b91c1c; }
  .stop:hover{ background:#991b1b; border-color:#ef4444; }
  details{ margin-top:14px; } details > summary{ list-style:none; cursor:pointer; user-select:none; padding:12px 14px; border-radius:12px; border:1px solid var(--edge); background:#101827; font-weight:700; }
  details[open] > summary{ border-bottom-left-radius:0; border-bottom-right-radius:0; }
  .wifiCard{ border:1px solid var(--edge); border-top:none; background:#0e1626; border-bottom-left-radius:12px; border-bottom-right-radius:12px; padding:14px; }
  .grid{ display:grid; gap:10px; grid-template-columns:1fr; } label{ color:var(--muted); font-size:.9rem; }
  input[type="text"], input[type="password"]{ width:100%; padding:12px 14px; border-radius:12px; border:1px solid rgba(255,255,255,.08); outline:none; background:#0b1220; color:var(--txt); font-size:1rem; }
  input[type="text"]:focus, input[type="password"]:focus{ border-color:var(--accent); box-shadow:0 0 0 4px rgba(34,211,238,.15); }
  .btnRow{ display:flex; gap:8px; flex-wrap:wrap; margin-top:8px; } .primary{ background:var(--accent); color:#05222a; border:1px solid transparent; }
  .ghost{ background:transparent; border:1px solid rgba(255,255,255,.12); color:var(--txt); } .danger{ background:#ef4444; color:#fff; border:1px solid #ef4444; }
  table{ width:100%; border-collapse:collapse; margin-top:10px; font-size:.95rem; } th,td{ text-align:left; padding:8px 10px; border-bottom:1px solid rgba(255,255,255,.06); } th{ color:#93c5fd; font-weight:600; }
  code{ background:#0b1220; padding:2px 6px; border-radius:6px; }
</style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>Carro ESP32</h1>
      <div class="sub">Flechas → mover | <b>STOP centrado</b> | Duración máx. 5 s</div>
      <div class="controls">
        <span>Velocidad: <b id="speedVal">60</b></span><input id="speed" type="range" min="0" max="100" step="10" value="60"/>
        <span>Duración (ms): <b id="durVal">800</b></span><input id="duration" type="range" min="100" max="5000" step="100" value="800"/>
        <span class="pill" id="statusPill">Status: ?</span>
      </div>
      <div class="ctl">
        <button class="btn up" onclick="send('F')">↑</button>
        <button class="btn left" onclick="send('L')">←</button>
        <button class="btn right" onclick="send('R')">→</button>
        <button class="btn down" onclick="send('B')">↓</button>
        <button class="btn stop" onclick="send('S')">■ STOP</button>
      </div>
      <pre id="log" style="margin-top:12px;"></pre>
      <details><summary>Configurar Wi-Fi (SSID / Contraseña)</summary>
        <div class="wifiCard">
          <div class="grid">
            <div><label for="ssid">SSID</label><input id="ssid" type="text" placeholder="Nombre de la red" /></div>
            <div><label for="pass">Contraseña</label><input id="pass" type="password" placeholder="Contraseña (opcional)" /></div>
          </div>
          <div class="btnRow">
            <button class="btn primary" onclick="doSave()">Guardar y Conectar</button>
            <button class="btn ghost" onclick="refreshStatus()">Estado</button>
            <button class="btn ghost" onclick="doScan()">Ver redes</button>
            <button class="btn danger" onclick="doForget()">Olvidar Wi-Fi</button>
          </div>
          <div style="margin-top:8px;color:#94a3b8;">IP AP: <code id="ipap"></code> · IP STA: <code id="ipsta"></code></div>
          <div id="kvs" style="margin-top:8px;color:#94a3b8;"></div>
          <div id="scanWrap" style="margin-top:10px;"></div>
        </div>
      </details>
    </div>
  </div>
<script>
const log = (m) => { const el = document.getElementById('log'); el.textContent = (new Date()).toLocaleTimeString()+" — "+m+"\n"+el.textContent; };
const speedEl = document.getElementById('speed'); const durationEl = document.getElementById('duration');
const speedVal = document.getElementById('speedVal'); const durVal = document.getElementById('durVal'); const statusPill = document.getElementById('statusPill');
speedEl.addEventListener('input', () => speedVal.textContent = speedEl.value);
durationEl.addEventListener('input', () => durVal.textContent = durationEl.value);
async function ping(){ try{ const r=await fetch('/health'); const j=await r.json(); statusPill.textContent="Status: "+j.status; }catch(e){ statusPill.textContent="Status: error"; } }
setInterval(ping, 2000); ping();
async function send(dir){
  try{
    const s = speedEl.value; const d = durationEl.value;
    const res = await fetch(`/api/move?dir=${dir}&speed=${s}&duration=${d}`, { method:'POST' });
    const js  = await res.json(); log("MOVE "+dir+" -> "+JSON.stringify(js));
  }catch(e){ log("ERROR "+e); }
}
async function refreshStatus(){
  try{
    const r = await fetch('/status'); const j = await r.json();
    document.getElementById('ipap').textContent  = j.ap_ip || '-';
    document.getElementById('ipsta').textContent = j.sta_ip || '0.0.0.0';
    document.getElementById('kvs').innerHTML = 'Guardado: SSID <code>'+(j.saved_ssid||'-')+'</code> · Estado STA: <code>'+(j.sta_state||'?')+'</code> · MQTT: <code>'+(j.mqtt_connected?'CONECTADO':'DESCONECTADO')+'</code>';
    if (j.saved_ssid) document.getElementById('ssid').value = j.saved_ssid;
    if (j.saved_pass) document.getElementById('pass').value = j.saved_pass;
  }catch(e){ document.getElementById('kvs').textContent='No se pudo obtener estado.'; }
}
async function doScan(){
  document.getElementById('scanWrap').innerHTML='Escaneando...';
  try{
    const r = await fetch('/scan'); const j = await r.json();
    if (!Array.isArray(j.networks)){ document.getElementById('scanWrap').textContent='Sin datos.'; return; }
    let html = '<table><thead><tr><th>SSID</th><th>RSSI</th><th>Canal</th><th>Cifrado</th></tr></thead><tbody>';
    for (const it of j.networks){ html += `<tr><td>${it.ssid||'-'}</td><td>${it.rssi}</td><td>${it.channel}</td><td>${it.enc}</td></tr>`; }
    html += '</tbody></table>'; document.getElementById('scanWrap').innerHTML=html;
  }catch(e){ document.getElementById('scanWrap').textContent='Error al escanear.'; }
}
async function doForget(){
  try{ const r=await fetch('/forget',{method:'POST'}); const t=await r.text(); alert(t); refreshStatus(); }
  catch(e){ alert('Error al olvidar credenciales'); }
}
async function doSave(){
  const ssid=document.getElementById('ssid').value.trim(); const pass=document.getElementById('pass').value;
  if (!ssid){ alert('SSID vacío'); return; }
  try{
    const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pass})});
    const tx=await r.text();
    try{ const j=JSON.parse(tx); alert((j.msg||'Guardado')+(j.sta_ip?(' | STA: '+j.sta_ip):'')); }
    catch(_){ const w=window.open("","_blank","width=480,height=600"); if(w){w.document.write(tx); w.document.close();} else { alert('Resultado recibido'); } }
    refreshStatus();
  }catch(e){ alert('Error al guardar/conectar'); }
}
refreshStatus();
</script>
</body>
</html>
)HTML";

// (También mantenemos CONFIG_HTML y helpers…)
static const char CONFIG_HTML[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Config</title></head><body><p>Usa la UI principal en <a href="/">/</a>. (Página clásica opcional).</p></body></html>)HTML";

static const char RESULT_HEAD[] PROGMEM = R"=====(<!doctype html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Equipo3 • Resultado</title>
<style>
  body{margin:0;background:#0f172a;color:#e5e7eb;font-family:system-ui,-apple-system,Segoe UI,Roboto}
  .wrap{max-width:780px;margin:6vh auto;padding:24px}
  .card{background:#111827;border:1px solid rgba(255,255,255,.06);border-radius:20px;padding:28px;box-shadow:0 10px 30px rgba(0,0,0,.35)}
  h1{margin:0 0 10px}
  a.btn{display:inline-block;margin-top:14px;padding:12px 16px;border-radius:12px;background:#22d3ee;color:#05222a;text-decoration:none;font-weight:700}
</style>
</head><body><div class="wrap"><div class="card"><h1>)=====";
static const char RESULT_MID[]  PROGMEM = R"=====(</h1><p>)=====";
static const char RESULT_TAIL[] PROGMEM = R"=====(</p><a class="btn" href="/">Volver</a></div></div></body></html>)=====";

//========================= Render helpers ==================================
String htmlControl() { return String(FPSTR(CONTROL_HTML)); }
String htmlConfig()  { return String(FPSTR(CONFIG_HTML)); }
String htmlResult(const String& title, const String& msg) {
  String html; html.reserve(2048);
  html  = FPSTR(RESULT_HEAD); html += title; html += FPSTR(RESULT_MID); html += msg; html += FPSTR(RESULT_TAIL);
  return html;
}

//========================= JSON simple (sin librerías) ======================
String jsonGetQuoted(const String& body, const char* key){
  String k = String("\"") + key + "\"";
  int p = body.indexOf(k); if (p < 0) return "";
  p = body.indexOf(':', p); if (p < 0) return "";
  while (++p < (int)body.length() && isspace((unsigned char)body[p]));
  if (p >= (int)body.length() || body[p] != '\"') return "";
  int start = ++p; bool esc=false;
  for (int i=start; i < (int)body.length(); ++i){
    char c = body[i];
    if (esc){ esc=false; continue; }
    if (c=='\\'){ esc=true; continue; }
    if (c=='\"'){ return body.substring(start, i); }
  }
  return "";
}

//========================= Wi-Fi helpers ===================================
void loadSavedCreds() {
  prefs.begin("wifi", true);
  savedSSID = prefs.getString("ssid", "");
  savedPASS = prefs.getString("pass", "");
  prefs.end();
}
void saveCreds(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  savedSSID = ssid; savedPASS = pass;
}
bool ssidVisible(const String& target) {
  int n = WiFi.scanNetworks(false, true);
  bool found = false;
  Serial.printf("[SCAN] Redes: %d\n", n);
  for (int i=0;i<n;i++){
    String s = WiFi.SSID(i);
    if (s == target) found = true;
    Serial.printf("  - %s (RSSI %d, ch %d, enc %d)\n", s.c_str(), WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i));
  }
  WiFi.scanDelete();
  Serial.printf("[SCAN] ¿Existe '%s'? %s\n", target.c_str(), found ? "Sí":"No");
  return found;
}
bool tryConnectSTA(const String& ssid, const String& pass, uint32_t timeoutMs) {
  Serial.printf("[STA] Conectando a '%s'\n", ssid.c_str());
  WiFi.mode(WIFI_AP_STA); // mantiene AP y agrega STA
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis()-t0) < timeoutMs) delay(200);
  bool ok = (WiFi.status() == WL_CONNECTED);
  Serial.printf("[STA] %s\n", ok ? "CONECTADO":"FALLO");
  if (ok) Serial.printf("[STA] IP: %s  RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return ok;
}
String encTypeToStr(wifi_auth_mode_t t) {
  switch (t) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
#if ARDUINO_ESP32_MAJOR_VERSION >= 2
    case WIFI_AUTH_WPA3_PSK: return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
#endif
    default: return "DESCONOCIDO";
  }
}

//========================= MQTT helpers ====================================
bool mqttConnected() { return mqtt.connected(); }

void mqttEnsureConnected() {
  if (!WiFi.isConnected()) return;        // sin STA no intentamos
  if (mqtt.connected()) return;
  unsigned long now = millis();
  if (now - mqttLastRetryMs < MQTT_RETRY_MS) return;

  mqttLastRetryMs = now;

  // ClientID simple
  String cid = String("ESP32-Car-") + String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.printf("[MQTT] Conectando a %s:%u ...\n", MQTT_BROKER, MQTT_PORT);
  if (mqtt.connect(cid.c_str())) {
    Serial.println("[MQTT] Conectado");
    // (Opcional: suscripciones si luego quieres recibir comandos por MQTT)
  } else {
    Serial.printf("[MQTT] Fallo rc=%d\n", mqtt.state());
  }
}

// Publica el JSON con la orden recibida
void mqttPublishMove(const String& clientIP, char dir, int speed, int duration) {
  if (!WiFi.isConnected()) return;     // requiere salida a red
  if (!mqtt.connected()) return;       // si está caído, no bloqueamos

  // JSON compacto (sin ArduinoJson)
  String payload = "{";
  payload += "\"client\":\"" + clientIP + "\",";
  payload += "\"dir\":\""; payload += dir; payload += "\",";
  payload += "\"speed\":"; payload += speed; payload += ",";
  payload += "\"duration\":"; payload += duration; payload += ",";
  payload += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  payload += "\"sta_ip\":\"" + (WiFi.isConnected()?WiFi.localIP().toString():"0.0.0.0") + "\",";
  payload += "\"ts\":"; payload += (uint32_t) (millis());  // timestamp relativo
  payload += "}";

  bool ok = mqtt.publish(MQTT_TOPIC, payload.c_str(), /*retain=*/false);
  Serial.printf("[MQTT] publish %s -> %s (%s)\n", MQTT_TOPIC, payload.c_str(), ok?"OK":"FAIL");
}

// Publica la distancia medida por el sensor ultrasonido
void mqttPublishDistance(float dist) {
  if (!WiFi.isConnected() || !mqtt.connected()) return;

  String payload = "{";
  payload += "\"distancia_cm\":" + String(dist, 2) + ",";
  payload += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  payload += "\"sta_ip\":\"" + (WiFi.isConnected() ? WiFi.localIP().toString() : "0.0.0.0") + "\",";
  payload += "\"ts\":" + String((uint32_t)millis());
  payload += "}";

  bool ok = mqtt.publish(MQTT_TOPIC_DIST, payload.c_str(), false);
  Serial.printf("[MQTT] Distancia %.2f cm -> %s (%s)\n", dist, MQTT_TOPIC_DIST, ok ? "OK" : "FAIL");
}


//========================= Status JSON =====================================
String jsonStatus() {
  String state = "DESCONECTADO";
  if (WiFi.status() == WL_CONNECTED) state = "CONECTADO";
  else if (WiFi.status() == WL_DISCONNECTED) state = "DESCONECTADO";
  else state = "EN_PROCESO";

  String json = "{";
  json += "\"ap_ip\":\""     + WiFi.softAPIP().toString() + "\",";
  json += "\"sta_ip\":\""    + ((WiFi.status()==WL_CONNECTED)?WiFi.localIP().toString():"0.0.0.0") + "\",";
  json += "\"saved_ssid\":\""+ savedSSID + "\",";
  json += "\"saved_pass\":\""+ savedPASS + "\",";
  json += "\"sta_state\":\"" + state + "\",";
  json += "\"mqtt_broker\":\"" + String(MQTT_BROKER) + "\",";
  json += "\"mqtt_port\":" + String(MQTT_PORT) + ",";
  json += "\"mqtt_topic\":\"" + String(MQTT_TOPIC) + "\",";
  json += "\"mqtt_connected\":" + String(mqttConnected() ? "true":"false");
  json += "}";
  return json;
}

//========================= Handlers HTTP ===================================
void handleRoot()     { server.send(200, "text/html", htmlControl()); } // UI control
void handleConfig()   { server.send(200, "text/html", htmlConfig());  } // UI clásica (opcional)
void handleHealth()   { server.send(200, "application/json", "{\"status\":\"ok\"}"); }
void handleStatus()   { server.send(200, "application/json", jsonStatus()); }

void handleScan() {
  int n = WiFi.scanNetworks(false, true);
  String out = "{\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i) out += ",";
    out += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    out += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    out += "\"channel\":" + String(WiFi.channel(i)) + ",";
    out += "\"enc\":\"" + encTypeToStr(WiFi.encryptionType(i)) + "\"}";
  }
  out += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", out);
  Serial.printf("[HTTP] /scan -> %d redes\n", n);
}

void handleForget() {
  prefs.begin("wifi", false);
  bool ok1 = prefs.remove("ssid");
  bool ok2 = prefs.remove("pass");
  prefs.end();
  savedSSID = ""; savedPASS = "";
  server.send(200, "text/plain", ok1 || ok2 ? "Credenciales olvidadas." : "No había credenciales.");
  Serial.println("[FORGET] Credenciales eliminadas de NVS");
}

// /save: x-www-form-urlencoded o JSON
void handleSave() {
  String ct = server.header("Content-Type"); ct.toLowerCase();
  // JSON
  if (ct.indexOf("application/json") >= 0 || server.hasArg("plain")) {
    String body = server.arg("plain");
    String ssid = jsonGetQuoted(body, "ssid");
    String pass = jsonGetQuoted(body, "pass");
    if (ssid.length()==0){ server.send(400, "application/json", "{\"error\":\"ssid vacio\"}"); return; }

    saveCreds(ssid, pass);
    Serial.printf("[SAVE:JSON] SSID '%s' len(pass)=%u\n", ssid.c_str(), pass.length());
    bool visible = ssidVisible(ssid);
    if (!visible){ server.send(200, "application/json", "{\"saved\":true,\"visible\":false,\"connected\":false,\"msg\":\"Guardado. SSID no visible.\"}"); return; }

    bool ok = tryConnectSTA(ssid, pass, 20000);
    if (ok){
      String out = String("{\"saved\":true,\"visible\":true,\"connected\":true,\"sta_ip\":\"")+WiFi.localIP().toString()+"\",\"msg\":\"Presiona RESET\"}";
      server.send(200, "application/json", out);
    } else {
      server.send(200, "application/json", "{\"saved\":true,\"visible\":true,\"connected\":false,\"msg\":\"Fallo la conexion\"}");
    }
    return;
  }
  // Form
  if (!server.hasArg("ssid") || !server.hasArg("pass")) {
    server.send(400, "text/html", htmlResult("Datos incompletos", "Faltan SSID y/o contraseña."));
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (ssid.length()==0){ server.send(400, "text/html", htmlResult("SSID vacío","Ingresa un SSID válido.")); return; }

  saveCreds(ssid, pass);
  Serial.printf("[SAVE:FORM] SSID '%s' len(pass)=%u\n", ssid.c_str(), pass.length());
  bool visible = ssidVisible(ssid);
  if (!visible) {
    server.send(200, "text/html", htmlResult("Guardado",
      "Credenciales guardadas.<br>El SSID <b>"+ssid+
      "</b> no está visible ahora mismo.<br><br>Cuando esté disponible, vuelve a intentar \"Guardar y Conectar\" o presiona RESET para reintentar al inicio."));
    return;
  }
  bool ok = tryConnectSTA(ssid, pass, 20000);
  if (ok) {
    String msg = "Conectado a <b>"+ssid+"</b>.<br>IP STA: <b>"+WiFi.localIP().toString()+"</b><br><br>⚠️ Ahora <b>presiona RESET</b> de la ESP32 para finalizar.";
    server.send(200, "text/html", htmlResult("Conexión exitosa", msg));
  } else {
    server.send(200, "text/html", htmlResult("No se pudo conectar",
      "Credenciales guardadas, SSID visible, pero falló la conexión.<br>Verifica la contraseña o la intensidad de señal y vuelve a intentar."));
  }
}

// Único endpoint de movimiento
void handleMove() {
  if (!server.hasArg("dir") || !server.hasArg("speed") || !server.hasArg("duration")) {
    server.send(400, "application/json", "{\"error\":\"params: dir(F|B|L|R|S), speed(0..100), duration(1..5000)\"}");
    return;
  }
  char dir = toupper(server.arg("dir")[0]);
  int speed = server.arg("speed").toInt();
  int duration = server.arg("duration").toInt();
  speed = clamp(speed, 0, 100);
  duration = clamp(duration, 1, 5000);

  if (speed % 10 == 0) setDutyByNumber(speed);
  else { dutySel = (DUTY_100 * speed) / 100; dutyGiros = max(dutySel / 2, MIN_DUTY_MOVE); }

  switch (dir) {
    case 'F': adelanteAmbos(); break;
    case 'B': atrasAmbos();    break;
    case 'L': giroIzquierda(); break;
    case 'R': giroDerecha();   break;
    case 'S': stopMotores();   break;
    default:
      server.send(400, "application/json", "{\"error\":\"dir debe ser F|B|L|R|S\"}"); return;
  }

  if (dir == 'S') cmdEndMillis = 0; else cmdEndMillis = millis() + (unsigned long)duration;

  String clientIP = server.client().remoteIP().toString();
  String json = String("{\"ok\":true,\"dir\":\"") + dir + "\",\"speed\":" + speed +
                ",\"duration\":" + duration + ",\"client\":\"" + clientIP + "\"}";
  server.send(200, "application/json", json);

  // ======= PUBLICAR EN MQTT =======
  mqttEnsureConnected();      // intenta conectar si aún no
  mqttPublishMove(clientIP, dir, speed, duration);
}

//========================= Utilidades varias =================================
int clamp(int v, int lo, int hi) { return (v<lo)?lo:(v>hi)?hi:v; }

//========================= Setup / Loop ====================================
void setup() {
  Serial.begin(115200); delay(150);

  // Pines motores
  pinMode(IN1_PIN, OUTPUT); pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT); pinMode(IN4_PIN, OUTPUT);

  // PWM por pin
  ledcAttach(ENA_PIN, FREQ, RESOLUTION);
  ledcAttach(ENB_PIN, FREQ, RESOLUTION);

  // Pines del sensor HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);


  stopMotores();

  // AP activo
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS, AP_CH, false, AP_MAX_CONN);
  Serial.println("===== Arranque Equipo3 =====");
  Serial.printf("[AP] SSID: %s  PASS: %s\n", AP_SSID, AP_PASS);
  Serial.print("[AP] IP: "); Serial.println(WiFi.softAPIP());

  // Cargar y (si procede) conectar STA
  loadSavedCreds();
  if (savedSSID.length() > 0) {
    Serial.printf("[NVS] SSID: '%s'  PASS len:%u\n", savedSSID.c_str(), savedPASS.length());
    if (ssidVisible(savedSSID)) {
      if (tryConnectSTA(savedSSID, savedPASS, 15000)) {
        Serial.print("[STA] IP: "); Serial.println(WiFi.localIP());
      } else {
        Serial.println("[STA] IP: 0.0.0.0");
      }
    }
  } else {
    Serial.println("[NVS] No hay credenciales guardadas.");
  }

    // Si ya tenemos STA conectada, sincronizar hora para TLS
  if (WiFi.isConnected()) {
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.printf("[TIME] Hora OK: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      Serial.println("[TIME] No se pudo obtener hora NTP (TLS puede fallar)");
    }
  }


  // Rutas HTTP
  server.on("/",        HTTP_GET,  handleRoot);     // UI control
  server.on("/config",  HTTP_GET,  handleConfig);   // UI clásica (opcional)
  server.on("/health",  HTTP_GET,  handleHealth);
  server.on("/status",  HTTP_GET,  handleStatus);
  server.on("/scan",    HTTP_GET,  handleScan);
  server.on("/save",    HTTP_POST, handleSave);
  server.on("/forget",  HTTP_POST, handleForget);
  server.on("/api/move",HTTP_POST, handleMove);
  server.onNotFound([](){ server.send(404, "application/json", "{\"error\":\"not found\"}"); });
  server.begin();
  Serial.println("[HTTP] Servidor en puerto 80 listo.");

  // MQTT init
  netClient.setCACert(MQTT_MOSQ_CA);  // ← usa el nombre que sí declaraste arriba
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
}

void loop() {
  server.handleClient();

  // Auto-stop
  if (cmdEndMillis != 0 && (long)(millis() - cmdEndMillis) >= 0) {
    stopMotores();
    Serial.println("[AUTO-STOP] Duración expirada");
  }

  // Mantener MQTT
  if (WiFi.isConnected()) {
    mqttEnsureConnected();
    mqtt.loop();
  }

    // === Sensor Ultrasonido cada 5 segundos ===
  if (millis() - lastDistMs >= DIST_INTERVAL_MS) {
    lastDistMs = millis();
    float d = leerDistanciaCM();
    if (d > 0) {
      ultimaDistancia = d;
      mqttPublishDistance(d);
    } else {
      Serial.println("[ULTRA] No se detectó eco.");
    }
  }

}