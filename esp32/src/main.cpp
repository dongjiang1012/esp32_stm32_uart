// ESP32-S3 Web 控制器
// AP 模式热点: RobotArm (无密码)
// 网页地址: http://192.168.4.1
// Serial1 (GPIO43/44) 与 STM32 通信

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#define STM32_TX_PIN 43
#define STM32_RX_PIN 44

const char* ssid = "RobotArm";
const char* password = "";

WebServer server(80);

// ========== 网页 HTML ==========
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
</div>

<div class="section">
  <div style="font-size:13px; margin-bottom:4px;">Log</div>
  <div id="log"></div>
</div>

<script>
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
  fetch('/cmd?c=' + encodeURIComponent(cmd))
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

// ========== 转发命令到 STM32 ==========
String sendToSTM32(String cmd) {
  Serial1.println(cmd);
  Serial.println("[TX->STM32] " + cmd);

  unsigned long start = millis();
  while (millis() - start < 2000) {
    if (Serial1.available()) {
      String resp = Serial1.readStringUntil('\n');
      resp.trim();
      if (resp.length() > 0) {
        Serial.println("[RX<-STM32] " + resp);
        return resp;
      }
    }
  }
  return "TIMEOUT";
}

// ========== Web 路由 ==========
void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleCmd() {
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

// ========== Setup & Loop ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== ESP32-S3 RobotArm Web Controller ===");

  // 初始化与 STM32 的串口
  Serial1.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  Serial.println("Serial1 ready: TX=" + String(STM32_TX_PIN) + " RX=" + String(STM32_RX_PIN));

  // 创建 WiFi 热点
  WiFi.softAP(ssid, password);
  Serial.println("AP SSID: " + String(ssid));
  Serial.println("AP IP: " + WiFi.softAPIP().toString());

  // 注册 Web 路由
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started on port 80");
  Serial.println("Open http://192.168.4.1 in browser");
  Serial.println("----------------------------------");
}

void loop() {
  server.handleClient();
}
