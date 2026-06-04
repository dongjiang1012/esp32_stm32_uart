#include "servo.h"
#include <Arduino.h>

String inputBuffer = "";

void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);
  servo_init();
  for (int i = 0; i < 6; i++) {
    setServoAngle(i, 90, 1000);
  }
  Serial.println("Ready");
}

void parseCommand(String cmd) {
  cmd.trim();

  if (cmd == "STOP") {
    for (int i = 0; i < 6; i++) {
      setServoAngle(i, 90, 500);
    }
    Serial3.println("OK");
    return;
  }

  if (cmd.startsWith("S")) {
    // S<id>,<angle>,<time>
    int firstComma = cmd.indexOf(',');
    int secondComma = cmd.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
      Serial3.println("ERR");
      return;
    }

    int id = cmd.substring(1, firstComma).toInt();
    int angle = cmd.substring(firstComma + 1, secondComma).toInt();
    int time_ms = cmd.substring(secondComma + 1).toInt();

    if (id < 0 || id > 5 || angle < 0 || angle > 180 || time_ms < 100) {
      Serial3.println("ERR");
      return;
    }

    setServoAngle(id, angle, time_ms);
    Serial3.println("OK");
    return;
  }

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
