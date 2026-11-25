# Relatório de validação — Etapa 04

## Verificações realizadas

- Arquivos de firmware e configuração encontrados no branch principal após publicação.
- Estrutura PlatformIO conferida.
- Dependências declaradas: DHTesp, PubSubClient e ArduinoJson.
- Código inspecionado para inicialização segura das saídas, Wi-Fi, MQTT, leitura DHT22/MQ-2, publicação de telemetria, recepção de comandos, ACK, NTP/TTL e LWT.
- Modelo de configuração não contém credenciais reais.
- `firmware/include/config.h` está listado no .gitignore.
- Workflow de GitHub Actions criado para compilar com `pio run -d firmware`.

## Resultado

**Inspeção estática: concluída.**

**Compilação PlatformIO: pendente.** O PlatformIO CLI não está instalado neste ambiente e o ambiente de execução não conseguiu acessar github.com para clonar o projeto. A execução do workflow também não foi confirmada; a consulta de execuções retornou lista vazia.

**Teste em placa ESP32: pendente.**

**Teste integrado com Mosquitto/Node-RED: pendente.**

## Limitações conhecidas

- O callback do PubSubClient não expõe o bit RETAIN ao sketch. Portanto, o ESP32 não detecta diretamente se um comando recebido foi retido. O Node-RED deve publicar comandos com `retain=false`; o TTL reduz o risco de comandos antigos.
- O MQ-2 é publicado como ADC bruto, não ppm.
- A conexão MQTT atual usa TCP sem TLS e é destinada apenas a laboratório em rede confiável.
- A pinagem e polaridade dos relés ainda são provisórias e precisam ser verificadas com o hardware real.
- A entrada analógica MQ-2 deve receber condicionamento de tensão no circuito físico.

## Próximos testes necessários

1. Confirmar sucesso do workflow GitHub Actions.
2. Compilar localmente com PlatformIO.
3. Conectar ESP32 ao Wi-Fi e Mosquitto.
4. Verificar telemetria no broker.
5. Enviar comandos válidos e expirados.
6. Desconectar/reconectar Wi-Fi e broker.
7. Validar polaridade e estado seguro dos relés em bancada, sem cargas de potência conectadas.
