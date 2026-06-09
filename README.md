
<h2 align="center"> 
<strong>OrbitAlert 🛰️ — Hardware</strong>
</h2>

**Global Solution 2026 — FIAP Engenharia de Software**
<br>
**EDGE COMPUTING & COMPUTER SYSTEMS**

---

## Para que serve

O OrbitAlert é um sistema físico de alertas de emergência para **enchentes e queimadas** em comunidades sem internet e sem cobertura de rede móvel. Dois dispositivos se comunicam pelo ar via rádio LoRa — um no campo, monitorando os sensores, e outro na base comunitária, exibindo o alerta em tempo real.

---

## Como funciona

```
CAMPO                              BASE COMUNITÁRIA
Arduino Nano                       Arduino Nano
  DHT22 — temperatura e umidade      ST7789 2.0" — display de status
  HC-SR04 — nível do rio             Buzzer — alerta sonoro
  Sensor nível d'água                LoRa SX1276 (RX)
  LoRa SX1276 (TX)
        │                                  │
        └──── 915 MHz · até 3 km ──────────┘
```

A cada **5 segundos** o transmissor lê os sensores, classifica o nível de risco e envia um pacote pelo ar. O receptor atualiza o display instantaneamente — sem fio, sem internet, sem celular.

---

## Níveis de alerta

| Nível | Sensor água | HC-SR04 | DHT22 | Display | Buzzer |
|---|---|---|---|---|---|
| 🟢 **NORMAL** | < 325 | > 180 cm | — | Verde | Silêncio |
| 🟡 **ATENÇÃO** | > 325 | < 180 cm | — | Amarelo | Silêncio |
| 🟠 **CRÍTICO** | > 650 | < 100 cm | — | Laranja | Bip 2 s |
| 🔴 **EMERGÊNCIA** | > 925 | < 50 cm | T > 38°C + U < 20% | Vermelho piscando | Contínuo |

---

## Hardware necessário

### Transmissor — Unidade de Campo

| Componente | Quantidade |
|---|---|
| Arduino Nano | 1 |
| Módulo LoRa SX1276 915 MHz | 1 |
| Sensor DHT22 | 1 |
| Sensor HC-SR04 | 1 |
| Sensor de nível d'água (analógico) | 1 |

### Receptor — Unidade de Base

| Componente | Quantidade |
|---|---|
| Arduino Nano | 1 |
| Módulo LoRa SX1276 915 MHz | 1 |
| Display TFT ST7789 2.0" 240×320 | 1 |
| Buzzer passivo | 1 |

> LoRa SX1276 e ST7789 operam em **3.3V**. O pino 3V3 do Nano fornece ~50 mA — suficiente para os dois juntos.

---

## Pinagem

### Transmissor

| Componente | Pino Arduino Nano |
|---|---|
| LoRa CS (NSS) | D10 |
| LoRa RST | D9 |
| LoRa DIO0 | D2 |
| LoRa SCK | D13 (SPI) |
| LoRa MOSI | D11 (SPI) |
| LoRa MISO | D12 (SPI) |
| DHT22 DATA | D4 |
| HC-SR04 TRIG | D5 |
| HC-SR04 ECHO | D6 |
| Sensor nível SIG | A0 |

### Receptor

| Componente | Pino Arduino Nano |
|---|---|
| LoRa CS (NSS) | D10 |
| LoRa RST | D9 |
| LoRa DIO0 | D2 |
| LoRa SCK | D13 (SPI) |
| LoRa MOSI | D11 (SPI) |
| LoRa MISO | D12 (SPI) |
| ST7789 CS | D8 |
| ST7789 DC | D7 |
| ST7789 RST | D6 |
| ST7789 SCL | D13 (SPI) |
| ST7789 SDA | D11 (SPI) |
| ST7789 BLK | 3.3V |
| Buzzer | D3 |

> LoRa e ST7789 **compartilham o barramento SPI** (D11/D12/D13). Cada módulo tem seu próprio CS — não há conflito.

---

## Parâmetros LoRa

```
Frequência  : 915 MHz  (banda ISM — homologada pela Anatel no Brasil)
SF          : 10       (Spreading Factor)
Bandwidth   : 125 kHz
Coding Rate : 4/5
TX Power    : 20 dBm   (máximo do SX1276 — transmissor)
Alcance     : até 3 km em campo aberto
```

> Os parâmetros devem ser **idênticos** no transmissor e no receptor.

---

## Formato do pacote

O transmissor envia um CSV pelo ar a cada 5 segundos:

```
agua,dist,temp10,umid10,alerta,tipo
```

| Campo | Descrição | Exemplo |
|---|---|---|
| `agua` | Leitura ADC do sensor (0–1023) | `850` |
| `dist` | Distância HC-SR04 em cm (-1 = erro) | `45` |
| `temp10` | Temperatura × 10 (evita float) | `395` = 39.5°C |
| `umid10` | Umidade × 10 | `182` = 18.2% |
| `alerta` | Nível: 0=NORMAL 1=ATENÇÃO 2=CRÍTICO 3=EMERGÊNCIA | `3` |
| `tipo` | Evento: 0=--- 1=ENCHENTE 2=QUEIMADA | `1` |

Exemplo real:
```
850,45,395,182,3,1
```

---

## Bibliotecas

### Transmissor
```
LoRa                    by Sandeep Mistry
DHT sensor library      by Adafruit
Adafruit Unified Sensor by Adafruit
```

### Receptor
```
LoRa                    by Sandeep Mistry
Adafruit ST7789         by Adafruit
Adafruit GFX Library    by Adafruit
Adafruit BusIO          by Adafruit
```

Instale pelo **Sketch → Include Library → Manage Libraries** no Arduino IDE.

---

## Estrutura do repositório

```
transmissor/
└── transmissor.ino    ← Arduino Nano + LoRa TX + sensores

receptor/
└── receptor.ino       ← Arduino Nano + LoRa RX + ST7789 + buzzer
```

---

## Por que LoRa 915 MHz?

O LoRa foi escolhido porque o sistema precisa funcionar em áreas remotas — florestas, margens de rios, regiões sem sinal de celular. O sinal de 915 MHz penetra vegetação e obstáculos com muito mais eficiência que o Wi-Fi (2.4 GHz) ou o NRF24L01, e o alcance de até 3 km garante que o transmissor no campo e o receptor na comunidade se comuniquem mesmo à distância.

---

## Simulação

**IMPORTANTE RESSALTAR QUE O WOKWI NÃO POSSUI TODOS OS COMPONENTES NECESSÁRIOS DO PROJETO, TORNANDO IMPOSSÍVEL SIMULAR O PROJETO REAL POR ELE. PARA CONTORNAR ESSE PROBLEMA USAMOS COMPONENTES PARECIDOS.**

O sensor de nível d'água foi trocado por um slide potenciometer - é possível simular o nível da água deslizando o potenciômetro.

O LoRa 915 MHz não foi possível trocar por nenhum outro pois o Wokwi não tem nenhum componente que funciona mandando ondas de rádio, wifi ou bluetooth. Para contornar esse problema fizemos os dois circuitos juntos (Receptor + Transmissor) e funcional. Aqui no repositório ele está separado.

---
**_link para simulação_**

https://wokwi.com/projects/466368816665166849

<img src="img/Captura de Tela 2026-06-09 às 15.36.23.png">

---
<table>
  <tr>
    <td>

## Equipe

| Nome | RM |
|---|---|
|Gianluca Antonicci | RM570081|
|Enzo Vieira Provenzano | RM569696|
|João Vitor Rodrigues Costa | RM569510|

  </td>
  <td valign="bottom" align="right" style="padding-left: 200px;" style="padding-bottom: 100px;">
    <img src="https://fiap-achievements.vercel.app/api/badge?badge=gs&year=2025&topic=future-of-work&theme=light" width="180"/>
  </td>
  </tr>
</table>
