# Etapa 04 — Firmware ESP32

Firmware inicial para ESP32 com Arduino framework / PlatformIO.

## Funcionalidades

- Inicialização de saídas em estado desligado.
- Conexão Wi-Fi e tentativas periódicas de reconexão.
- Conexão MQTT e tentativas periódicas de reconexão.
- Leitura DHT22 e MQ-2 a cada 5 segundos.
- Publicação de temperatura, umidade e ADC bruto do MQ-2.
- Recebimento de comandos para bomba, ventilador, relés e buzzer.
- Validação de JSON, estado ON/OFF, NTP e TTL.
- ACK de comandos aplicados/rejeitados.
- Estado de atuadores e status do dispositivo.
- Last Will and Testament (LWT).
- Eventos de falha de leitura.

## Estrutura

```
firmware/
├── platformio.ini
├── include/config.example.h
├── src/main.cpp
└── README.md
```

## Dependências

PlatformIO instala automaticamente DHTesp, PubSubClient e ArduinoJson.

## Configuração

1. Instale VS Code e a extensão PlatformIO IDE.
2. Abra a pasta `firmware`.
3. Copie `include/config.example.h` para `include/config.h`.
4. Preencha SSID, senha Wi-Fi, IP/hostname e credenciais MQTT.
5. Compile, carregue em uma placa ESP32 compatível e abra o monitor serial em 115200 baud.

Não publique o arquivo `config.h` com credenciais reais.

## MQTT

Prefixo: `attwita/v1/device-01/`

Publica: `sensors/temperature`, `sensors/humidity`, `sensors/gas`, `state/actuators`, `status`, `events/alerts`, `commands/ack`.

Assina: `actuators/pump/set`, `actuators/fan/set`, `actuators/relay/set`, `actuators/buzzer/set`.

Use o IP LAN do broker Mosquitto; `localhost` não representa o computador do broker para o ESP32. A porta 1883 sem TLS deve ficar restrita à rede de laboratório. Produção exige TLS, autenticação e ACL.

### Payload de comando

```json
{
  "command_id": "pump-001",
  "state": "ON",
  "issued_at": 1795615200000,
  "ttl_ms": 10000
}
```

Para `actuators/relay/set`, incluir `"channel": 1` ou `"channel": 2`. O ESP32 exige relógio sincronizado via NTP e rejeita TTL nulo, expirado ou superior a 60 segundos. O callback da biblioteca PubSubClient não informa ao sketch o bit RETAIN da mensagem recebida; por isso, o firmware não consegue detectar diretamente comandos retidos. O Node-RED deve publicar comandos sempre com retain=false e o broker deve ser configurado/testado para esse fluxo.

## Limitações e segurança

- MQ-2 transmite ADC bruto, não ppm calibrado.
- A ligação analógica direta do MQ-2 no GPIO 34 é apenas uma simplificação do Wokwi. No circuito físico, dimensionar divisor/condicionamento para proteger o ADC.
- Relés são considerados ativos em nível baixo por padrão. Confirmar o módulo real e ajustar `RELAY_ACTIVE_HIGH`.
- Os relés representam canais de comando; não conecte motores reais sem driver, proteção e fonte dimensionados.
- O transporte MQTT desta configuração inicial não usa TLS e deve ficar em rede de laboratório confiável.
- Firmware ainda não foi compilado nem testado em placa física.

## Testes pendentes

- Compilação PlatformIO.
- Conexão Wi-Fi e Mosquitto.
- Publicação e assinatura MQTT.
- Reconexão após perda de rede.
- Expiração de comando e NTP.
- Polaridade dos relés.
- Testes físicos com condicionamento elétrico correto.
