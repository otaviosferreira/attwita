/*
 * Attwita - Etapa 03: teste individual do hardware no Wokwi
 * Este sketch NÃO é o firmware final do produto.
 * Ele valida leitura do DHT22/MQ-2 e acionamento básico das saídas.
 *
 * GPIOs desta simulação:
 * DHT22 DATA: GPIO 15
 * MQ-2 AOUT: GPIO 34 (com divisor resistivo no diagrama)
 * Buzzer: GPIO 25
 * Relé bomba (canal de teste): GPIO 26
 * Relé ventilador (canal de teste): GPIO 27
 * LED Wi-Fi/status: GPIO 18
 * LED alarme: GPIO 19
 */

#include <DHTesp.h>

constexpr uint8_t PIN_DHT = 15;
constexpr uint8_t PIN_MQ2 = 34;
constexpr uint8_t PIN_BUZZER = 25;
constexpr uint8_t PIN_RELAY_PUMP = 26;
constexpr uint8_t PIN_RELAY_FAN = 27;
constexpr uint8_t PIN_LED_STATUS = 18;
constexpr uint8_t PIN_LED_ALARM = 19;

constexpr uint32_t SENSOR_INTERVAL_MS = 5000;
constexpr uint32_t OUTPUT_TEST_INTERVAL_MS = 8000;

DHTesp dht;
uint32_t lastSensorRead = 0;
uint32_t lastOutputTest = 0;
bool outputTestState = false;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_MQ2, INPUT);
  pinMode(PIN_RELAY_PUMP, OUTPUT);
  pinMode(PIN_RELAY_FAN, OUTPUT);
  pinMode(PIN_LED_STATUS, OUTPUT);
  pinMode(PIN_LED_ALARM, OUTPUT);

  // Estado inicial seguro: saídas desenergizadas.
  digitalWrite(PIN_RELAY_PUMP, HIGH);
  digitalWrite(PIN_RELAY_FAN, HIGH);
  digitalWrite(PIN_LED_STATUS, LOW);
  digitalWrite(PIN_LED_ALARM, LOW);
  noTone(PIN_BUZZER);

  dht.setup(PIN_DHT, DHTesp::DHT22);

  Serial.println("\n=== ATTWITA | TESTE DE HARDWARE ===");
  Serial.println("Saidas inicializadas em estado seguro.");
}

void testSensors() {
  TempAndHumidity reading = dht.getTempAndHumidity();
  int gasRaw = analogRead(PIN_MQ2);

  if (isnan(reading.temperature) || isnan(reading.humidity)) {
    Serial.println("[ERRO] DHT22: leitura invalida.");
    digitalWrite(PIN_LED_ALARM, HIGH);
    tone(PIN_BUZZER, 1200, 180);
  } else {
    Serial.printf("[OK] DHT22 | temperatura=%.1f C | umidade=%.1f %%\n",
                  reading.temperature, reading.humidity);
    digitalWrite(PIN_LED_STATUS, HIGH);
    digitalWrite(PIN_LED_ALARM, LOW);
  }

  Serial.printf("[INFO] MQ-2 | ADC bruto=%d (nao representa ppm calibrado)\n",
                gasRaw);
}

void testOutputs() {
  outputTestState = !outputTestState;

  // No módulo Wokwi documentado, IN em LOW conecta COM a NO.
  // O comportamento elétrico do módulo físico deverá ser confirmado.
  digitalWrite(PIN_RELAY_PUMP, outputTestState ? LOW : HIGH);
  digitalWrite(PIN_RELAY_FAN, outputTestState ? HIGH : LOW);

  if (outputTestState) {
    tone(PIN_BUZZER, 1600, 250);
    digitalWrite(PIN_LED_ALARM, HIGH);
    Serial.println("[TESTE] Canal de rele bomba=ACIONADO; ventilador=DESLIGADO");
  } else {
    noTone(PIN_BUZZER);
    digitalWrite(PIN_LED_ALARM, LOW);
    Serial.println("[TESTE] Canal de rele bomba=DESLIGADO; ventilador=ACIONADO");
  }
}

void loop() {
  const uint32_t now = millis();

  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    testSensors();
  }

  if (now - lastOutputTest >= OUTPUT_TEST_INTERVAL_MS) {
    lastOutputTest = now;
    testOutputs();
  }
}
