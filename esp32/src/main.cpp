// ESP32-S3 Robot Arm Controller v4.0
// FreeRTOS 多任务架构 + 网页登录认证
// Serial1 (GPIO43/44) 与 STM32 通信

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// ============ 引脚定义 ============
#define STM32_TX_PIN 43
#define STM32_RX_PIN 44

// ============ WiFi 配置 ============
const char* WIFI_SSID = "Xiaomi 13";
const char* WIFI_PASS = "88888888";

// ============ 登录配置 ============
const char* AUTH_USER = "admin";
const char* AUTH_PASS = "password";

// ============ FreeRTOS 对象 ============
SemaphoreHandle_t uartMutex = NULL;
WebServer server(80);

// ============ Session 管理 ============
String sessionToken = "";
unsigned long sessionTime = 0;
const unsigned long SESSION_TIMEOUT = 3600000; // 1 小时

bool isTokenValid(String token) {
  return token.length() > 0 && token == sessionToken && (millis() - sessionTime < SESSION_TIMEOUT);
}

// ========== 登录页面 HTML ==========
const char LOGIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Login - Robot Arm</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
.login-box { background: #16213e; border-radius: 12px; padding: 32px; width: 320px; box-shadow: 0 4px 20px rgba(0,0,0,0.4); }
h1 { text-align: center; color: #e94560; margin-bottom: 24px; font-size: 22px; }
.form-group { margin-bottom: 16px; }
.form-group label { display: block; font-size: 13px; color: #aaa; margin-bottom: 6px; }
.form-group input { width: 100%; padding: 10px 12px; background: #0f3460; color: #eee; border: 1px solid #333; border-radius: 6px; font-size: 14px; outline: none; }
.form-group input:focus { border-color: #e94560; }
.btn-login { width: 100%; padding: 12px; background: #e94560; color: #fff; border: none; border-radius: 6px; font-size: 15px; cursor: pointer; margin-top: 8px; }
.btn-login:active { background: #c73e54; }
.error { color: #e94560; text-align: center; font-size: 13px; margin-top: 12px; min-height: 20px; }
</style>
</head>
<body>
<div class="login-box">
  <h1>Robot Arm</h1>
  <div class="form-group">
    <label>Username</label>
    <input type="text" id="user" autocomplete="username">
  </div>
  <div class="form-group">
    <label>Password</label>
    <input type="password" id="pass" autocomplete="current-password">
  </div>
  <button class="btn-login" onclick="doLogin()">Login</button>
  <div class="error" id="err"></div>
</div>
<script>
function doLogin() {
  const u = document.getElementById('user').value;
  const p = document.getElementById('pass').value;
  fetch('/login?user=' + encodeURIComponent(u) + '&pass=' + encodeURIComponent(p))
    .then(r => r.text())
    .then(t => {
      if (t.startsWith('OK:')) {
        const token = t.substring(3);
        window.location.href = '/?auth=' + token;
      } else {
        document.getElementById('err').textContent = 'Invalid username or password';
      }
    });
}
document.getElementById('pass').addEventListener('keydown', e => { if (e.key === 'Enter') doLogin(); });
</script>
</body>
</html>
)rawliteral";

// ========== 控制页面 HTML ==========
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Robot Arm Controller</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee; padding: 10px; }
h1 { text-align: center; padding: 10px; color: #e94560; }
.info { text-align: center; font-size: 12px; color: #888; margin-bottom: 8px; }
.section { background: #16213e; border-radius: 8px; padding: 12px; margin: 8px 0; }
.servo-row { display: flex; align-items: center; gap: 6px; margin: 6px 0; flex-wrap: wrap; }
.servo-row label { min-width: 70px; font-size: 13px; }
.servo-row input[type=range] { flex: 1; min-width: 100px; accent-color: #e94560; }
.servo-row .val { min-width: 30px; text-align: center; font-weight: bold; color: #e94560; }
.servo-row input[type=number] { width: 70px; background: #0f3460; color: #eee; border: 1px solid #333; border-radius: 4px; padding: 4px; text-align: center; }
.servo-row button { background: #e94560; color: #fff; border: none; border-radius: 4px; padding: 6px 12px; cursor: pointer; }
.servo-row button:active { background: #c73e54; }
.btn-row { display: flex; gap: 8px; margin: 8px 0; flex-wrap: wrap; }
.btn-row button { flex: 1; padding: 12px; font-size: 15px; border: none; border-radius: 6px; cursor: pointer; color: #fff; }
.btn-stop { background: #e94560; font-size: 20px !important; font-weight: bold; }
.btn-home { background: #0f3460; }
.btn-grab { background: #533483; }
.btn-wave { background: #2b6777; }
.btn-logout { background: #333; font-size: 12px !important; padding: 6px 12px !important; }
.btn-row button:active { opacity: 0.7; }
#log { background: #0a0a1a; border-radius: 6px; padding: 8px; height: 120px; overflow-y: auto; font-size: 12px; font-family: monospace; }
#log div { margin: 2px 0; }
.tx { color: #4fc3f7; }
.rx { color: #81c784; }
.err { color: #e94560; }
</style>
</head>
<body>
<h1>Robot Arm</h1>
<div class="info" id="connInfo"></div>

<div class="section" style="text-align:center;">
  <span style="font-size:13px;">Battery: </span>
  <span id="voltage" style="font-size:18px; font-weight:bold; color:#e94560;">--</span>
  <span style="font-size:13px;"> V</span>
</div>

<div class="section" id="servos"></div>

<div class="section">
  <div class="btn-row">
    <button class="btn-stop" onclick="sendCmd('STOP')">STOP</button>
  </div>
  <div class="btn-row">
    <button class="btn-home" onclick="runPreset('home')">Home</button>
    <button class="btn-grab" onclick="runPreset('grab')">Grab</button>
    <button class="btn-wave" onclick="runPreset('wave')">Wave</button>
  </div>
  <div class="btn-row">
    <button class="btn-logout" onclick="location.href='/logout'">Logout</button>
  </div>
</div>

<div class="section">
  <div style="font-size:13px; margin-bottom:4px;">Log</div>
  <div id="log"></div>
</div>

<script>
const AUTH = new URLSearchParams(window.location.search).get('auth') || '';
const names = ['Gripper','Rotate1','ArmX-1','ArmX-2','ArmX-3','Base'];
const presets = {
  home: [
    [0,90,500,100],[1,90,500,100],[2,90,500,100],
    [3,90,500,100],[4,90,500,100],[5,90,500,0]
  ],
  grab: [
    [5,90,800,200],[4,60,800,200],[3,100,600,200],
    [2,70,600,200],[0,30,500,800],[4,100,600,200],
    [3,120,500,700],[0,150,400,600],[3,80,800,200],
    [4,40,800,800],[5,140,1000,1200],[4,100,600,200],
    [3,120,500,700],[0,30,400,600],[3,80,600,200],
    [4,40,600,200],[5,90,800,200],[2,70,600,200],[0,90,500,0]
  ],
  wave: [
    [5,60,400,500],[5,120,400,500],[5,60,400,500],
    [5,120,400,500],[5,90,400,0]
  ]
};

let seqRunning = false;

// Build servo sliders
const container = document.getElementById('servos');
for (let i = 0; i < 6; i++) {
  const row = document.createElement('div');
  row.className = 'servo-row';
  row.innerHTML = `
    <label>${i} ${names[i]}</label>
    <input type="range" min="0" max="180" value="90" id="s${i}">
    <span class="val" id="v${i}">90</span>
    <input type="number" min="100" max="5000" value="1000" id="t${i}" step="100">
    <span style="font-size:11px">ms</span>
    <button onclick="sendServo(${i})">Send</button>
  `;
  container.appendChild(row);
  document.getElementById('s'+i).addEventListener('input', function() {
    document.getElementById('v'+i).textContent = this.value;
  });
}

// 显示连接信息
fetch('/info?auth=' + AUTH).then(r=>r.text()).then(t=>{
  document.getElementById('connInfo').textContent = t;
});

// 定期读取电池电压
function refreshVoltage() {
  fetch('/cmd?auth=' + AUTH + '&c=VOLT')
    .then(r => r.text())
    .then(v => {
      document.getElementById('voltage').textContent = v;
      const val = parseFloat(v);
      document.getElementById('voltage').style.color = val < 3.5 ? '#ff4444' : '#e94560';
    })
    .catch(() => {});
}
refreshVoltage();
setInterval(refreshVoltage, 5000);

function log(msg, cls) {
  const el = document.getElementById('log');
  const d = document.createElement('div');
  d.className = cls || '';
  d.textContent = msg;
  el.appendChild(d);
  el.scrollTop = el.scrollHeight;
}

function sendServo(i) {
  const angle = document.getElementById('s'+i).value;
  const time = document.getElementById('t'+i).value;
  sendCmd('S' + i + ',' + angle + ',' + time);
}

function sendCmd(cmd) {
  log('TX: ' + cmd, 'tx');
  fetch('/cmd?auth=' + AUTH + '&c=' + encodeURIComponent(cmd))
    .then(r => r.text())
    .then(r => { log('RX: ' + r, 'rx'); })
    .catch(e => { log('ERR: ' + e, 'err'); });
}

function runPreset(name) {
  if (seqRunning) { seqRunning = false; return; }
  const steps = presets[name];
  if (!steps) return;
  seqRunning = true;
  log('--- ' + name + ' start ---', 'rx');
  let i = 0;
  function next() {
    if (!seqRunning || i >= steps.length) {
      seqRunning = false;
      log('--- done ---', 'rx');
      return;
    }
    const [sid, angle, moveTime, delay] = steps[i];
    document.getElementById('s'+sid).value = angle;
    document.getElementById('v'+sid).textContent = angle;
    document.getElementById('t'+sid).value = moveTime;
    sendCmd('S' + sid + ',' + angle + ',' + moveTime);
    i++;
    setTimeout(next, moveTime + delay);
  }
  next();
}
</script>
</body>
</html>
)rawliteral";

// ========== Session 验证 ==========
String getAuthToken() {
  if (server.hasArg("auth")) return server.arg("auth");
  return "";
}

// ========== 转发命令到 STM32 (线程安全) ==========
String sendToSTM32(String cmd) {
  String resp = "TIMEOUT";

  if (xSemaphoreTake(uartMutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
    Serial1.println(cmd);
    Serial.println("[TX->STM32] " + cmd);

    unsigned long start = millis();
    while (millis() - start < 2000) {
      if (Serial1.available()) {
        String line = Serial1.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
          Serial.println("[RX<-STM32] " + line);
          resp = line;
          break;
        }
      }
    }
    xSemaphoreGive(uartMutex);
  } else {
    Serial.println("[ERR] UART mutex timeout");
  }

  return resp;
}

// ========== Web 路由 ==========
void handleLogin() {
  if (!server.hasArg("user") || !server.hasArg("pass")) {
    server.send(200, "text/html", LOGIN_PAGE);
    return;
  }
  String user = server.arg("user");
  String pass = server.arg("pass");
  if (user == AUTH_USER && pass == AUTH_PASS) {
    sessionToken = String(micros()) + String(random(100000));
    sessionTime = millis();
    server.send(200, "text/plain", "OK:" + sessionToken);
    Serial.println("[AUTH] Login success");
  } else {
    server.send(200, "text/plain", "FAIL");
    Serial.println("[AUTH] Login failed: " + user);
  }
}

void handleLogout() {
  sessionToken = "";
  server.sendHeader("Location", "/login");
  server.send(302);
  Serial.println("[AUTH] Logout");
}

void handleRoot() {
  if (server.hasArg("auth") && isTokenValid(server.arg("auth"))) {
    server.send(200, "text/html", HTML_PAGE);
  } else {
    server.send(200, "text/html", LOGIN_PAGE);
  }
}

void handleInfo() {
  if (!isTokenValid(getAuthToken())) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  String info = "Local: " + WiFi.localIP().toString();
  info += " | RSSI: " + String(WiFi.RSSI()) + "dBm";
  server.send(200, "text/plain", info);
}

void handleCmd() {
  if (!isTokenValid(getAuthToken())) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  if (!server.hasArg("c")) {
    server.send(400, "text/plain", "Missing param");
    return;
  }
  String cmd = server.arg("c");
  String resp = sendToSTM32(cmd);
  server.send(200, "text/plain", resp);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// ========== FreeRTOS 任务: WiFi 管理 ==========
void taskWifi(void* pvParameters) {
  Serial.printf("[WiFi] Connecting to: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }

  Serial.println();
  Serial.println("[WiFi] Connected!");
  Serial.println("[WiFi] IP: " + WiFi.localIP().toString());
  Serial.println("[WiFi] RSSI: " + String(WiFi.RSSI()) + " dBm");

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Lost, reconnecting...");
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(5000));
    } else {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }
}

// ========== FreeRTOS 任务: Web 服务器 ==========
void taskWebServer(void* pvParameters) {
  // 等待 WiFi 连接
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  // 注册路由
  server.on("/login", handleLogin);
  server.on("/logout", handleLogout);
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/info", handleInfo);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("[Web] Server started on port 80");
  Serial.println("[Web] natapp -> " + WiFi.localIP().toString() + ":80");

  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ========== FreeRTOS 任务: USB CDC 串口 ==========
void taskUsbSerial(void* pvParameters) {
  String buffer = "";

  for (;;) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        buffer.trim();
        if (buffer.length() > 0) {
          Serial.println("[USB] " + buffer);
          String resp = sendToSTM32(buffer);
          Serial.println(resp);
        }
        buffer = "";
      } else if (c != '\r') {
        buffer += c;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ========== Setup & Loop ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== ESP32-S3 RobotArm Controller v4.0 ===");
  Serial.println("=== FreeRTOS + Login Auth ===");

  // 初始化 UART 互斥锁
  uartMutex = xSemaphoreCreateMutex();

  // 初始化与 STM32 的串口
  Serial1.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  Serial.println("[UART] Serial1: TX=" + String(STM32_TX_PIN) + " RX=" + String(STM32_RX_PIN));

  // 随机种子 (用于 session token)
  randomSeed(esp_random());

  // 创建 FreeRTOS 任务
  xTaskCreatePinnedToCore(taskWifi,       "WiFi",      4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskWebServer,  "WebServer",  8192, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskUsbSerial,  "UsbSerial",  4096, NULL, 2, NULL, 1);

  Serial.println("[SYS] Tasks created, starting scheduler...");
  Serial.println("----------------------------------");
}

void loop() {
  // FreeRTOS 任务已接管所有工作，loop 空转
  vTaskDelay(pdMS_TO_TICKS(1000));
}
