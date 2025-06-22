#include <ESP8266WiFi.h>

#include "client.hpp"

WebSocket webSocket("wss", "echo.websocket.org", 443, "/");

void onopen() {
    Serial.println("Connected!");
    webSocket.send("hi");
}

void onmessage(uint8_t* msg, uint8_t opcode) {
    Serial.println(reinterpret_cast<char*>(msg));
}

void onclose(uint16_t code, uint8_t* reason) {
    Serial.print("disconnected, code: ");
    Serial.print(String(code));
    Serial.print(", reason: ");
    Serial.println((char*)reason);
}

void setup() {
    Serial.begin(115200);

    WiFi.begin("ssid", "pass");

    Serial.println("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("Connected!");

    webSocket.onopen = onopen;
    webSocket.onmessage = onmessage;
    webSocket.onclose = onclose;

    webSocket.connect();
}

void loop() {
    webSocket.loop();
}
