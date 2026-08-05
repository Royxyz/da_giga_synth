#include "SynthNetwork.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <ESPmDNS.h> 
#include "GlobalState.h"
#include "Sequencer.h"
#include "Storage.h"

// --- Network Credentials ---
const char* ssid = "MRO_TAKI_BSNL";
const char* password = "26031972";
const char* hostname = "synth-hardware"; 

WebServer server(80); 
WebSocketsServer webSocket = WebSocketsServer(81); 

// --- Helper: Get MIME Type for SD Card Files ---
String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".json")) return "application/json";
    else if (filename.endsWith(".png")) return "image/png";
    return "text/plain";
}

File uploadFile; 

void handleFileUpload() {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        
        Serial.printf("[HTTP] Uploading file: %s\n", filename.c_str());
        
        if (SD.exists(filename)) {
            SD.remove(filename); 
        }
        uploadFile = SD.open(filename, FILE_WRITE);
        
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
        
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("[HTTP] Upload complete! Size: %u bytes\n", upload.totalSize);
        }
    }
}

// --- HTTP Route: Serve Static Files from SD ---
void handleNotFound() {
    String path = server.uri();
    if (path.endsWith("/")) {
        path += "index.html"; 
    }
    
    // Check if file exists on SD card
    if (SD.exists(path)) {
        File file = SD.open(path, FILE_READ);
        server.streamFile(file, getContentType(path));
        file.close();
        return;
    }
    
    server.send(404, "text/plain", "404: File Not Found on SD Card");
}

// --- WebSocket Uplink (Browser -> ESP32) ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WS] Client #%u disconnected\n", num);
            break;
            
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[WS] Client #%u connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
            break;
        }
            
        case WStype_TEXT: {
            // Allocate a JSON document large enough for the sequencer payload
            JsonDocument doc; 
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                const char* msgType = doc["type"];
                
                // 1. MODULATION MATRIX ROUTING
                if (strcmp(msgType, "matrix") == 0) {
                    int src = doc["src"];
                    int dest = doc["dest"];
                    float val = doc["val"]; 
                    
                    if (src >= 0 && src < 6 && dest >= 0 && dest < 5) {
                        int index = (src * 5) + dest;
                        state.modMatrix[index].store(val);
                    }
                }
                
                // 2. ENVELOPE UPDATES
                else if (strcmp(msgType, "env") == 0) {
                    const char* target = doc["target"]; // "amp", "mod1", "mod2"
                    int stage = doc["stage"];           // 0:A, 1:D, 2:S, 3:R
                    float val = doc["val"];
                    
                    if (stage >= 0 && stage <= 3) {
                        if (strcmp(target, "amp") == 0) state.ampEnv[stage].store(val);
                        else if (strcmp(target, "mod1") == 0) state.modEnv1[stage].store(val);
                        else if (strcmp(target, "mod2") == 0) state.modEnv2[stage].store(val);
                    }
                }
                
                else if (strcmp(msgType, "sequence") == 0) {
                    SequenceData newSeq;
                    memset(&newSeq, 0, sizeof(SequenceData));
                    
                    newSeq.numSteps = doc["numSteps"] | 16;
                    if (newSeq.numSteps > MAX_STEPS) newSeq.numSteps = MAX_STEPS;

                    JsonArray stepsArr = doc["steps"];
                    int stepIdx = 0;
                    
                    for (JsonVariant stepEvents : stepsArr) {
                        if (stepIdx >= newSeq.numSteps) break;
                        
                        int polyIdx = 0;
                        JsonArray eventsArr = stepEvents.as<JsonArray>();
                        
                        for (JsonVariant ev : eventsArr) {
                            if (polyIdx >= MAX_POLYPHONY) break;
                            newSeq.grid[stepIdx][polyIdx].note = ev["n"];
                            newSeq.grid[stepIdx][polyIdx].length = ev["l"];
                            polyIdx++;
                        }
                        stepIdx++;
                    }
                    sequencer.loadNextPattern(newSeq);
                }
                
                // 4. OSCILLATOR TUNING
                else if (strcmp(msgType, "tune") == 0) {
                    int osc = doc["osc"]; // 1 or 2
                    float coarse = doc["coarse"];
                    if (osc == 1) state.osc1Coarse.store(coarse);
                    if (osc == 2) state.osc2Coarse.store(coarse);
                }
                else if (strcmp(msgType, "bpm") == 0) {
                    float bpm = doc["val"];
                    sequencer.setBPM(bpm);
                }
            }
            break;
        }
        
    }
}

void networkTask(void *pvParameters) {
    WiFi.setHostname(hostname);
    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Connecting");
    
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    Serial.println(String("\n[WiFi] Connected! IP: ") + WiFi.localIP().toString());

    if (!MDNS.begin(hostname)) {
        Serial.println("[mDNS] Error setting up MDNS responder!");
    } else {
        Serial.println(String("[mDNS] Responder started: http://") + hostname + ".local");
    }

    // --- INJECT THESE ROUTES ---
    server.on("/upload", HTTP_POST, []() {
        server.send(200, "application/json", "{\"status\":\"success\"}");
    }, handleFileUpload);

    server.on("/load", HTTP_GET, []() {
        if (server.hasArg("file") && server.hasArg("bank")) {
            String filename = server.arg("file");
            int bank = server.arg("bank").toInt();
            
            bool success = loadWavetableToBank(filename.c_str(), bank);
            if (success) {
                server.send(200, "application/json", "{\"status\":\"loaded\"}");
            } else {
                server.send(500, "application/json", "{\"status\":\"failed\"}");
            }
        } else {
            server.send(400, "application/json", "{\"error\":\"Missing args\"}");
        }
    });
    // ---------------------------

    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("[HTTP] WebServer active on port 80");

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("[WS] WebSocket server active on port 81");

    unsigned long lastTelemetryTime = 0;
    const unsigned long TELEMETRY_INTERVAL = 33; // ~30Hz

    for(;;) {
        server.handleClient();
        webSocket.loop();

        unsigned long now = millis();
        if (now - lastTelemetryTime >= TELEMETRY_INTERVAL) {
            lastTelemetryTime = now;
            
            if (webSocket.connectedClients() > 0) {
                JsonDocument doc;
                doc["type"]         = "telemetry";
                
                // Hardware Macros
                doc["cutoff"]       = (int)state.filterCutoff.load();
                doc["res"]          = state.filterRes.load();
                doc["morph1"]       = state.osc1BaseMorph.load();
                doc["morph2"]       = state.osc2BaseMorph.load();
                doc["lfo1Rate"]     = state.lfo1Rate.load();
                doc["abyssSend"]    = state.abyssSend.load();
                doc["oscMix"]       = state.oscMix.load();
                
                // Hardware Buttons (Discrete States)
                doc["osc1Bank"]     = state.osc1Bank.load();
                doc["osc2Bank"]     = state.osc2Bank.load();
                doc["filterMode"]   = state.filterMode.load();
                doc["lfo1Wave"]     = state.lfo1Wave.load();
                doc["lfo2Wave"]     = state.lfo2Wave.load();
                doc["fxFreeze"]     = state.fxFreeze.load();
                
                // Sequencer Sync
                doc["currentStep"]  = sequencer.getCurrentStep(); 
                
                String output;
                serializeJson(doc, output);
                webSocket.broadcastTXT(output);
            }
        }

        // Yield to FreeRTOS watchdog
        vTaskDelay(pdMS_TO_TICKS(5)); 
    }
}