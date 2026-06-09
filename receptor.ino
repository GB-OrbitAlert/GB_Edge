// ═══════════════════════════════════════════════════════════════════
//  OrbitAlert — Unidade Receptora (Receptor)
//  Hardware : Arduino Nano + LoRa SX1276 + ST7789 2.0" + Buzzer
//  Global Solution 2026 — FIAP Engenharia de Software
// ═══════════════════════════════════════════════════════════════════
//
//  Bibliotecas necessárias:
//    - "LoRa"             by Sandeep Mistry
//    - "Adafruit ST7789"  by Adafruit
//    - "Adafruit GFX"     by Adafruit
//    - "Adafruit BusIO"   by Adafruit
//
//  LoRa SX1276 e ST7789 compartilham SPI (D11/D12/D13).
//  Cada módulo tem seu próprio CS — sem conflito.
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
//  │  ST7789 2.0"       →   Arduino Nano             │
//  │    VCC             →   3.3V                     │
//  │    GND             →   GND                      │
//  │    SCL  (SCK)      →   D13 (SPI)                │
//  │    SDA  (MOSI)     →   D11 (SPI)                │
//  │    RES  (RST)      →   D6                       │
//  │    DC              →   D7                       │
//  │    CS              →   D8                       │
//  │    BLK             →   3.3V                     │
//  ├──────────────────────────────────────────────────┤
//  │  Buzzer                                          │
//  │    +               →   D3                       │
//  │    -               →   GND                      │
//  └──────────────────────────────────────────────────┘
//
//  ATENÇÃO: LoRa SX1276 e ST7789 operam em 3.3V.
//  O pino 3.3V do Nano fornece ~50mA — suficiente para ambos.
//
//  Pacote recebido: "agua,dist,temp10,umid10,alerta,tipo"
// ═══════════════════════════════════════════════════════════════════

// Biblioteca SPI — barramento compartilhado entre LoRa e ST7789
#include <SPI.h>
// Biblioteca LoRa — controla o módulo SX1276 no modo recepção
#include <LoRa.h>
// Biblioteca GFX — primitivas gráficas (textos, retângulos, linhas)
#include <Adafruit_GFX.h>
// Biblioteca ST7789 — driver do display TFT 240×320
#include <Adafruit_ST7789.h>

// ── Pinos de controle do módulo LoRa SX1276 ──────────────────────────
#define LORA_CS   10   // Chip Select — ativa o LoRa no barramento SPI
#define LORA_RST   9   // Reset do módulo LoRa
#define LORA_DIO0  2   // Interrupção — sinaliza pacote recebido
#define LORA_FREQ  915E6  // Frequência 915 MHz — banda ISM Brasil

// ── Pinos de controle do display ST7789 ──────────────────────────────
#define TFT_CS   8   // Chip Select — ativa o display no barramento SPI
#define TFT_DC   7   // Data/Command — distingue dados de comandos
#define TFT_RST  6   // Reset do display

// ── Pino do buzzer ───────────────────────────────────────────────────
// D3 suporta tone() no Arduino Nano (timer PWM)
#define BUZZER   3

// ── Tempo máximo sem receber sinal antes de exibir aviso ─────────────
#define TIMEOUT_SEM_SINAL  30000UL  // 30 segundos em ms

// ── Paleta de cores RGB565 (formato nativo do display ST7789) ────────
// RGB565: 5 bits vermelho, 6 bits verde, 5 bits azul = 16 bits por cor
#define COR_FUNDO        0x0841   // cinza muito escuro (fundo da tela)
#define COR_HEADER_BG    0x000F   // azul marinho escuro (cabeçalho)
#define COR_HEADER_TEXTO 0x07FF   // ciano claro (título OrbitAlert)
#define COR_HEADER_SUB   0x5AEB   // cinza-azulado (subtítulo FIAP)
#define COR_GRADE        0x2945   // cinza escuro (linhas divisórias)
#define COR_LABEL        0x8C51   // cinza médio (rótulos dos campos)
#define COR_VALOR        0xFFFF   // branco (valores dos sensores)
#define COR_RODAPE_BG    0x10A2   // cinza-azul escuro (rodapé)
#define COR_NORMAL       0x07E0   // verde (alerta NORMAL)
#define COR_ATENCAO      0xFFE0   // amarelo (alerta ATENÇÃO)
#define COR_CRITICO      0xFD20   // laranja (alerta CRÍTICO)
#define COR_EMERGENCIA   0xF800   // vermelho (alerta EMERGÊNCIA)

// ── Layout da tela em portrait (240 × 320 pixels) ────────────────────
//  y=0   ┌─────────────────────────┐
//        │        HEADER           │ h=50px
//  y=50  ├─────────────────────────┤
//        │      BANNER STATUS      │ h=115px
//  y=165 ├──────────┬──────────────┤
//        │ TEMP     │ UMIDADE      │ h=55px
//  y=220 ├──────────┼──────────────┤
//        │ N.ÁGUA   │ DISTÂNCIA    │ h=60px
//  y=280 ├─────────────────────────┤
//        │         RODAPÉ          │ h=40px
//  y=320 └─────────────────────────┘
#define SCR_W    240   // largura do display em pixels
#define SCR_H    320   // altura do display em pixels
#define HEADER_H  50   // altura do cabeçalho
#define BANNER_Y  50   // posição Y onde começa o banner de status
#define BANNER_H 115   // altura do banner de status
#define GRID_Y   165   // posição Y onde começa o grid de sensores
#define GRID_H   115   // altura total do grid
#define FOOTER_Y 280   // posição Y onde começa o rodapé
#define FOOTER_H  40   // altura do rodapé
#define HALF_W   120   // metade da largura (divisão das colunas do grid)

// ── Enumeração dos níveis de alerta ──────────────────────────────────
// uint8_t usa apenas 1 byte — economiza RAM no Arduino Nano
enum EstadoAlerta : uint8_t {
  NORMAL = 0, ATENCAO = 1, CRITICO = 2, EMERGENCIA = 3
};

// ── Variáveis de estado global ────────────────────────────────────────
EstadoAlerta  estadoAtual   = NORMAL;  // nível de alerta atual
bool          buzzerAtivo   = false;   // true = buzzer deve tocar
uint8_t       ulTipo        = 0;       // tipo do último evento (0=---, 1=ENCHENTE, 2=QUEIMADA)
bool          flashState    = false;   // controla piscar do banner na emergência
unsigned long ultimoFlashMs = 0;       // timestamp do último flash
unsigned long ultimoBipMs   = 0;       // timestamp do último bip (modo CRITICO)
unsigned long ultimoRxMs    = 0;       // timestamp do último pacote recebido

// ── Instância do display ST7789 ──────────────────────────────────────
Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// ════════════════════════════════════════════════════════════════════
//  printCentrado()
//  Imprime texto centralizado horizontalmente em uma área do display.
//
//  Parâmetros:
//    txt → string a exibir
//    y   → posição vertical do texto
//    sz  → tamanho da fonte (1=pequeno, 2=médio, 3=grande)
//    cTxt → cor do texto em RGB565
//    cBg  → cor do fundo em RGB565
//    ax, aw → posição X e largura da área de centralização
// ════════════════════════════════════════════════════════════════════
void printCentrado(const char* txt, int y, uint8_t sz,
                   uint16_t cTxt, uint16_t cBg,
                   int ax = 0, int aw = SCR_W) {
  // Calcula X para centralizar: margem esquerda + (área - largura_texto) / 2
  // Cada caractere GFX ocupa 6×sz pixels de largura
  int x = ax + (aw - (int)strlen(txt) * 6 * sz) / 2;
  tft.setTextColor(cTxt, cBg);
  tft.setTextSize(sz);
  tft.setCursor(x, y);
  tft.print(txt);
}

// ════════════════════════════════════════════════════════════════════
//  atualizarCampo()
//  Apaga uma região do display e escreve novo valor centralizado.
//  Usado para atualizar os valores dos sensores sem redesenhar tudo.
// ════════════════════════════════════════════════════════════════════
void atualizarCampo(int rx, int ry, int rw, int rh,
                    const char* txt, uint8_t sz,
                    uint16_t cTxt, uint16_t cBg) {
  // Limpa a área com a cor de fundo antes de escrever o novo valor
  tft.fillRect(rx, ry, rw, rh, cBg);
  // Centraliza o texto horizontal e verticalmente na área
  int x = rx + (rw - (int)strlen(txt) * 6 * sz) / 2;
  int y = ry + (rh - 8 * sz) / 2; // 8×sz = altura do caractere GFX
  tft.setTextColor(cTxt, cBg);
  tft.setTextSize(sz);
  tft.setCursor(x, y);
  tft.print(txt);
}

// ════════════════════════════════════════════════════════════════════
//  corDoEstado()
//  Retorna a cor RGB565 correspondente ao nível de alerta atual.
//  Usada para colorir o banner de status dinamicamente.
// ════════════════════════════════════════════════════════════════════
uint16_t corDoEstado(EstadoAlerta e) {
  switch (e) {
    case EMERGENCIA: return COR_EMERGENCIA; // vermelho
    case CRITICO:    return COR_CRITICO;    // laranja
    case ATENCAO:    return COR_ATENCAO;    // amarelo
    default:         return COR_NORMAL;     // verde
  }
}

// ════════════════════════════════════════════════════════════════════
//  desenharEstrutura()
//  Desenha a interface estática do display — elementos que não mudam:
//  cabeçalho, linhas divisórias, rótulos dos campos e rodapé.
//  Chamada apenas uma vez no setup() para economizar processamento.
// ════════════════════════════════════════════════════════════════════
void desenharEstrutura() {
  // Limpa toda a tela com a cor de fundo escura
  tft.fillScreen(COR_FUNDO);

  // Desenha o cabeçalho azul escuro com o nome do sistema
  tft.fillRect(0, 0, SCR_W, HEADER_H, COR_HEADER_BG);
  printCentrado("OrbitAlert",                  6,  2, COR_HEADER_TEXTO, COR_HEADER_BG);
  printCentrado("Global Solution 2026 - FIAP", 32, 1, COR_HEADER_SUB,   COR_HEADER_BG);

  // Linhas divisórias do grid de sensores
  tft.drawFastHLine(0,      GRID_Y,       SCR_W,  COR_GRADE); // linha superior
  tft.drawFastHLine(0,      GRID_Y + 55,  SCR_W,  COR_GRADE); // linha do meio
  tft.drawFastVLine(HALF_W, GRID_Y,       GRID_H, COR_GRADE); // linha vertical
  tft.drawFastHLine(0,      FOOTER_Y,     SCR_W,  COR_GRADE); // linha do rodapé

  // Rótulos estáticos dos campos de sensores
  tft.setTextColor(COR_LABEL, COR_FUNDO);
  tft.setTextSize(1);
  tft.setCursor(8,          GRID_Y + 6);  tft.print(F("TEMPERATURA"));
  tft.setCursor(8,          GRID_Y + 61); tft.print(F("NIVEL AGUA"));
  tft.setCursor(HALF_W + 8, GRID_Y + 6);  tft.print(F("UMIDADE"));
  tft.setCursor(HALF_W + 8, GRID_Y + 61); tft.print(F("DISTANCIA"));

  // Preenche o rodapé com sua cor de fundo
  tft.fillRect(0, FOOTER_Y, SCR_W, FOOTER_H, COR_RODAPE_BG);
}

// ════════════════════════════════════════════════════════════════════
//  atualizarBanner()
//  Redesenha o banner central de status com a cor e texto do alerta.
//  No nível EMERGÊNCIA, exibe um triângulo de aviso (⚠).
//  O parâmetro flash=true inverte as cores para criar o efeito piscar.
// ════════════════════════════════════════════════════════════════════
void atualizarBanner(EstadoAlerta estado, uint8_t tipo, bool flash = false) {
  const char* tipoStr[] = { "---", "ENCHENTE", "QUEIMADA" };

  // Define as cores do banner de acordo com o nível de alerta
  uint16_t bg = corDoEstado(estado);
  // Na atenção (fundo amarelo), o texto é preto para melhor contraste
  uint16_t fg = (estado == ATENCAO) ? (uint16_t)0x0000 : (uint16_t)0xFFFF;

  // Modo flash: inverte para preto/vermelho (efeito piscante da emergência)
  if (flash && estado == EMERGENCIA) { bg = 0x0000; fg = COR_EMERGENCIA; }

  // Preenche toda a área do banner com a cor do alerta
  tft.fillRect(0, BANNER_Y, SCR_W, BANNER_H, bg);

  // Na emergência, desenha o triângulo de aviso (⚠) manualmente
  if (estado == EMERGENCIA) {
    // Triângulo principal
    tft.fillTriangle(120, BANNER_Y + 10,
                      95, BANNER_Y + 48,
                     145, BANNER_Y + 48, fg);
    // Haste do "!" (retângulo sobre o fundo para criar o negativo)
    tft.fillRect(117, BANNER_Y + 22, 6, 14, bg);
    // Ponto do "!" (retângulo menor abaixo)
    tft.fillRect(117, BANNER_Y + 38, 6,  6, bg);
  }

  // Seleciona o texto do nível de alerta
  const char* nivelTxt;
  switch (estado) {
    case EMERGENCIA: nivelTxt = "EMERGENCIA"; break;
    case CRITICO:    nivelTxt = "CRITICO";    break;
    case ATENCAO:    nivelTxt = "ATENCAO";    break;
    default:         nivelTxt = "NORMAL";     break;
  }

  // Na emergência o texto fica mais abaixo (por causa do triângulo)
  int tY = (estado == EMERGENCIA) ? BANNER_Y + 56 : BANNER_Y + 38;
  printCentrado(nivelTxt, tY, 2, fg, bg);

  // Exibe o tipo do evento entre colchetes: [ ENCHENTE ] ou [ QUEIMADA ]
  char sub[20];
  snprintf(sub, sizeof(sub), "[ %s ]", tipoStr[tipo]);
  printCentrado(sub, tY + 26, 2, fg, bg);

  // Linha decorativa na base do banner
  tft.drawFastHLine(20, BANNER_Y + BANNER_H - 8, SCR_W - 40, fg);
}

// ════════════════════════════════════════════════════════════════════
//  atualizarSensores()
//  Atualiza os 4 campos do grid com os valores recebidos do transmissor.
//  Temperatura e umidade chegam ×10 (ex: 395 = 39.5°C) para evitar float.
// ════════════════════════════════════════════════════════════════════
void atualizarSensores(int agua, int dist, int temp10, int umid10) {
  char buf[14]; // buffer para formatar o texto de cada campo
  int vy1 = GRID_Y + 20; // y do valor na linha 1 do grid
  int vy2 = GRID_Y + 76; // y do valor na linha 2 do grid

  // Temperatura — converte inteiro ×10 para "XX.X *C"
  if (temp10 == -999) snprintf(buf, sizeof(buf), "ERRO");
  else snprintf(buf, sizeof(buf), "%d.%d *C", temp10/10, abs(temp10%10));
  atualizarCampo(2, vy1, HALF_W-4, 28, buf, 2, COR_VALOR, COR_FUNDO);

  // Umidade — converte inteiro ×10 para "XX.X %"
  if (umid10 == -999) snprintf(buf, sizeof(buf), "ERRO");
  else snprintf(buf, sizeof(buf), "%d.%d %%", umid10/10, abs(umid10%10));
  atualizarCampo(HALF_W+2, vy1, HALF_W-4, 28, buf, 2, COR_VALOR, COR_FUNDO);

  // Nível de água — valor ADC direto (0–1023)
  snprintf(buf, sizeof(buf), "%d", agua);
  atualizarCampo(2, vy2, HALF_W-4, 28, buf, 2, COR_VALOR, COR_FUNDO);

  // Distância HC-SR04 — exibe "---" se houver erro de leitura (-1)
  (dist < 0) ? snprintf(buf, sizeof(buf), "---")
             : snprintf(buf, sizeof(buf), "%d cm", dist);
  atualizarCampo(HALF_W+2, vy2, HALF_W-4, 28, buf, 2, COR_VALOR, COR_FUNDO);
}

// ════════════════════════════════════════════════════════════════════
//  atualizarRodape()
//  Atualiza o rodapé com a frequência LoRa e o RSSI do último pacote.
//  RSSI (Received Signal Strength Indicator) indica a força do sinal
//  em dBm — quanto mais próximo de zero, melhor o sinal.
// ════════════════════════════════════════════════════════════════════
void atualizarRodape(int rssi) {
  // Limpa o rodapé antes de redesenhar
  tft.fillRect(0, FOOTER_Y+1, SCR_W, FOOTER_H-1, COR_RODAPE_BG);
  char buf[18];
  snprintf(buf, sizeof(buf), "LoRa 915MHz %ddBm", rssi);
  printCentrado(buf, FOOTER_Y + 14, 1, COR_LABEL, COR_RODAPE_BG);
}

// ════════════════════════════════════════════════════════════════════
//  telaSplash()
//  Exibe a tela de inicialização com barra de progresso animada.
//  Chamada no setup() enquanto o LoRa ainda está inicializando.
// ════════════════════════════════════════════════════════════════════
void telaSplash() {
  tft.fillScreen(0x000F); // fundo azul escuro
  printCentrado("OrbitAlert",         85, 3, 0x07FF, 0x000F);
  printCentrado("v2.0",              121, 2, 0x9CF3, 0x000F);
  printCentrado("Unidade Receptora", 148, 1, 0x5AEB, 0x000F);
  printCentrado("FIAP GS 2026",      162, 1, 0x5AEB, 0x000F);

  // Barra de progresso animada — indica que o sistema está carregando
  int bx=20, by=210, bw=200, bh=10;
  tft.drawRect(bx, by, bw, bh, 0x5AEB); // contorno da barra
  for (int i=0; i<=bw-4; i+=8) {        // preenche progressivamente
    tft.fillRect(bx+2, by+2, i, bh-4, 0x07FF);
    delay(20);
  }
}

// ════════════════════════════════════════════════════════════════════
//  processarPacote()
//  Recebe o pacote CSV do LoRa, faz o parse dos campos e atualiza
//  o display, o estado do buzzer e o estado global do sistema.
//
//  Formato esperado: "agua,dist,temp10,umid10,alerta,tipo"
//  Usa strtok para dividir o CSV — evita String para poupar RAM.
// ════════════════════════════════════════════════════════════════════
void processarPacote(const char* pkt, int rssi) {
  // Copia para buffer local — strtok modifica a string original
  char tmp[48];
  strncpy(tmp, pkt, 47); tmp[47] = '\0';

  // Divide o CSV em 6 campos usando vírgula como delimitador
  char* p[6];
  p[0] = strtok(tmp, ",");
  for (int i = 1; i < 6; i++) {
    p[i] = strtok(NULL, ",\n\r"); // continua de onde parou
    if (!p[i]) {
      // Se algum campo faltar, o pacote está corrompido — ignora
      Serial.println(F("[RX] Pacote invalido."));
      return;
    }
  }

  // Converte os campos de string para os tipos corretos
  int     agua   = atoi(p[0]); // nível de água ADC (0–1023)
  int     dist   = atoi(p[1]); // distância em cm (-1 = erro)
  int     temp10 = atoi(p[2]); // temperatura × 10
  int     umid10 = atoi(p[3]); // umidade × 10
  uint8_t alerta = (uint8_t)atoi(p[4]); // 0–3
  uint8_t tipo   = (uint8_t)atoi(p[5]); // 0–2

  const char* alertaStr[] = { "NORMAL","ATENCAO","CRITICO","EMERGENCIA" };
  const char* tipoStr[]   = { "---","ENCHENTE","QUEIMADA" };

  // Log no Serial Monitor para debug
  Serial.print(F("[RX] ")); Serial.print(pkt);
  Serial.print(F(" RSSI:")); Serial.println(rssi);
  Serial.print(F("     ")); Serial.print(alertaStr[alerta]);
  Serial.print(F(":")); Serial.println(tipoStr[tipo]);

  // Proteção contra valores fora do intervalo (pacote corrompido)
  if (alerta > 3) alerta = 0;
  if (tipo   > 2) tipo   = 0;

  // Atualiza o estado global do sistema
  estadoAtual = (EstadoAlerta)alerta;
  ulTipo      = tipo;
  buzzerAtivo = (estadoAtual >= CRITICO); // buzzer só em CRITICO ou EMERGENCIA
  flashState  = false;       // reinicia o ciclo de flash ao mudar de estado
  ultimoRxMs  = millis();    // registra o momento do recebimento

  // Atualiza a interface do display com os novos dados
  atualizarBanner(estadoAtual, ulTipo);
  atualizarSensores(agua, dist, temp10, umid10);
  atualizarRodape(rssi);
}

// ════════════════════════════════════════════════════════════════════
//  setup()
//  Executado uma única vez ao ligar o Arduino.
//  Inicializa o display, o módulo LoRa e desenha a interface base.
// ════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(9600);
  Serial.println(F("OrbitAlert — Receptor (Nano + LoRa + ST7789)"));

  // Configura o buzzer como saída e garante silêncio inicial
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);

  // Inicializa o display ST7789 (240×320, portrait)
  tft.init(240, 320);
  tft.setRotation(0); // 0 = portrait padrão

  // Exibe splash enquanto o LoRa inicializa
  telaSplash();
  delay(400);

  // Informa os pinos de controle ao módulo LoRa
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  // Inicializa o LoRa — se falhar, exibe erro no display e trava
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println(F("[ERRO] LoRa nao inicializado!"));
    tft.fillScreen(COR_EMERGENCIA);
    printCentrado("ERRO CRITICO",   120, 2, 0xFFFF, COR_EMERGENCIA);
    printCentrado("LoRa ausente",   150, 2, 0xFFFF, COR_EMERGENCIA);
    while (true) delay(1000); // trava — requer reinicialização
  }

  // Parâmetros LoRa — DEVEM ser idênticos ao transmissor
  LoRa.setSpreadingFactor(10);    // SF10 — alcance x velocidade
  LoRa.setSignalBandwidth(125E3); // 125 kHz — padrão LoRaWAN
  LoRa.setCodingRate4(5);         // CR 4/5 — redundância mínima
  // Nota: setTxPower não é necessário no receptor

  Serial.println(F("[OK] LoRa 915 MHz | SF10 | 125kHz | CR4/5"));

  // Desenha a estrutura estática da interface
  desenharEstrutura();
  // Exibe o estado NORMAL como padrão inicial
  atualizarBanner(NORMAL, 0);
  // Exibe zeros nos campos de sensores enquanto aguarda o primeiro pacote
  atualizarSensores(0, -1, 0, 0);
  atualizarRodape(0);

  // Registra o momento de início para o controle de timeout
  ultimoRxMs = millis();
  Serial.println(F("[OK] Aguardando pacotes LoRa...\n"));
}

// ════════════════════════════════════════════════════════════════════
//  loop()
//  Executado continuamente após o setup().
//  Verifica pacotes LoRa, gerencia o efeito flash e controla o buzzer.
// ════════════════════════════════════════════════════════════════════
void loop() {
  // ── Verifica se chegou um pacote LoRa ───────────────────────────
  // parsePacket() retorna o tamanho do pacote disponível (0 = nenhum)
  int tamanho = LoRa.parsePacket();
  if (tamanho > 0) {
    // Lê os bytes do pacote para um buffer local
    char pkt[48];
    int  idx = 0;
    while (LoRa.available() && idx < 47) pkt[idx++] = (char)LoRa.read();
    pkt[idx] = '\0'; // garante terminador de string

    // Processa o pacote e atualiza display/estado/buzzer
    processarPacote(pkt, LoRa.packetRssi());
  }

  // ── Efeito flash do banner na Emergência ─────────────────────────
  // Alterna as cores do banner a cada 500 ms para chamar atenção
  if (estadoAtual == EMERGENCIA && millis() - ultimoFlashMs > 500UL) {
    ultimoFlashMs = millis();
    flashState    = !flashState; // inverte o estado do flash
    atualizarBanner(EMERGENCIA, ulTipo, flashState);
  }

  // ── Controle do buzzer ───────────────────────────────────────────
  if (buzzerAtivo) {
    if (estadoAtual == EMERGENCIA) {
      // EMERGÊNCIA: buzzer toca continuamente em 1000 Hz
      tone(BUZZER, 1000);
    } else if (estadoAtual == CRITICO && millis() - ultimoBipMs > 2000UL) {
      // CRÍTICO: bip curto (150 ms) a cada 2 segundos
      tone(BUZZER, 800, 150);
      ultimoBipMs = millis();
    }
  } else {
    // NORMAL ou ATENÇÃO: buzzer silencioso
    noTone(BUZZER);
  }

  // ── Timeout — sem sinal por mais de 30 segundos ──────────────────
  // Se o transmissor parou de enviar, exibe aviso no display
  if (millis() - ultimoRxMs > TIMEOUT_SEM_SINAL) {
    Serial.println(F("[AVISO] Sem sinal ha 30s."));
    estadoAtual = ATENCAO; // ativa ATENÇÃO visual sem acionar buzzer
    buzzerAtivo = false;
    // Redesenha o banner com mensagem de aviso
    tft.fillRect(0, BANNER_Y, SCR_W, BANNER_H, COR_GRADE);
    printCentrado("SEM SINAL",         BANNER_Y + 38, 2, COR_ATENCAO, COR_GRADE);
    printCentrado("[ TRANSMISSOR ]",   BANNER_Y + 64, 2, COR_ATENCAO, COR_GRADE);
    printCentrado("Verifique o radio", BANNER_Y + 92, 1, COR_LABEL,   COR_GRADE);
    // Reinicia o timer para não repetir o aviso a cada ciclo
    ultimoRxMs = millis();
  }

  // Pequena pausa para não sobrecarregar o processador
  delay(10);
}
