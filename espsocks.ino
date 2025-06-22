#include <ESP8266WiFi.h>
#include "client.hpp"

const char* ssid = "";
const char* password = "";

WebSocket webSocket("ws", "51.91.214.104", 8088, "/");

void onopen() {
    Serial.println("Connected!");

    webSocket.send("hi");
    webSocket.send("love");
    webSocket.send("loveee");
}

void onclose(uint16_t code, String reason) {
    Serial.print("disconnected for " + reason + " and code: " + String(code));
}

void onerror(String error) {
    Serial.println("error: " + error);
}

void onmessage(String msg, uint8_t opcode) {
    Serial.println(msg);
}

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println();
    Serial.println("Connecting to WiFi");

    WiFi.begin(ssid, password);

    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("Connected to WiFi");

    Serial.print("ip addr: ");
    Serial.println(WiFi.localIP());

    webSocket.onopen = onopen;
    webSocket.onmessage = onmessage;
    webSocket.onclose = onclose;
    webSocket.onerror = onerror;

    webSocket.setHeader("origin", "https://brutal.io");
    webSocket.connect();
}

void loop() {
    webSocket.loop();
}
