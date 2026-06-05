#include "servo.h"
#include <Arduino.h>

#define LED_PIN PB9
#define ADC_PIN PC3

String inputBuffer = "";

void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // 默认灭
  servo_init();
  for (int i = 0; i < 6; i++) {
    setServoAngle(i, 90, 1000);
  }
  Serial.println("Ready");
}

void parseCommand(String cmd) {
  cmd.trim();
  digitalWrite(LED_PIN, LOW); // 收到命令，LED 亮

  if (cmd == "VOLT") {
    float v = analogRead(ADC_PIN) * (3.3f / 4095.0f);
    digitalWrite(LED_PIN, HIGH);
    Serial3.println(String(v, 2));
    return;
  }

  if (cmd == "STOP") {
    for (int i = 0; i < 6; i++) {
      setServoAngle(i, 90, 500);
    }
    digitalWrite(LED_PIN, HIGH);
    Serial3.println("OK");
    return;
  }

  if (cmd.startsWith("S")) {
    // S<id>,<angle>,<time>
    int firstComma = cmd.indexOf(',');
    int secondComma = cmd.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
      digitalWrite(LED_PIN, HIGH);
      Serial3.println("ERR");
      return;
    }

    int id = cmd.substring(1, firstComma).toInt();
    int angle = cmd.substring(firstComma + 1, secondComma).toInt();
    int time_ms = cmd.substring(secondComma + 1).toInt();

    if (id < 0 || id > 5 || angle < 0 || angle > 180 || time_ms < 100) {
      digitalWrite(LED_PIN, HIGH);
      Serial3.println("ERR");
      return;
    }

    setServoAngle(id, angle, time_ms);
    digitalWrite(LED_PIN, HIGH);
    Serial3.println("OK");
    return;
  }

  digitalWrite(LED_PIN, HIGH);
  Serial3.println("ERR");
}

void loop() {
  while (Serial3.available() > 0) {
    char c = Serial3.read();
    if (c == '\n') {
      if (inputBuffer.length() > 0) {
        parseCommand(inputBuffer);
        inputBuffer = "";
      }
    } else if (c != '\r') {
      inputBuffer += c;
    }
  }
}
