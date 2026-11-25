# Etapa 03 — Estrutura e simulação do hardware

## Projeto Wokwi

O circuito inicial está em `hardware/wokwi/`:

- `diagram.json`: ESP32 DevKitC V4, DHT22, MQ-2, dois módulos de relé, buzzer e LEDs de status.
- `sketch.ino`: firmware temporário de teste individual dos componentes.
- `libraries.txt`: dependência do DHT22 para ESP32.

O sketch é exclusivamente um teste de bancada/simulação. Não é o firmware final e não implementa Wi-Fi ou MQTT.

## Mapeamento provisório da simulação

| Componente | Pino ESP32 | Observação |
|---|---:|---|
| DHT22 DATA | GPIO 15 | Alimentação 3,3 V no diagrama |
| MQ-2 AOUT | GPIO 34 | Divisor resistivo 12 kΩ / 20 kΩ no diagrama |
| Buzzer | GPIO 25 | Piezo do Wokwi |
| Relé canal bomba | GPIO 26 | Saída de teste, sem bomba física |
| Relé canal ventilador | GPIO 27 | Saída de teste, sem ventilador físico |
| LED status | GPIO 18 | Resistor de 220 Ω |
| LED alarme | GPIO 19 | Resistor de 220 Ω |

**Atenção:** esse mapeamento é apenas para o protótipo Wokwi. Não é pinagem física aprovada.

## Limitações confirmadas do Wokwi

A lista oficial de hardware suportado inclui ESP32, DHT22, MQ-2, buzzer, LEDs, resistores e módulo de relé. Não lista bomba d'água DC, ventilador DC, MOSFET IRFZ44N, transistor 2N2222 ou regulador LM2596 como componentes padrão simuláveis.

Alternativas nesta simulação:

- Os dois módulos de relé representam os canais de comando da bomba e do ventilador, mas não simulam os motores nem seu consumo elétrico.
- Os LEDs representam somente sinalização lógica, não uma carga de potência.
- O LM2596, MOSFET e 2N2222 ficam documentados para projeto físico e seleção de componentes equivalentes; não serão falsamente apresentados como simulados.

Fontes:
- https://docs.wokwi.com/getting-started/supported-hardware
- https://docs.wokwi.com/parts/wokwi-dht22
- https://docs.wokwi.com/parts/wokwi-gas-sensor
- https://docs.wokwi.com/parts/wokwi-relay-module
- https://docs.wokwi.com/parts/wokwi-buzzer

## Cuidados para o circuito físico

1. Confirmar a placa ESP32 exata e a pinagem real antes de replicar o diagrama.
2. O MQ-2 é alimentado em 5 V. No diagrama Wokwi, AOUT está ligado diretamente ao GPIO 34 apenas para exercitar a leitura simulada. **Essa ligação não deve ser copiada para hardware físico.** O sinal real precisa de condicionamento/divisor calculado para nunca exceder a faixa do ADC. A documentação do Wokwi alerta que sua simulação analógica é básica e não simula resistores em conjunto com componentes analógicos; por isso o divisor não está representado como circuito funcional.
3. Confirmar se o módulo de relé aceita nível lógico de 3,3 V e se é ativo em nível alto ou baixo.
4. Motores DC precisam de driver dimensionado, proteção contra cargas indutivas, fonte apropriada e cabeamento compatível.
5. O IRFZ44N não deve ser tratado como MOSFET logic-level de 3,3 V.
6. Não conectar cargas de rede elétrica durante testes de protótipo.

## Estado dos testes

Até a validação externa no simulador, os arquivos são artefatos preparados e inspecionados estaticamente. A criação dos arquivos não comprova que o circuito foi executado no Wokwi nem que os componentes funcionam em hardware real. A leitura analógica direta do MQ-2 é uma simplificação exclusiva do Wokwi, não um esquema físico seguro.
