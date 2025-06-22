#include <ESP8266WiFi.h>

#include "espsocks/client.hpp"

WebSocket webSocket("wss", "echo.websocket.org", 443, "/");

void onopen() {
    Serial.println("Connected!");
    webSocket.send("hi");
}

void onmessage(String msg, uint8_t opcode) {
    Serial.println("received: " + msg);
}

void onclose(uint16_t code, String reason) {
    Serial.print("disconnected, code: ");
    Serial.print(String(code));
    Serial.println(", reason: " + reason);
}

void setup() {
    Serial.begin(115200);
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    webSocket.onopen = onopen;
    webSocket.onmessage = onmessage;
    webSocket.onclose = onclose;
    webSocket.connect();
}

void loop() {
    webSocket.loop();
}
