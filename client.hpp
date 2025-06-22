#include <WiFiClient.h>
#include <base64.h>
#include <SHA1.h>

#define WS_FIN 0x80
#define WS_MASK 0x80
#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT         0x1
#define WS_OPCODE_BINARY       0x2
#define WS_OPCODE_CLOSE        0x8
#define WS_OPCODE_PING         0x9
#define WS_OPCODE_PONG         0xA

// good thing I studied binary operations in school

class WebSocket {
public:
    WebSocket(String protocol, String h, uint16_t p, String pa) : host(h), port(p), path(pa), headers(""), onopen(nullptr), onclose(nullptr), onmessage(nullptr), onerror(nullptr) {
        if(protocol.startsWith("wss")) {
            client = new WiFiClientSecure();
        } else {
            client = new WiFiClient();
        }
    }

    ~WebSocket() {
        if(client) delete client;
    }

    void connect() {
        if (!client->connect(host.c_str(), port)) {
            hasConnection = false;
            return;
        }

        String key = getKey();
        String request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + ":" + String(port) + "\r\n";
        request += "Upgrade: websocket\r\n";
        request += "Connection: Upgrade\r\n";
        request += "Sec-WebSocket-Key: " + key + "\r\n";
        request += "Sec-WebSocket-Version: 13\r\n";
        if (headers.length()) request += headers;
        request += "\r\n";

        client->print(request);

        String response;
        while (client->connected()) {
            String line = client->readStringUntil('\n');
            if (line == "\r") break;
            response += line + "\n";
        }

        hasConnection = response.indexOf("101 Switching Protocols") != -1;

        if(hasConnection && onopen) onopen();
        else if(!hasConnection && onerror) onerror(response);
    }

    void close(uint16_t code, String reason) {
        if (!client || !hasConnection) return;

        uint8_t payload[125];
        payload[0] = (code >> 8) & 0xFF;
        payload[1] = code & 0xFF;

        int reasonLen = reason.length();
        reasonLen = min(reasonLen, 123);
        memcpy(payload + 2, reason.c_str(), reasonLen);

        int totalLen = 2 + reasonLen;

        uint8_t header[2];
        header[0] = WS_FIN | WS_OPCODE_CLOSE;

        uint8_t maskKey[4];
        for (int i = 0; i < 4; i++) maskKey[i] = random(0, 256);

        for (int i = 0; i < totalLen; i++) {
            payload[i] ^= maskKey[i % 4];
        }

        if (totalLen <= 125) {
            header[1] = WS_MASK | totalLen;
            client->write(header, 2);
            client->write(maskKey, 4);
            client->write(payload, totalLen);
        } else {
            return;
        }

        client->flush();
        hasConnection = false;
        client->stop();

        if(onclose) onclose(code, reason);
    }

    void send(String data) {
        if (!client || !hasConnection) return;

        size_t len = data.length();
        uint8_t header[10];
        size_t headerLen = 0;

        header[0] = WS_FIN | WS_OPCODE_TEXT;

        if (len <= 125) {
            header[1] = WS_MASK | len;
            headerLen = 2;
        } else if (len <= 65535) {
            header[1] = WS_MASK | 126;
            header[2] = (len >> 8) & 0xFF;
            header[3] = len & 0xFF;
            headerLen = 4;
        } else {
            header[1] = WS_MASK | 127;
            for (int i = 0; i < 8; i++) {
                header[2 + i] = (len >> (56 - 8 * i)) & 0xFF;
            }
            headerLen = 10;
        }

        uint8_t maskKey[4];
        for (int i = 0; i < 4; i++) maskKey[i] = random(0, 256);

        uint8_t* maskedData = new uint8_t[len];
        for (size_t i = 0; i < len; i++) {
            maskedData[i] = data[i] ^ maskKey[i % 4];
        }

        client->write(header, headerLen);
        client->write(maskKey, 4);
        client->write(maskedData, len);

        delete[] maskedData;
    }

    void send(uint8_t* data, size_t length) {
        if (!client || !hasConnection) return;

        uint8_t header[10];
        size_t headerLen = 0;

        header[0] = WS_FIN | WS_OPCODE_BINARY;

        if (length <= 125) {
            header[1] = WS_MASK | length;
            headerLen = 2;
        } else if (length <= 65535) {
            header[1] = WS_MASK | 126;
            header[2] = (length >> 8) & 0xFF;
            header[3] = length & 0xFF;
            headerLen = 4;
        } else {
            header[1] = WS_MASK | 127;
            for (int i = 0; i < 8; i++) {
                header[2 + i] = (length >> (56 - 8 * i)) & 0xFF;
            }
            headerLen = 10;
        }

        uint8_t maskKey[4];
        for (int i = 0; i < 4; i++) {
            maskKey[i] = random(0, 256);
        }

        uint8_t* maskedData = new uint8_t[length];
        for (size_t i = 0; i < length; i++) {
            maskedData[i] = data[i] ^ maskKey[i % 4];
        }

        client->write(header, headerLen);
        client->write(maskKey, 4);
        client->write(maskedData, length);

        delete[] maskedData;
    }

    void ping() {
        uint8_t header[2] = { WS_FIN | WS_OPCODE_PING, 0 };
        client->write(header, 2);
    }

    void pong() {
        uint8_t header[2] = { WS_FIN | WS_OPCODE_PONG, 0 };
        client->write(header, 2);
    }

    void setHeader(String header, String value) {
        headers += header + ": " + value + "\r\n";
    }

    void loop() {
        if (!client || !client->available()) return;

        uint8_t firstByte = client->read();
        uint8_t opcode = firstByte & 0x0F;

        uint8_t secondByte = client->read();
        bool masked = secondByte & 0x80;
        uint64_t payloadLen = secondByte & 0x7F;

        if (payloadLen == 126) {
            payloadLen = ((uint16_t)client->read() << 8) | client->read();
        } else if (payloadLen == 127) {
            payloadLen = 0;
            for (int i = 0; i < 8; i++) {
                payloadLen = (payloadLen << 8) | client->read();
            }
        }

        uint8_t maskKey[4];
        if (masked) {
            client->read(maskKey, 4);
        }

        uint8_t* payload = new uint8_t[payloadLen];
        client->read(payload, payloadLen);

        if (masked) {
            for (uint64_t i = 0; i < payloadLen; i++) {
                payload[i] ^= maskKey[i % 4];
            }
        }

        if (opcode == WS_OPCODE_TEXT || opcode == WS_OPCODE_BINARY) {
            String message = String((char*)payload, payloadLen);
            if (onmessage) onmessage(message, opcode);
        } else if (opcode == WS_OPCODE_PING) {
            if (onping) onping();
        } else if (opcode == WS_OPCODE_PONG) {
            if (onpong) onpong();
        } else if (opcode == WS_OPCODE_CLOSE) {
            uint16_t code = 1005;
            String reason = "";

            if (payloadLen >= 2) {
                code = (payload[0] << 8) | payload[1];
            }

            if (payloadLen > 2) {
                reason = String((char*)(payload + 2), payloadLen - 2);
            }

            hasConnection = false;
            client->stop();

            if (onclose) onclose(code, reason);

            if(reconnectInterval > 0) {
                delay(reconnectInterval);
                connect();
            }
        }

        delete[] payload;
    }

private:
    WiFiClient* client;
    String host, path;
    uint16_t port;
    String headers;

    String getKey() {
        uint8_t keyBytes[16];
        for (int i = 0; i < 16; i++) {
            keyBytes[i] = random(0, 256);
        }
        return base64::encode((char*)keyBytes, 16);
    }
public:
    bool hasConnection;
    int reconnectInterval;

    void (*onopen)();
    void (*onclose)(uint16_t, String);
    void (*onmessage)(String, uint8_t);
    void (*onerror)(String);
    void (*onping)();
    void (*onpong)();
};
