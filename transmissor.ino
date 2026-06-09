// ═══════════════════════════════════════════════════════════════════
//  OrbitAlert — Unidade Sensora (Transmissor)
//  Hardware : Arduino Nano + DHT22 + HC-SR04 + Sensor nível d'água
//  Rádio    : LoRa SX1276 915 MHz
//  Global Solution 2026 — FIAP Engenharia de Software
// ═══════════════════════════════════════════════════════════════════
//
//  Bibliotecas necessárias:
//    - "LoRa"               by Sandeep Mistry
//    - "DHT sensor library" by Adafruit
//    - "Adafruit Unified Sensor" by Adafruit
//
//  Diagrama de pinos:
//  ┌──────────────────────────────────────────────────┐
//  │  LoRa SX1276       →   Arduino Nano             │
//  │    VCC             →   3.3V                     │
//  │    GND             →   GND                      │
//  │    NSS (CS)        →   D10 (SS hardware)        │
//  │    RST             →   D9                       │
//  │    DIO0            →   D2                       │
//  │    SCK             →   D13 (SPI)                │
//  │    MOSI            →   D11 (SPI)                │
//  │    MISO            →   D12 (SPI)                │
//  ├──────────────────────────────────────────────────┤
//  │  DHT22             →   Arduino Nano             │
//  │    VCC             →   5V                       │
//  │    GND             →   GND                      │
//  │    DATA            →   D4                       │
//  ├──────────────────────────────────────────────────┤
//  │  HC-SR04           →   Arduino Nano             │
//  │    VCC             →   5V                       │
//  │    GND             →   GND                      │
//  │    TRIG            →   D5                       │
//  │    ECHO            →   D6                       │
//  │  Nano e HC-SR04 são ambos 5V — sem divisor      │
//  │  de tensão necessário.                          │
//  ├──────────────────────────────────────────────────┤
//  │  Sensor nível d'água (analógico)                 │
//  │    VCC             →   5V                       │
//  │    GND             →   GND                      │
//  │    SIG             →   A0                       │
//  └──────────────────────────────────────────────────┘
//
//  ADC Arduino Nano: 10 bits → faixa 0–1023
//  Pacote LoRa: CSV "agua,dist,temp10,umid10,alerta,tipo"
//  Exemplo: "850,45,395,182,3,1"
// ═══════════════════════════════════════════════════════════════════

// Biblioteca SPI — necessária para comunicação com o módulo LoRa
#include <SPI.h>
// Biblioteca LoRa — controla o módulo SX1276
#include <LoRa.h>
// Biblioteca DHT — lê temperatura e umidade do sensor DHT22
#include <DHT.h>

// ── Pinos de controle do módulo LoRa SX1276 ──────────────────────────
// CS (Chip Select): ativa o módulo no barramento SPI
#define LORA_CS   10
// RST (Reset): reinicia o módulo quando necessário
#define LORA_RST   9
// DIO0: interrupção — indica fim de transmissão ou recepção
#define LORA_DIO0  2
// Frequência de operação: 915 MHz — banda ISM homologada pela Anatel
#define LORA_FREQ  915E6

// ── Pinos dos sensores ───────────────────────────────────────────────
// Pino de dados do DHT22
#define DHT_PIN    4
// Modelo do sensor de temperatura e umidade
#define DHT_TIPO   DHT22
// Pino TRIG do HC-SR04 — dispara o pulso ultrassônico
#define TRIG_PIN   5
// Pino ECHO do HC-SR04 — recebe o eco refletido
#define ECHO_PIN   6
// Pino analógico do sensor de nível d'água (ADC 10 bits: 0–1023)
#define AGUA_PIN   A0

// ── Limites de alerta — sensor de nível (ADC 10 bits, 0–1023) ────────
// Abaixo de ATENCAO o sistema está em NORMAL
#define AGUA_ATENCAO    325
// Acima de CRITICO o sistema entra em estado crítico
#define AGUA_CRITICO    650
// Acima de EMERGENCIA o sistema dispara alerta máximo
#define AGUA_EMERGENCIA 925

// ── Limites de alerta — HC-SR04 (distância em cm) ────────────────────
// Quanto MENOR a distância, mais CHEIO está o rio
#define DIST_ATENCAO    180
#define DIST_CRITICO    100
#define DIST_EMERGENCIA  50

// ── Limites de alerta — DHT22 (risco de queimada) ────────────────────
// Temperatura armazenada como inteiro ×10 para evitar float
// 380 = 38.0°C — temperatura crítica de risco de queimada
#define TEMP_QUEIMADA   380
// 200 = 20.0% — umidade mínima antes do risco de queimada
#define UMID_QUEIMADA   200

// ── Intervalo entre transmissões (milissegundos) ─────────────────────
// O transmissor envia um pacote a cada 5 segundos
#define TX_INTERVALO_MS  5000UL

// ── Instância do sensor DHT22 ────────────────────────────────────────
DHT dht(DHT_PIN, DHT_TIPO);

// ════════════════════════════════════════════════════════════════════
//  medirDistancia()
//  Mede a distância até a superfície da água usando o HC-SR04.
//  Retorna a distância em centímetros ou -1 em caso de timeout.
//
//  Funcionamento:
//    1. Envia pulso de 10µs no pino TRIG
//    2. O sensor emite um sinal ultrassônico de 40kHz
//    3. O sinal reflete na superfície e volta ao sensor
//    4. O tempo do eco em ECHO é proporcional à distância
//    Fórmula: distância = (duração × velocidade_som) / 2
//             velocidade do som = 343 m/s = 0.0343 cm/µs
// ════════════════════════════════════════════════════════════════════
long medirDistancia() {
  // Garante que TRIG está em LOW antes de iniciar
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Dispara o pulso ultrassônico por 10 microssegundos
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Mede o tempo que o sinal levou para voltar (timeout: 30 ms = ~5 m)
  long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);

  // Se dur=0, não houve eco dentro do timeout — retorna -1 (erro)
  return (dur == 0) ? -1 : (long)(dur * 0.0343 / 2.0);
}

// ════════════════════════════════════════════════════════════════════
//  classificarAlerta()
//  Analisa todas as leituras dos sensores e define o nível de alerta.
//
//  Parâmetros:
//    agua   → leitura ADC do sensor de nível (0–1023)
//    dist   → distância HC-SR04 em cm (-1 = erro)
//    temp10 → temperatura × 10 (evita float no AVR)
//    umid10 → umidade × 10
//    alerta → saída: 0=NORMAL 1=ATENCAO 2=CRITICO 3=EMERGENCIA
//    tipo   → saída: 0=--- 1=ENCHENTE 2=QUEIMADA
//
//  Prioridade: EMERGENCIA > CRITICO > ATENCAO > NORMAL
// ════════════════════════════════════════════════════════════════════
void classificarAlerta(int agua, long dist, int temp10, int umid10,
                       uint8_t& alerta, uint8_t& tipo) {

  // EMERGÊNCIA por enchente — nível máximo de água ou distância crítica
  if (agua > AGUA_EMERGENCIA || (dist > 0 && dist < DIST_EMERGENCIA))
    { alerta = 3; tipo = 1; return; }

  // EMERGÊNCIA por queimada — temperatura alta E umidade baixa ao mesmo tempo
  if (temp10 > TEMP_QUEIMADA && umid10 < UMID_QUEIMADA)
    { alerta = 3; tipo = 2; return; }

  // CRÍTICO — nível de água ou distância em zona de perigo
  if (agua > AGUA_CRITICO || (dist > 0 && dist < DIST_CRITICO))
    { alerta = 2; tipo = 1; return; }

  // ATENÇÃO — início de elevação do nível da água
  if (agua > AGUA_ATENCAO || (dist > 0 && dist < DIST_ATENCAO))
    { alerta = 1; tipo = 1; return; }

  // NORMAL — todos os sensores dentro dos limites seguros
  alerta = 0; tipo = 0;
}

// ════════════════════════════════════════════════════════════════════
//  setup()
//  Executado uma única vez ao ligar o Arduino.
//  Inicializa sensores, comunicação serial e módulo LoRa.
// ════════════════════════════════════════════════════════════════════
void setup() {
  // Inicia a comunicação serial para debug (9600 baud)
  Serial.begin(9600);
  Serial.println(F("OrbitAlert — Transmissor (Nano + LoRa SX1276)"));

  // Configura os pinos do HC-SR04
  pinMode(TRIG_PIN, OUTPUT);  // TRIG gera o pulso — é saída
  pinMode(ECHO_PIN, INPUT);   // ECHO recebe o eco — é entrada

  // Inicializa o sensor DHT22
  dht.begin();

  // Informa ao módulo LoRa quais são os seus pinos de controle
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  // Tenta inicializar o módulo LoRa na frequência de 915 MHz
  // Se falhar, trava o programa com mensagem de erro
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println(F("[ERRO] LoRa nao inicializado!"));
    Serial.println(F("       Verifique: VCC=3.3V, CS=D10, RST=D9, DIO0=D2"));
    while (true) delay(1000); // trava — requer reinicialização
  }

  // SF10: Spreading Factor — quanto maior, maior o alcance e menor a velocidade
  LoRa.setSpreadingFactor(10);
  // 125 kHz: largura de banda — padrão LoRaWAN para melhor equilíbrio
  LoRa.setSignalBandwidth(125E3);
  // 4/5: Coding Rate — redundância mínima, maximiza velocidade
  LoRa.setCodingRate4(5);
  // 20 dBm: potência máxima do SX1276 — maior alcance possível
  LoRa.setTxPower(20);

  Serial.println(F("[OK] LoRa 915 MHz | SF10 | 125kHz | CR4/5 | 20dBm"));
  Serial.println(F("[OK] Transmissao iniciada.\n"));
}

// ════════════════════════════════════════════════════════════════════
//  loop()
//  Executado continuamente após o setup().
//  Lê sensores, classifica alerta e transmite pelo LoRa.
// ════════════════════════════════════════════════════════════════════
void loop() {
  // ── Leitura dos sensores ─────────────────────────────────────────

  // Lê o nível de água (0=seco, 1023=máximo submerso)
  int  agua  = analogRead(AGUA_PIN);

  // Mede a distância até a superfície da água em cm
  long dist  = medirDistancia();

  // Lê temperatura e umidade do DHT22
  float tempF = dht.readTemperature();
  float umidF = dht.readHumidity();

  // Converte para inteiro ×10 (evita uso de float no snprintf do AVR)
  // Se o sensor falhou (isnan), usa -999 como valor sentinela de erro
  int temp10 = isnan(tempF) ? -999 : (int)(tempF * 10.0);
  int umid10 = isnan(umidF) ? -999 : (int)(umidF * 10.0);

  // ── Classificação do alerta ──────────────────────────────────────
  uint8_t alerta, tipo;
  classificarAlerta(agua, dist, temp10, umid10, alerta, tipo);

  // ── Monta o pacote CSV para transmissão via LoRa ─────────────────
  // Formato: "agua,dist,temp10,umid10,alerta,tipo"
  // Exemplo: "850,45,395,182,3,1" (39.5°C, 18.2%, EMERGENCIA:ENCHENTE)
  char pkt[40];
  snprintf(pkt, sizeof(pkt), "%d,%ld,%d,%d,%d,%d",
           agua, dist, temp10, umid10, alerta, tipo);

  // ── Transmissão LoRa ─────────────────────────────────────────────
  LoRa.beginPacket();   // inicia o buffer de transmissão
  LoRa.print(pkt);      // carrega o pacote CSV no buffer
  int ok = LoRa.endPacket(); // envia pelo ar (retorna 1 se ok)

  // ── Log no Serial Monitor para debug ────────────────────────────
  const char* alertaStr[] = { "NORMAL","ATENCAO","CRITICO","EMERGENCIA" };
  const char* tipoStr[]   = { "---","ENCHENTE","QUEIMADA" };

  Serial.print(F("[TX] ")); Serial.print(pkt);
  Serial.print(F(" | ")); Serial.println(ok ? F("ENVIADO") : F("FALHA"));
  Serial.print(F("     Agua:")); Serial.print(agua);
  Serial.print(F(" Dist:"));    Serial.print(dist); Serial.print(F("cm"));
  Serial.print(F(" Temp:"));    Serial.print(temp10 / 10);
  Serial.print(F("."));         Serial.print(abs(temp10 % 10));
  Serial.print(F("C Umid:"));   Serial.print(umid10 / 10);
  Serial.print(F("."));         Serial.print(abs(umid10 % 10));
  Serial.print(F("% "));        Serial.print(alertaStr[alerta]);
  Serial.print(F(":"));         Serial.println(tipoStr[tipo]);

  // ── Aguarda o intervalo antes da próxima leitura ─────────────────
  delay(TX_INTERVALO_MS);
}
