#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32WebServer.h>

// Définition capteurs/actionneurs
#define DHTPIN 4
#define DHTTYPE DHT11
#define LUM_SENSOR 34
#define LEVEL_SENSOR 35
#define POMPE_PIN 23

#define NUM_LEDS 2
int ledPins[NUM_LEDS] = {5, 12};

// WiFi
const char* ssid = "A15 de khouloud";
const char* password = "54042358";

// Objets
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
ESP32WebServer server(80);

// Variables
bool modeAuto = true;
float temp, humi;
int lum, level;
bool systemStatus = false;

// HTML
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <title>Hydroponie Connectée</title>
  <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" rel="stylesheet">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Poppins', sans-serif;
      display: flex;
      align-items: center;
      justify-content: center;
      height: 100vh;
      background: linear-gradient(135deg, #e0eafc, #cfdef3);
    }
    .container {
      background: rgba(255, 255, 255, 0.2);
      backdrop-filter: blur(15px);
      border-radius: 20px;
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.2);
      padding: 30px;
      width: 320px;
      text-align: center;
    }
    h2 { margin-bottom: 20px; color: #333; font-weight: 600; }
    .card p { margin: 10px 0; font-size: 1rem; }
    button {
      display: block;
      width: 100%; margin: 8px 0; padding: 12px;
      border: none; border-radius: 12px;
      background: #4CAF50; color: white; font-size: 1rem;
      cursor: pointer; transition: transform 0.2s, background 0.3s;
    }
    button:hover { background: #45a049; transform: translateY(-3px); }
    .status-ok { color: green; font-weight: bold; }
    .status-bad { color: red; font-weight: bold; }
  </style>
</head>
<body>
  <div class="container">
    <h2><i class="fas fa-leaf"></i> Hydroponie</h2>
    <div class="card">
      <p>🌡 Température: <span id="temp">--</span> °C</p>
      <p>💧 Humidité: <span id="humi">--</span> %</p>
      <p>🔆 Luminosité: <span id="lum">--</span></p>
      <p>🌊 Niveau d'eau: <span id="level">--</span></p>
      <p>🟢 État: <span id="status">--</span></p>
      <p>⚙️ Mode: <span id="mode">--</span></p>
    </div>
    <button onclick="toggleMode()">Changer Mode</button>
    <button onclick="control('led', 1)">Allumer LEDs</button>
    <button onclick="control('led', 0)">Éteindre LEDs</button>
    <button onclick="control('pump', 1)">Pompe ON</button>
    <button onclick="control('pump', 0)">Pompe OFF</button>
    <button onclick="window.location.href='/exportCSV'">Exporter CSV</button>
  </div>
  <script>
    function updateData() {
      fetch('/sensorData')
        .then(res => res.json())
        .then(data => {
          document.getElementById('temp').textContent = data.temp.toFixed(1);
          document.getElementById('humi').textContent = data.humi.toFixed(1);
          document.getElementById('lum').textContent = data.lum;
          document.getElementById('level').textContent = data.level;
          document.getElementById('status').textContent = data.status;
          document.getElementById('mode').textContent = data.mode;
          document.getElementById('status').className = (data.status == "Hydraté") ? 'status-ok' : 'status-bad';
        });
    }
    function toggleMode() {
      fetch('/toggleMode').then(() => updateData());
    }
    function control(device, state) {
      fetch(`/control?device=${device}&state=${state}`);
    }
    setInterval(updateData, 5000);
    updateData();
  </script>
</body>
</html>
)rawliteral";

// Handlers
void handleToggleMode() {
  modeAuto = !modeAuto;
  server.send(200, "text/plain", "Mode changé");
}
void handleControl() {
  String device = server.arg("device");
  int state = server.arg("state").toInt();
  if (device == "led") {
    for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], state ? HIGH : LOW);
  } else if (device == "pump") {
    digitalWrite(POMPE_PIN, state);
  }
  server.send(200, "text/plain", "Commande OK");
}
void handleSensorData() {
  temp = dht.readTemperature();
  humi = dht.readHumidity();
  lum = analogRead(LUM_SENSOR);
  level = analogRead(LEVEL_SENSOR);
  systemStatus = (level >= 1000);

  String jsonData = "{\"temp\":" + String(temp, 1) +
                    ",\"humi\":" + String(humi, 1) +
                    ",\"lum\":" + String(lum) +
                    ",\"level\":" + String(level) +
                    ",\"status\":\"" + (systemStatus ? "Hydraté" : "Pas Hydraté") + "\"" +
                    ",\"mode\":\"" + (modeAuto ? "Automatique" : "Manuel") + "\"}";
  server.send(200, "application/json", jsonData);
}
void handleExportCSV() {
  String csv = "Température,Humidité,Luminosité,Niveau d'eau\n";
  csv += String(temp, 1) + "," + String(humi, 1) + "," + String(lum) + "," + String(level) + "\n";
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=sensor_data.csv");
  server.send(200, "text/csv", csv);
}

// Setup
void setup() {
  Serial.begin(115200);
  lcd.init(); lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Connexion WiFi...");
  dht.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000); Serial.print(".");
  }
  Serial.println(); Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("WiFi Connecté");
  lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());

  pinMode(POMPE_PIN, OUTPUT);
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(ledPins[i], OUTPUT); digitalWrite(ledPins[i], LOW);
  }

  server.on("/", HTTP_GET, []() { server.send(200, "text/html", htmlPage); });
  server.on("/toggleMode", HTTP_GET, handleToggleMode);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/sensorData", HTTP_GET, handleSensorData);
  server.on("/exportCSV", HTTP_GET, handleExportCSV);
  server.begin();
}

// Loop
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 5000;
void loop() {
  server.handleClient();
  if (millis() - lastUpdate > updateInterval) {
    lastUpdate = millis();
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    lum = analogRead(LUM_SENSOR);
    level = analogRead(LEVEL_SENSOR);
    systemStatus = (level >= 1000);
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(systemStatus ? "Hydraté" : "Pas Hydraté");
    lcd.setCursor(0, 1); lcd.print("Mode: "); lcd.print(modeAuto ? "Auto" : "Manuel");
    if (modeAuto) {
      int n = map(lum, 0, 4095, 0, NUM_LEDS);
      for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], i < n ? HIGH : LOW);
      digitalWrite(POMPE_PIN, (level < 1000) ? HIGH : LOW);
    }
  }
}
