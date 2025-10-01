#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <pgmspace.h>

//------------------------- Config AP (sin Internet) -------------------------
const char* AP_SSID    = "Equipo3-ESP32";
const char* AP_PASS    = "123456789";     // >= 8 chars
const uint8_t AP_CH    = 6;               // 1/6/11 recomendados
const int AP_MAX_CONN  = 4;

//------------------------- Persistencia (NVS) -------------------------------
Preferences prefs; // namespace "wifi"
String savedSSID = "";
String savedPASS = "";

//------------------------- Servidor Web ------------------------------------
WebServer server(80);

//------------------------- Prototipos --------------------------------------
String htmlIndex();
String htmlResult(const String& title, const String& msg);
String jsonStatus();
void loadSavedCreds();
void saveCreds(const String& ssid, const String& pass);
bool ssidVisible(const String& target);
bool tryConnectSTA(const String& ssid, const String& pass, uint32_t timeoutMs = 20000);
String encTypeToStr(wifi_auth_mode_t t);

//------------------------- HTML (PROGMEM) ----------------------------------
// Página principal
static const char INDEX_HTML[] PROGMEM = R"=====(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Equipo3 • Config Wi-Fi</title>
<style>
  :root{ --bg:#0f172a; --card:#111827; --accent:#22d3ee; --text:#e5e7eb; --muted:#94a3b8; }
  *{ box-sizing:border-box; }
  body{ margin:0; font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu; color:var(--text); background:linear-gradient(120deg,#0f172a,#111827 40%,#0b1323);}
  .wrap{ max-width:780px; margin:6vh auto; padding:24px; }
  .card{ background:var(--card); border:1px solid rgba(255,255,255,.06); border-radius:20px; padding:28px; box-shadow:0 10px 30px rgba(0,0,0,.35); }
  h1{ margin:0 0 12px; font-weight:700; letter-spacing:.3px; }
  p.small{ color:var(--muted); margin-top:6px; }
  form{ margin-top:18px; display:grid; gap:14px; }
  .grid{ display:grid; gap:14px; grid-template-columns:1fr; }
  label{ font-size:.92rem; color:var(--muted); }
  input{
    width:100%; padding:12px 14px; border-radius:12px; border:1px solid rgba(255,255,255,.08);
    outline:none; background:#0b1220; color:var(--text); font-size:1rem;
  }
  input:focus{ border-color:var(--accent); box-shadow:0 0 0 4px rgba(34,211,238,.15); }
  .row{ display:flex; gap:10px; flex-wrap:wrap; }
  button{
    appearance:none; border:none; border-radius:12px; padding:12px 16px; cursor:pointer;
    background:var(--accent); color:#05222a; font-weight:700; letter-spacing:.3px;
    transition:transform .04s ease, filter .2s ease;
  }
  button:hover{ filter:brightness(1.05); }
  button:active{ transform:translateY(1px); }
  .ghost{ background:transparent; border:1px solid rgba(255,255,255,.12); color:var(--text); }
  .danger{ background:#ef4444; color:#fff; }
  .kvs{ margin-top:16px; color:var(--muted); font-size:.92rem; }
  code{ background:#0b1220; padding:2px 6px; border-radius:6px; }
  .foot{ margin-top:22px; color:var(--muted); font-size:.86rem; }
  table{ width:100%; border-collapse:collapse; margin-top:10px; }
  th,td{ text-align:left; padding:8px 10px; border-bottom:1px solid rgba(255,255,255,.06); }
  th{ color:#93c5fd; font-weight:600; }
</style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>Equipo3 • Configurar Wi-Fi</h1>
      <span class="badge">AP activo: Equipo3-ESP32</span>
      <p class="small">Conéctate a este punto de acceso (sin Internet) para configurar las credenciales de tu red.</p>

      <form id="wifiForm" method="POST" action="/save">
        <div class="grid">
          <div>
            <label for="ssid">SSID</label>
            <input id="ssid" name="ssid" placeholder="Nombre de la red" required>
          </div>
          <div>
            <label for="pass">Contraseña</label>
            <input id="pass" name="pass" type="password" placeholder="Contraseña (opcional)">
          </div>
        </div>
        <div class="row">
          <button type="submit">Guardar y Conectar</button>
          <button class="ghost" type="button" id="btnStatus">Estado</button>
          <button class="ghost" type="button" id="btnScan">Ver redes disponibles</button>
          <button class="danger" type="button" id="btnForget">Olvidar Wi-Fi</button>
        </div>
      </form>

      <div class="kvs" id="kvs"></div>

      <div id="scanWrap" style="margin-top:16px;"></div>

      <div class="foot">
        <div>IP AP: <code id="ipap"></code> · IP STA: <code id="ipsta"></code></div>
        <div>Tras conectar, <b>presiona el botón RESET</b> de la ESP32 para finalizar.</div>
      </div>
    </div>
  </div>

<script>
async function refreshStatus(){
  try{
    const r = await fetch('/status'); 
    const j = await r.json();
    document.getElementById('ipap').textContent  = j.ap_ip;
    document.getElementById('ipsta').textContent = j.sta_ip;
    document.getElementById('kvs').innerHTML =
      'Guardado: SSID <code>' + (j.saved_ssid||'-') + '</code> · Estado STA: <code>' + j.sta_state + '</code>';
    if (j.saved_ssid) document.getElementById('ssid').value = j.saved_ssid;
    if (j.saved_pass) document.getElementById('pass').value = j.saved_pass;
  }catch(e){}
}
async function doScan(){
  document.getElementById('scanWrap').innerHTML = 'Escaneando...';
  try{
    const r = await fetch('/scan');
    const j = await r.json();
    if (!Array.isArray(j.networks)){ document.getElementById('scanWrap').textContent = 'Sin datos.'; return; }
    let html = '<table><thead><tr><th>SSID</th><th>RSSI</th><th>Canal</th><th>Cifrado</th></tr></thead><tbody>';
    for (const it of j.networks){
      html += `<tr><td>${it.ssid||'-'}</td><td>${it.rssi}</td><td>${it.channel}</td><td>${it.enc}</td></tr>`;
    }
    html += '</tbody></table>';
    document.getElementById('scanWrap').innerHTML = html;
  }catch(e){
    document.getElementById('scanWrap').textContent = 'Error al escanear.';
  }
}
async function doForget(){
  try{
    const r = await fetch('/forget', { method:'POST' });
    const t = await r.text();
    alert(t);
    refreshStatus();
  }catch(e){
    alert('Error al olvidar credenciales');
  }
}
document.getElementById('btnStatus').addEventListener('click', refreshStatus);
document.getElementById('btnScan').addEventListener('click', doScan);
document.getElementById('btnForget').addEventListener('click', doForget);
refreshStatus();
</script>
</body>
</html>
)=====";

// Página de resultado (con título y mensaje dinámicos)
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

//------------------------- Helpers JSON simples (sin librerías) ------------
String jsonGetQuoted(const String& body, const char* key){
  String k = String("\"") + key + "\"";
  int p = body.indexOf(k);
  if (p < 0) return "";
  p = body.indexOf(':', p); if (p < 0) return "";
  while (++p < (int)body.length() && isspace((unsigned char)body[p]));
  if (p >= (int)body.length() || body[p] != '\"') return ""; // esperamos comillas
  int start = ++p;
  bool esc=false;
  for (int i=start; i < (int)body.length(); ++i){
    char c = body[i];
    if (esc){ esc=false; continue; }
    if (c=='\\'){ esc=true; continue; }
    if (c=='\"'){ return body.substring(start, i); }
  }
  return "";
}

//------------------------- Utilidades --------------------------------------
void loadSavedCreds() {
  prefs.begin("wifi", true);      // read-only
  savedSSID = prefs.getString("ssid", "");
  savedPASS = prefs.getString("pass", "");
  prefs.end();
}

void saveCreds(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);     // write
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  savedSSID = ssid;
  savedPASS = pass;
}

bool ssidVisible(const String& target) {
  int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
  bool found = false;
  Serial.printf("[SCAN] Redes encontradas: %d\n", n);
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s == target) { found = true; }
    Serial.printf("  - %s (RSSI %d, ch %d, enc %d)\n",
                  s.c_str(), WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i));
  }
  WiFi.scanDelete();
  Serial.printf("[SCAN] ¿Existe SSID '%s'? %s\n", target.c_str(), found ? "Sí" : "No");
  return found;
}

bool tryConnectSTA(const String& ssid, const String& pass, uint32_t timeoutMs) {
  Serial.printf("[STA] Intentando conectar a SSID '%s'\n", ssid.c_str());
  WiFi.mode(WIFI_AP_STA); // mantenemos AP activo y añadimos STA
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeoutMs) {
    delay(200);
  }
  bool ok = (WiFi.status() == WL_CONNECTED);
  Serial.printf("[STA] Resultado: %s\n", ok ? "CONECTADO" : "FALLO");
  if (ok) {
    Serial.printf("[STA] IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  return ok;
}

String encTypeToStr(wifi_auth_mode_t t) {
  switch (t) {
    case WIFI_AUTH_OPEN:           return "OPEN";
    case WIFI_AUTH_WEP:            return "WEP";
    case WIFI_AUTH_WPA_PSK:        return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:       return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:   return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:return "WPA2-ENT";
#if ARDUINO_ESP32_MAJOR_VERSION >= 2
    case WIFI_AUTH_WPA3_PSK:       return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:  return "WPA2/WPA3";
#endif
    default:                       return "DESCONOCIDO";
  }
}

//------------------------- Handlers HTTP -----------------------------------
void handleRoot() {
  server.send(200, "text/html", htmlIndex());
}

void handleStatus() {
  server.send(200, "application/json", jsonStatus());
}

// /save: acepta x-www-form-urlencoded **o** JSON raw (sin librerías)
void handleSave() {
  String ct = server.header("Content-Type");
  ct.toLowerCase();

  // --- Caso JSON (raw) ---
  if (ct.indexOf("application/json") >= 0 || server.hasArg("plain")) {
    String body = server.arg("plain");
    String ssid = jsonGetQuoted(body, "ssid");
    String pass = jsonGetQuoted(body, "pass"); // puede ser ""

    if (ssid.length() == 0) {
      server.send(400, "application/json", "{\"error\":\"ssid vacio\"}");
      return;
    }

    saveCreds(ssid, pass);
    Serial.printf("[SAVE:JSON] SSID guardado: '%s'  PASS len: %u\n", ssid.c_str(), pass.length());

    bool visible = ssidVisible(ssid);
    if (!visible) {
      server.send(200, "application/json",
                  "{\"saved\":true,\"visible\":false,\"connected\":false,"
                  "\"msg\":\"Guardado. SSID no visible ahora.\"}");
      return;
    }

    bool ok = tryConnectSTA(ssid, pass, 20000);
    if (ok) {
      String out = String("{\"saved\":true,\"visible\":true,\"connected\":true,"
                          "\"sta_ip\":\"") + WiFi.localIP().toString() +
                          "\",\"msg\":\"Presiona RESET\"}";
      server.send(200, "application/json", out);
    } else {
      server.send(200, "application/json",
                  "{\"saved\":true,\"visible\":true,\"connected\":false,"
                  "\"msg\":\"Fallo la conexion\"}");
    }
    return;
  }

  // --- Caso x-www-form-urlencoded (HTML como antes) ---
  if (!server.hasArg("ssid") || !server.hasArg("pass")) {
    server.send(400, "text/html", htmlResult("Datos incompletos",
      "Faltan SSID y/o contraseña."));
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  if (ssid.length() == 0) {
    server.send(400, "text/html", htmlResult("SSID vacío",
      "Ingresa un SSID válido."));
    return;
  }

  saveCreds(ssid, pass);
  Serial.printf("[SAVE:FORM] SSID guardado: '%s'  PASS: (%u chars)\n", ssid.c_str(), pass.length());

  bool visible = ssidVisible(ssid);
  if (!visible) {
    server.send(200, "text/html", htmlResult("Guardado",
      "Credenciales guardadas.<br>El SSID <b>" + ssid +
      "</b> no está visible ahora mismo.<br><br>"
      "Cuando esté disponible, vuelve a intentar \"Guardar y Conectar\" o presiona RESET para reintentar al inicio."));
    return;
  }

  bool ok = tryConnectSTA(ssid, pass, 20000);
  if (ok) {
    String msg = "Conectado a <b>" + ssid + "</b>.<br>"
                 "IP STA: <b>" + WiFi.localIP().toString() + "</b><br><br>"
                 "⚠️ Ahora <b>presiona el botón RESET</b> de la ESP32 para finalizar.";
    server.send(200, "text/html", htmlResult("Conexión exitosa", msg));
  } else {
    server.send(200, "text/html", htmlResult("No se pudo conectar",
      "Credenciales guardadas, SSID visible, pero falló la conexión.<br>"
      "Verifica la contraseña o la intensidad de señal y vuelve a intentar."));
  }
}

void handleScan() {
  int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
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
  savedSSID = "";
  savedPASS = "";
  server.send(200, "text/plain", ok1 || ok2 ? "Credenciales olvidadas." : "No había credenciales.");
  Serial.println("[FORGET] Credenciales eliminadas de NVS");
}

//------------------------- Setup & Loop ------------------------------------
void setup() {
  // Serial para depuración mínima
  Serial.begin(115200);
  delay(150);

  // Iniciar AP (sin Internet)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS, AP_CH, false, AP_MAX_CONN);
  Serial.println("===== Arranque Equipo3 =====");
  Serial.printf("[AP] SSID: %s  PASS: %s\n", AP_SSID, AP_PASS);
  Serial.print("[AP] IP: "); Serial.println(WiFi.softAPIP());      // p.ej. 192.168.4.1

  // Cargar credenciales guardadas
  loadSavedCreds();
  if (savedSSID.length() > 0) {
    Serial.printf("[NVS] SSID guardado: '%s'  PASS len: %u\n", savedSSID.c_str(), savedPASS.length());
  } else {
    Serial.println("[NVS] No hay credenciales guardadas.");
  }

  // Si hay credenciales, escanear e intentar conectar STA automáticamente
  if (savedSSID.length() > 0) {
    if (ssidVisible(savedSSID)) {
      if (tryConnectSTA(savedSSID, savedPASS, 15000)) {
        Serial.print("[STA] IP: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println("[STA] IP: 0.0.0.0");
      }
    }
  }

  // Rutas HTTP
  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/status",  HTTP_GET,  handleStatus);
  server.on("/save",    HTTP_POST, handleSave);   // <- unificado (form o JSON)
  server.on("/scan",    HTTP_GET,  handleScan);
  server.on("/forget",  HTTP_POST, handleForget);
  server.begin();
  Serial.println("[HTTP] Servidor en puerto 80");
}

void loop() {
  server.handleClient();
}

//------------------------- Render helpers ----------------------------------
String htmlIndex() {
  return String(FPSTR(INDEX_HTML));
}

String htmlResult(const String& title, const String& msg) {
  String html;
  html.reserve(2048);
  html  = FPSTR(RESULT_HEAD);
  html += title;
  html += FPSTR(RESULT_MID);
  html += msg;
  html += FPSTR(RESULT_TAIL);
  return html;
}

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
  json += "\"sta_state\":\"" + state + "\"";
  json += "}";
  return json;
}
