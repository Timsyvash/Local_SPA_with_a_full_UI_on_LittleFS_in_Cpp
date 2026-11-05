#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

const char *ssid = "EpsilonNET";
const char *password = "zaq123456";

// Піни для датчика HC-SR04
const int trigPin = 14; // D5
const int echoPin = 12; // D6

float distance = 0.0;
unsigned long last_sensor_time = 0;

enum States
{
  CONNECTING,
  SUCCESS,
  FAILED
};

States cur_state = CONNECTING;

unsigned long last_time_connected = 0;
unsigned long start_attempt_time = 0;
unsigned long last_time_failed = 0;

ESP8266WebServer server(80);

// Функція для зняття показників з HC-SR04 без блокування основного циклу надовго
void measureDistance()
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // Таймаут 30мс
  if (duration == 0)
  {
    distance = -1.0; // Помилка або поза діапазоном
  }
  else
  {
    distance = duration * 0.034 / 2.0; // Переведення в сантиметри
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\nBooting SPA server...");

  // Налаштування пінів датчика
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Ініціалізація LittleFS
  if (!LittleFS.begin())
  {
    Serial.println("Помилка монтування LittleFS!");
    return;
  }
  Serial.println("LittleFS замонтовано успішно.");

  // Підключення до Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  start_attempt_time = millis();
}

void loop()
{
  unsigned long cur_time = millis();

  switch (cur_state)
  {
  case CONNECTING:
    if (WiFi.status() == WL_CONNECTED)
    {
      cur_state = SUCCESS;
      Serial.println("\nWiFi connected success!");
      Serial.print("Connected to SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      // 1. Головна сторінка SPA
      server.on("/", HTTP_GET, []()
                {
        if (LittleFS.exists("/index.html")) {
          File file = LittleFS.open("/index.html", "r");
          server.streamFile(file, "text/html");
          file.close();
        } else {
          server.send(404, "text/plain", "Error: index.html not found in LittleFS!");
        } });

      // 2. GET API: передаємо uptime та дані з датчика відстані
      server.on("/api/data", HTTP_GET, []()
                {
        JsonDocument doc;
        doc["uptime"] = millis() / 1000;
        doc["distance"] = distance;
        doc["status"] = "OK";
        
        String json;
        serializeJson(doc, json);
        server.send(200, "application/json", json); });

      // 3. POST API для дій
      server.on("/api/action", HTTP_POST, []()
                {
        if (!server.hasArg("plain")) {
          server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Body is empty\"}");
          return;
        }

        String body = server.arg("plain");
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
          server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
          return;
        }

        bool newState = doc["state"] | false; 
        const char* deviceName = doc["device"] | "unknown";

        Serial.printf("Отримано POST запит для пристрою: %s, стан: %s\n", 
                      deviceName, newState ? "ON" : "OFF");

        JsonDocument responseDoc;
        responseDoc["status"] = "success";
        responseDoc["received_state"] = newState;
        
        String responseString;
        serializeJson(responseDoc, responseString);
        server.send(200, "application/json", responseString); });

      // 4. Роздача статичних файлів
      server.serveStatic("/style.css", LittleFS, "/style.css");
      server.serveStatic("/app.js", LittleFS, "/app.js");

      // 5. SPA Fallback
      server.onNotFound([]()
                        {
        File file = LittleFS.open("/index.html", "r");
        if (file) {
          server.streamFile(file, "text/html");
          file.close();
        } else {
          server.send(404, "text/plain", "404: Not Found");
        } });

      server.begin();
      Serial.println("HTTP server started.");
    }
    else
    {
      if (cur_time - last_time_connected >= 300)
      {
        last_time_connected = cur_time;
        Serial.print(".");
      }

      if (cur_time - start_attempt_time >= 30000)
      {
        Serial.println("\nFailed to connect within 30 seconds. Restarting...");
        cur_state = FAILED;
        last_time_failed = cur_time;
      }
    }
    break;

  case SUCCESS:
    server.handleClient();

    // Опитуємо датчик раз на 1 секунду без блокування loop
    if (cur_time - last_sensor_time >= 1000)
    {
      last_sensor_time = cur_time;
      measureDistance();
    }
    break;

  case FAILED:
    if (cur_time - last_time_failed >= 5000)
    {
      ESP.restart();
    }
    break;
  default:
    break;
  }
}