#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <time.h>

#if __has_include("config.h")
  #include "config.h"
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif
#ifndef MQTT_HOST
  #define MQTT_HOST "192.168.1.10"
#endif
#ifndef MQTT_PORT
  #define MQTT_PORT 1883
#endif
#ifndef MQTT_USERNAME
  #define MQTT_USERNAME ""
#endif
#ifndef MQTT_PASSWORD
  #define MQTT_PASSWORD ""
#endif
#ifndef MQTT_CLIENT_ID
  #define MQTT_CLIENT_ID "attwita-esp32-device-01"
#endif
#ifndef MQTT_TOPIC_PREFIX
  #define MQTT_TOPIC_PREFIX "attwita/v1/device-01"
#endif
#ifndef RELAY_ACTIVE_HIGH
  #define RELAY_ACTIVE_HIGH 0
#endif
#ifndef MQ2_RAW_ALERT_THRESHOLD
  #define MQ2_RAW_ALERT_THRESHOLD 0
#endif

namespace Pins {
constexpr uint8_t DHT = 15, MQ2 = 34, BUZZER = 25;
constexpr uint8_t RELAY_PUMP = 26, RELAY_FAN = 27;
constexpr uint8_t LED_STATUS = 18, LED_ALARM = 19;
}
namespace Timing {
constexpr uint32_t SENSOR_INTERVAL_MS = 5000;
constexpr uint32_t STATUS_INTERVAL_MS = 10000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 5000;
constexpr uint32_t COMMAND_MAX_TTL_MS = 60000;
}

WiFiClient networkClient;
PubSubClient mqtt(networkClient);
DHTesp dht;
uint32_t lastSensorRead = 0, lastStatusPublish = 0;
uint32_t lastWifiAttempt = 0, lastMqttAttempt = 0;
bool pumpOn = false, fanOn = false, relay1On = false, relay2On = false, buzzerOn = false;

String topic(const char* suffix) {
  return String(MQTT_TOPIC_PREFIX) + "/" + suffix;
}

void publishJson(const String& destination, JsonDocument& document, bool retained = false) {
  if (!mqtt.connected()) return;
  char payload[384];
  const size_t length = serializeJson(document, payload, sizeof(payload));
  if (length == 0 || length >= sizeof(payload)) {
    Serial.println("[MQTT] Payload JSON excedeu o buffer.");
    return;
  }
  if (!mqtt.publish(destination.c_str(), payload, retained))
    Serial.printf("[MQTT] Falha ao publicar em %s\n", destination.c_str());
}

void publishEvent(const char* level, const char* code, const char* message) {
  JsonDocument doc;
  doc["level"] = level;
  doc["code"] = code;
  doc["message"] = message;
  doc["uptime_ms"] = millis();
  publishJson(topic("events/alerts"), doc);
}

void applyRelay(uint8_t pin, bool enabled) {
#if RELAY_ACTIVE_HIGH
  digitalWrite(pin, enabled ? HIGH : LOW);
#else
  digitalWrite(pin, enabled ? LOW : HIGH);
#endif
}

void publishActuatorState() {
  JsonDocument doc;
  doc["pump"] = pumpOn ? "ON" : "OFF";
  doc["fan"] = fanOn ? "ON" : "OFF";
  doc["relay_1"] = relay1On ? "ON" : "OFF";
  doc["relay_2"] = relay2On ? "ON" : "OFF";
  doc["buzzer"] = buzzerOn ? "ON" : "OFF";
  doc["timestamp_ms"] = static_cast<uint64_t>(time(nullptr)) * 1000ULL;
  publishJson(topic("state/actuators"), doc, true);
}

void setPump(bool enabled) {
  pumpOn = enabled;
  relay1On = enabled;
  applyRelay(Pins::RELAY_PUMP, enabled);
  publishActuatorState();
}
void setFan(bool enabled) {
  fanOn = enabled;
  relay2On = enabled;
  applyRelay(Pins::RELAY_FAN, enabled);
  publishActuatorState();
}
void setRelay(uint8_t channel, bool enabled) {
  if (channel == 1) setPump(enabled);
  else if (channel == 2) setFan(enabled);
}
void setBuzzer(bool enabled) {
  buzzerOn = enabled;
  if (enabled) {
    tone(Pins::BUZZER, 1800);
    digitalWrite(Pins::LED_ALARM, HIGH);
  } else {
    noTone(Pins::BUZZER);
    digitalWrite(Pins::LED_ALARM, LOW);
  }
  publishActuatorState();
}

bool parseState(JsonDocument& doc, bool& enabled) {
  const char* state = doc["state"] | "";
  if (strcmp(state, "ON") == 0) { enabled = true; return true; }
  if (strcmp(state, "OFF") == 0) { enabled = false; return true; }
  return false;
}

bool commandIsFresh(JsonDocument& doc) {
  const uint64_t issuedAt = doc["issued_at"] | 0ULL;
  const uint32_t ttl = doc["ttl_ms"] | 0U;
  const time_t nowSeconds = time(nullptr);
  if (nowSeconds < 1700000000 || issuedAt == 0 || ttl == 0 ||
      ttl > Timing::COMMAND_MAX_TTL_MS) return false;
  const uint64_t nowMs = static_cast<uint64_t>(nowSeconds) * 1000ULL;
  if (issuedAt > nowMs + 5000ULL) return false;
  return nowMs <= issuedAt + ttl;
}

void publishCommandAck(const char* commandId, const char* result, const char* reason) {
  JsonDocument doc;
  doc["command_id"] = commandId;
  doc["result"] = result;
  if (reason && reason[0]) doc["reason"] = reason;
  doc["timestamp_ms"] = static_cast<uint64_t>(time(nullptr)) * 1000ULL;
  publishJson(topic("commands/ack"), doc);
}

void handleCommand(const String& incomingTopic, const byte* payload,
                   unsigned int length, bool retained) {
  if (retained) {
    publishEvent("warning", "RETAINED_COMMAND_REJECTED", "Comando retido rejeitado.");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) {
    publishEvent("error", "INVALID_JSON", "Payload JSON invalido.");
    return;
  }
  const char* commandId = doc["command_id"] | "unknown";
  if (!commandIsFresh(doc)) {
    publishCommandAck(commandId, "REJECTED", "Comando expirado ou relogio sem sincronizacao NTP.");
    return;
  }
  bool enabled = false;
  if (!parseState(doc, enabled)) {
    publishCommandAck(commandId, "REJECTED", "state deve ser ON ou OFF.");
    return;
  }

  if (incomingTopic == topic("actuators/pump/set")) setPump(enabled);
  else if (incomingTopic == topic("actuators/fan/set")) setFan(enabled);
  else if (incomingTopic == topic("actuators/buzzer/set")) setBuzzer(enabled);
  else if (incomingTopic == topic("actuators/relay/set")) {
    const uint8_t channel = doc["channel"] | 0;
    if (channel != 1 && channel != 2) {
      publishCommandAck(commandId, "REJECTED", "channel deve ser 1 ou 2.");
      return;
    }
    setRelay(channel, enabled);
  } else {
    publishCommandAck(commandId, "REJECTED", "Topico desconhecido.");
    return;
  }
  publishCommandAck(commandId, "APPLIED", "");
  Serial.printf("[CMD] %s -> %s\n", incomingTopic.c_str(), enabled ? "ON" : "OFF");
}

void mqttCallback(char* rawTopic, byte* payload, unsigned int length) {
  handleCommand(String(rawTopic), payload, length, false);
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED || strlen(WIFI_SSID) == 0) return;
  const uint32_t now = millis();
  if (now - lastWifiAttempt < Timing::WIFI_RETRY_INTERVAL_MS) return;
  lastWifiAttempt = now;
  Serial.printf("[WiFi] Tentando conectar em %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void subscribeCommands() {
  mqtt.subscribe(topic("actuators/pump/set").c_str(), 1);
  mqtt.subscribe(topic("actuators/fan/set").c_str(), 1);
  mqtt.subscribe(topic("actuators/relay/set").c_str(), 1);
  mqtt.subscribe(topic("actuators/buzzer/set").c_str(), 1);
}

void connectMqtt() {
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED || strlen(MQTT_HOST) == 0) return;
  const uint32_t now = millis();
  if (now - lastMqttAttempt < Timing::MQTT_RETRY_INTERVAL_MS) return;
  lastMqttAttempt = now;
  const String statusTopic = topic("status");
  bool connected;
  if (strlen(MQTT_USERNAME) > 0) {
    connected = mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD,
                             statusTopic.c_str(), 1, true, "offline");
  } else {
    connected = mqtt.connect(MQTT_CLIENT_ID, statusTopic.c_str(), 1, true, "offline");
  }
  if (!connected) {
    Serial.printf("[MQTT] Falha, estado=%d\n", mqtt.state());
    return;
  }
  Serial.println("[MQTT] Conectado.");
  JsonDocument doc;
  doc["state"] = "online";
  doc["ip"] = WiFi.localIP().toString();
  doc["firmware"] = "0.1.0";
  publishJson(statusTopic, doc, true);
  subscribeCommands();
  publishActuatorState();
}

void publishSensorReadings() {
  const TempAndHumidity reading = dht.getTempAndHumidity();
  const int gasRaw = analogRead(Pins::MQ2);
  const uint64_t timestampMs = static_cast<uint64_t>(time(nullptr)) * 1000ULL;
  if (isnan(reading.temperature) || isnan(reading.humidity)) {
    publishEvent("error", "DHT22_READ_FAILED", "Falha na leitura do DHT22.");
    Serial.println("[SENSOR] DHT22 sem leitura valida.");
  } else {
    JsonDocument temperature, humidity;
    temperature["value"] = reading.temperature;
    temperature["unit"] = "C";
    temperature["timestamp_ms"] = timestampMs;
    humidity["value"] = reading.humidity;
    humidity["unit"] = "%";
    humidity["timestamp_ms"] = timestampMs;
    publishJson(topic("sensors/temperature"), temperature);
    publishJson(topic("sensors/humidity"), humidity);
    Serial.printf("[SENSOR] %.1f C | %.1f %%\n", reading.temperature, reading.humidity);
  }
  JsonDocument gas;
  gas["raw"] = gasRaw;
  gas["unit"] = "adc";
  gas["calibrated"] = false;
  gas["timestamp_ms"] = timestampMs;
  publishJson(topic("sensors/gas"), gas);
  Serial.printf("[SENSOR] MQ-2 ADC=%d (nao representa ppm)\n", gasRaw);
#if MQ2_RAW_ALERT_THRESHOLD > 0
  if (gasRaw >= MQ2_RAW_ALERT_THRESHOLD)
    publishEvent("warning", "MQ2_RAW_THRESHOLD", "Limite experimental de ADC ultrapassado.");
#endif
}

void publishDeviceStatus() {
  JsonDocument doc;
  doc["state"] = "online";
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime_ms"] = millis();
  publishJson(topic("status"), doc, true);
}

void initializeOutputs() {
  pinMode(Pins::RELAY_PUMP, OUTPUT);
  pinMode(Pins::RELAY_FAN, OUTPUT);
  pinMode(Pins::LED_STATUS, OUTPUT);
  pinMode(Pins::LED_ALARM, OUTPUT);
  pinMode(Pins::BUZZER, OUTPUT);
  applyRelay(Pins::RELAY_PUMP, false);
  applyRelay(Pins::RELAY_FAN, false);
  digitalWrite(Pins::LED_STATUS, LOW);
  digitalWrite(Pins::LED_ALARM, LOW);
  noTone(Pins::BUZZER);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  initializeOutputs();
  pinMode(Pins::MQ2, INPUT);
  analogReadResolution(12);
  dht.setup(Pins::DHT, DHTesp::DHT22);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(3);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("\n=== ATTWITA | FIRMWARE 0.1.0 ===");
  Serial.println("[BOOT] Saidas inicializadas em estado desligado.");
  connectWiFi();
}

void loop() {
  connectWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(Pins::LED_STATUS, HIGH);
    connectMqtt();
  } else {
    digitalWrite(Pins::LED_STATUS, LOW);
  }
  if (mqtt.connected()) mqtt.loop();

  const uint32_t now = millis();
  if (now - lastSensorRead >= Timing::SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    if (mqtt.connected()) publishSensorReadings();
    else {
      const TempAndHumidity reading = dht.getTempAndHumidity();
      Serial.printf("[LOCAL] DHT22 %.1f C / %.1f %% | MQ-2 ADC %d\n",
                    reading.temperature, reading.humidity, analogRead(Pins::MQ2));
    }
  }
  if (mqtt.connected() && now - lastStatusPublish >= Timing::STATUS_INTERVAL_MS) {
    lastStatusPublish = now;
    publishDeviceStatus();
  }
  delay(5);
}
