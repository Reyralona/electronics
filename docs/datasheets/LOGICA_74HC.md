# Datasheets oficiais — chips 74HC (pinagem DIP e comportamento ideal)

Consultado em 20/11/2026. Todas as fontes são PDFs oficiais baixados de ti.com (e onsemi.com para confirmação de duas pinagens). Texto extraído com `pdftotext`; quando a figura de pinagem era vetorial (sem texto), a página foi renderizada e lida visualmente (74HC85, 74HC4511, 74HC374).

Convenções:
- `~NOME` = pino ativo em nível baixo (barra no datasheet).
- **Fato** = está no datasheet citado. **Inferido** = dedução minha (ex.: frações de VCC, ordem de prioridade não escrita, observação de modelo); sempre marcado.
- H = alto, L = baixo, X = indiferente, ↑ = borda de subida (L→H), Z = alta impedância, Q0 = estado anterior.

---

## 0. Limites da família 74HC (Absolute Maximum Ratings e condições recomendadas)

Fonte de referência: TI SN74HC04, SCLS078H (rev. abril 2021) — https://www.ti.com/lit/ds/symlink/sn74hc04.pdf — consultado em 20/11/2026.

| Parâmetro | Mín | Máx | Unidade | Fato |
|---|---|---|---|---|
| VCC (abs. max) | -0,5 | 7 | V | sim |
| IIK corrente de clamp na entrada (VI < 0 ou VI > VCC) | | ±20 | mA | sim |
| IOK corrente de clamp na saída (VO < 0 ou VO > VCC) | | ±20 | mA | sim |
| IO corrente contínua por saída (VO = 0 a VCC) | | ±25 | mA | sim |
| Corrente contínua por VCC ou GND (total do chip) | | ±50 | mA | sim |
| TJ | | 150 | °C | sim |
| Tstg | -65 | 150 | °C | sim |
| VCC recomendado | 2 | 6 (nom. 5) | V | sim |
| VIH mín @ VCC 2 / 4,5 / 6 V | 1,5 / 3,15 / 4,2 | | V | sim |
| VIL máx @ VCC 2 / 4,5 / 6 V | | 0,5 / 1,35 / 1,8 | V | sim |
| VI, VO | 0 | VCC | V | sim |

Nota do datasheet (fato): "as tensões de entrada/saída podem ser excedidas se os limites de corrente de entrada/saída forem respeitados".

**Variações por chip (fato, de cada datasheet):** a classe "bus driver / saída de alta corrente" tem IO = ±35 mA e VCC/GND = ±70 mA: **74HC125, 74HC151, 74HC157, 74HC244, 74HC373, 74HC374, 74HC595**. Todos os demais desta lista seguem ±25 mA / ±50 mA, inclusive o **CD74HC4511** (ver seção do 4511) e o **CD74HC85** (rotulado "output source or sink current per output pin ±25 mA", ICC ±50 mA). VCC abs. max -0,5..7 V e IIK/IOK ±20 mA são iguais em todos.

Corrente de saída de teste (fato, tabelas de VOH/VOL): saídas padrão ±4 mA @ 4,5 V (±5,2 mA @ 6 V); saídas de barramento ±6 mA @ 4,5 V (±7,8 mA @ 6 V) — ex.: QA..QH do 595; QH' do 595 é saída padrão (4 mA).

---

## 1. 74HC02 — 4 portas NOR de 2 entradas

Fonte: TI SN74HC02, SCLS076G (dez/2020) — https://www.ti.com/lit/ds/symlink/sn74hc02.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | 1Y | Saída da porta 1 |
| 2 | 1A | Entrada A da porta 1 |
| 3 | 1B | Entrada B da porta 1 |
| 4 | 2Y | Saída da porta 2 |
| 5 | 2A | Entrada A da porta 2 |
| 6 | 2B | Entrada B da porta 2 |
| 7 | GND | Terra |
| 8 | 3A | Entrada A da porta 3 |
| 9 | 3B | Entrada B da porta 3 |
| 10 | 3Y | Saída da porta 3 |
| 11 | 4A | Entrada A da porta 4 |
| 12 | 4B | Entrada B da porta 4 |
| 13 | 4Y | Saída da porta 4 |
| 14 | VCC | Alimentação positiva |

Função (fato): Y = NOT(A OR B). Atenção: no 02 a saída vem ANTES das entradas (pinos 1, 4) e na metade de cima as entradas vêm antes da saída (10, 13) — diferente do 00/08.
Abs. max: família (±25/±50 mA).

---

## 2. 74HC04 — 6 inversores

Fonte: TI SN74HC04, SCLS078H — https://www.ti.com/lit/ds/symlink/sn74hc04.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | 1A | Entrada do inversor 1 |
| 2 | 1Y | Saída do inversor 1 |
| 3 | 2A | Entrada do inversor 2 |
| 4 | 2Y | Saída do inversor 2 |
| 5 | 3A | Entrada do inversor 3 |
| 6 | 3Y | Saída do inversor 3 |
| 7 | GND | Terra |
| 8 | 4Y | Saída do inversor 4 |
| 9 | 4A | Entrada do inversor 4 |
| 10 | 5Y | Saída do inversor 5 |
| 11 | 5A | Entrada do inversor 5 |
| 12 | 6Y | Saída do inversor 6 |
| 13 | 6A | Entrada do inversor 6 |
| 14 | VCC | Alimentação positiva |

Função (fato): Y = NOT A. Abs. max: família.

---

## 3. 74HC10 — 3 portas NAND de 3 entradas

Fonte: TI SN74HC10, SCLS083E (abr/2021) — https://www.ti.com/lit/ds/symlink/sn74hc10.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | 1A | Entrada A da porta 1 |
| 2 | 1B | Entrada B da porta 1 |
| 3 | 2A | Entrada A da porta 2 |
| 4 | 2B | Entrada B da porta 2 |
| 5 | 2C | Entrada C da porta 2 |
| 6 | 2Y | Saída da porta 2 |
| 7 | GND | Terra |
| 8 | 3Y | Saída da porta 3 |
| 9 | 3A | Entrada A da porta 3 |
| 10 | 3B | Entrada B da porta 3 |
| 11 | 3C | Entrada C da porta 3 |
| 12 | 1Y | Saída da porta 1 |
| 13 | 1C | Entrada C da porta 1 |
| 14 | VCC | Alimentação positiva |

Função (fato): Y = NOT(A AND B AND C). Note que a porta 1 está dividida: entradas 1, 2, 13 e saída 12. Abs. max: família.

---

## 4. 74HC11 — 3 portas AND de 3 entradas

Fonte: TI SN74HC11, SCLS084E (abr/2021) — https://www.ti.com/lit/ds/symlink/sn74hc11.pdf — consultado em 20/11/2026.

Pinagem idêntica à do 74HC10 (fato: mesma figura): 1 1A, 2 1B, 3 2A, 4 2B, 5 2C, 6 2Y, 7 GND, 8 3Y, 9 3A, 10 3B, 11 3C, 12 1Y, 13 1C, 14 VCC. Descrições como no 74HC10, trocando "NAND" por "AND".

Função (fato): Y = A AND B AND C. Abs. max: família.

---

## 5. 74HC14 — 6 inversores Schmitt-trigger

Fonte: TI SN74HC14, SCLS085L (fev/2025) — https://www.ti.com/lit/ds/symlink/sn74hc14.pdf — consultado em 20/11/2026.

Pinagem idêntica à do 74HC04 (fato): 1 1A, 2 1Y, 3 2A, 4 2Y, 5 3A, 6 3Y, 7 GND, 8 4Y, 9 4A, 10 5Y, 11 5A, 12 6Y, 13 6A, 14 VCC. Entrada "nA": entrada com histerese do inversor n; "nY": saída invertida.

Função (fato): Y = NOT A, com limiares de comutação com histerese. Limiares (fato, TA = 25 °C, mín / típ / máx):

| VCC | VT+ (subida) | VT- (descida) | Histerese VT+ − VT- |
|---|---|---|---|
| 2 V | 0,7 / 1,2 / 1,5 V | 0,3 / 0,6 / 1,0 V | 0,2 / 0,6 / 1,2 V |
| 4,5 V | 1,55 / 2,5 / 3,13 V | 0,9 / 1,6 / 2,45 V | 0,4 / 0,9 / 2,1 V |
| 6 V | 2,1 / 3,3 / 4,2 V | 1,2 / 2,0 / 3,2 V | 0,5 / 1,3 / 2,5 V |

**Inferido (o datasheet não dá 5 V):** valores típicos como fração de VCC: 4,5 V → VT+ ≈ 0,556·VCC, VT- ≈ 0,356·VCC; 6 V → 0,55·VCC e 0,333·VCC. Para modelo ideal a 5 V: **VT+ ≈ 0,55·VCC (2,75 V), VT- ≈ 0,35·VCC (1,75 V), histerese ≈ 0,2·VCC (1,0 V)**. Comportamento: saída vai a L quando a entrada sobe acima de VT+; volta a H quando a entrada cai abaixo de VT-; entre os dois, mantém o estado.
Abs. max: família.

---

## 6. 74HC74 — 2 flip-flops D com preset e clear

Fonte: TI SN74HC74, SCLS094F (jun/2021) — https://www.ti.com/lit/ds/symlink/sn74hc74.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | ~1CLR | Clear assíncrono do FF1: em L força Q=L |
| 2 | 1D | Dado do FF1: copiado na borda de subida do clock |
| 3 | 1CLK | Clock do FF1: captura D na borda de subida |
| 4 | ~1PRE | Preset assíncrono do FF1: em L força Q=H |
| 5 | 1Q | Saída do FF1 |
| 6 | ~1Q | Saída invertida do FF1 |
| 7 | GND | Terra |
| 8 | ~2Q | Saída invertida do FF2 |
| 9 | 2Q | Saída do FF2 |
| 10 | ~2PRE | Preset assíncrono do FF2: em L força Q=H |
| 11 | 2CLK | Clock do FF2: captura D na borda de subida |
| 12 | 2D | Dado do FF2 |
| 13 | ~2CLR | Clear assíncrono do FF2: em L força Q=L |
| 14 | VCC | Alimentação positiva |

Tabela de função (fato):

| ~PRE | ~CLR | CLK | D | Q | ~Q |
|---|---|---|---|---|---|
| L | H | X | X | H | L |
| H | L | X | X | L | H |
| L | L | X | X | H* | H* |
| H | H | ↑ | H | H | L |
| H | H | ↑ | L | L | H |
| H | H | L | X | Q0 | ~Q0 |

\* Fato: "configuração não estável; não persiste quando PRE ou CLR voltam ao nível inativo" (ambas as saídas em H).
Abs. max: família.

---

## 7. 74HC273 — 8 flip-flops D com clear

Fonte: TI SN74HC273, SCLS136F (abr/2022) — https://www.ti.com/lit/ds/symlink/sn74hc273.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | ~CLR | Clear assíncrono: em L zera todas as saídas |
| 2 | 1Q | Saída 1 |
| 3 | 1D | Entrada de dado 1 |
| 4 | 2D | Entrada de dado 2 |
| 5 | 2Q | Saída 2 |
| 6 | 3Q | Saída 3 |
| 7 | 3D | Entrada de dado 3 |
| 8 | 4D | Entrada de dado 4 |
| 9 | 4Q | Saída 4 |
| 10 | GND | Terra |
| 11 | CLK | Clock comum: carrega os 8 dados na borda de subida |
| 12 | 5Q | Saída 5 |
| 13 | 5D | Entrada de dado 5 |
| 14 | 6D | Entrada de dado 6 |
| 15 | 6Q | Saída 6 |
| 16 | 7Q | Saída 7 |
| 17 | 7D | Entrada de dado 7 |
| 18 | 8D | Entrada de dado 8 |
| 19 | 8Q | Saída 8 |
| 20 | VCC | Alimentação positiva |

Tabela (fato): ~CLR=L → Q=L (independe do clock: assíncrono); ~CLR=H e CLK ↑ → Q=D; ~CLR=H e CLK=L → Q0. Saídas totem-pole, sem 3-state.
Abs. max: família (±25/±50 mA).

---

## 8. 74HC373 — 8 latches D transparentes, saídas 3-state

Fonte: TI SN74HC373, SCLS140G (fev/2025) — https://www.ti.com/lit/ds/symlink/sn74hc373.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | ~OE | Habilita saídas: em L saídas ativas; em H alta impedância |
| 2 | 1Q | Saída 1 (3-state) |
| 3 | 1D | Entrada de dado 1 |
| 4 | 2D | Entrada de dado 2 |
| 5 | 2Q | Saída 2 |
| 6 | 3Q | Saída 3 |
| 7 | 3D | Entrada de dado 3 |
| 8 | 4D | Entrada de dado 4 |
| 9 | 4Q | Saída 4 |
| 10 | GND | Terra |
| 11 | LE | Habilita latch: em H as saídas seguem D; em L retêm |
| 12 | 5Q | Saída 5 |
| 13 | 5D | Entrada de dado 5 |
| 14 | 6D | Entrada de dado 6 |
| 15 | 6Q | Saída 6 |
| 16 | 7Q | Saída 7 |
| 17 | 7D | Entrada de dado 7 |
| 18 | 8D | Entrada de dado 8 |
| 19 | 8Q | Saída 8 |
| 20 | VCC | Alimentação positiva |

Tabela (fato): ~OE=L, LE=H → Q=D (transparente); ~OE=L, LE=L → Q0 (retém o valor presente quando LE caiu); ~OE=H → Z.
**Inferido** (padrão da família, consistente com o diagrama): ~OE não afeta o conteúdo do latch — com ~OE=H o latch continua capturando/retendo, só as saídas ficam em Z.
Abs. max: **IO ±35 mA, VCC/GND ±70 mA** (fato).

---

## 9. 74HC374 — 8 flip-flops D, saídas 3-state

Fontes: TI SN74HC374, SCLS141G (abr/2022) — https://www.ti.com/lit/ds/symlink/sn74hc374.pdf (pinagem lida da figura renderizada); confirmada em onsemi MC74HC374A — https://www.onsemi.com/download/data-sheet/pdf/mc74hc374a-d.pdf — ambos consultados em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | ~OE | Habilita saídas: em L ativas; em H alta impedância |
| 2 | 1Q | Saída 1 (3-state) |
| 3 | 1D | Entrada de dado 1 |
| 4 | 2D | Entrada de dado 2 |
| 5 | 2Q | Saída 2 |
| 6 | 3Q | Saída 3 |
| 7 | 3D | Entrada de dado 3 |
| 8 | 4D | Entrada de dado 4 |
| 9 | 4Q | Saída 4 |
| 10 | GND | Terra |
| 11 | CLK | Clock: carrega os 8 dados na borda de subida |
| 12 | 5Q | Saída 5 |
| 13 | 5D | Entrada de dado 5 |
| 14 | 6D | Entrada de dado 6 |
| 15 | 6Q | Saída 6 |
| 16 | 7Q | Saída 7 |
| 17 | 7D | Entrada de dado 7 |
| 18 | 8D | Entrada de dado 8 |
| 19 | 8Q | Saída 8 |
| 20 | VCC | Alimentação positiva |

Tabela (fato, TI): ~OE=L, CLK ↑ → Q=D; ~OE=L, CLK H ou L → Q0; ~OE=H → Z.
Fato (onsemi): "Output Enable não afeta o estado dos flip-flops; com OE alto todas as saídas ficam em alta impedância; os dados podem ser armazenados mesmo com saídas desabilitadas". Sem clear.
Abs. max: **±35 mA / ±70 mA** (fato).

---

## 10. 74HC163 — contador binário síncrono de 4 bits, clear síncrono

Fonte: TI SN74HC163, SCLS298D (out/2003) — https://www.ti.com/lit/ds/symlink/sn74hc163.pdf — consultado em 20/11/2026. (Conferido com TI CD74HC163 — https://www.ti.com/lit/ds/symlink/cd74hc163.pdf, que usa nomes MR/PE/TE/SPE/TC.)

| Pino | Nome | Descrição |
|---|---|---|
| 1 | ~CLR | Clear síncrono: em L zera na próxima borda de subida |
| 2 | CLK | Clock: conta/carrega na borda de subida |
| 3 | A | Dado de carga, bit 0 (LSB) |
| 4 | B | Dado de carga, bit 1 |
| 5 | C | Dado de carga, bit 2 |
| 6 | D | Dado de carga, bit 3 (MSB) |
| 7 | ENP | Habilita contagem (paralelo): ambos ENP e ENT em H para contar |
| 8 | GND | Terra |
| 9 | ~LOAD | Carga síncrona: em L copia A–D na próxima borda de subida |
| 10 | ENT | Habilita contagem e também habilita o RCO (cascata) |
| 11 | QD | Saída bit 3 (MSB) |
| 12 | QC | Saída bit 2 |
| 13 | QB | Saída bit 1 |
| 14 | QA | Saída bit 0 (LSB) |
| 15 | RCO | Carry de saída: H quando contagem = 15 e ENT=H |
| 16 | VCC | Alimentação positiva |

Comportamento (fato): tudo na borda de subida de CLK. ~CLR=L → Q=0000 na próxima ↑ "independente das entradas de habilitação". ~LOAD=L → Q=DCBA na próxima ↑ "independente dos enables". ~CLR=H, ~LOAD=H, ENP=ENT=H → conta +1 (15 → 0). ENP ou ENT em L → mantém. RCO = ENT AND (contagem = 15) — é combinacional (pulso H enquanto a contagem é máxima). Mudanças em ENP/ENT/~LOAD não afetam o conteúdo até a borda de clock.
**Inferido:** prioridade ~CLR > ~LOAD > contagem (da tabela do CD74HC163: Reset tem Pn/PE/TE = X; Load tem PE/TE = X).
Abs. max: família (±25/±50 mA).

---

## 11. 74HC193 — contador up/down síncrono de 4 bits, dois clocks, clear

Fonte: TI SN74HC193, SCLS122D (out/2003) — https://www.ti.com/lit/ds/symlink/sn74hc193.pdf — consultado em 20/11/2026. (Tabela verdade conferida em TI CD74HC193 — https://www.ti.com/lit/ds/symlink/cd74hc193.pdf.)

| Pino | Nome | Descrição |
|---|---|---|
| 1 | B | Dado de carga, bit 1 |
| 2 | QB | Saída bit 1 |
| 3 | QA | Saída bit 0 (LSB) |
| 4 | DOWN | Clock de descida: decrementa na borda de subida (UP em H) |
| 5 | UP | Clock de subida: incrementa na borda de subida (DOWN em H) |
| 6 | QC | Saída bit 2 |
| 7 | QD | Saída bit 3 (MSB) |
| 8 | GND | Terra |
| 9 | D | Dado de carga, bit 3 (MSB) |
| 10 | C | Dado de carga, bit 2 |
| 11 | ~LOAD | Carga assíncrona: em L as saídas copiam A–D imediatamente |
| 12 | ~CO | Carry (vai-um): L enquanto contagem = 15 e UP = L |
| 13 | ~BO | Borrow (empresta): L enquanto contagem = 0 e DOWN = L |
| 14 | CLR | Clear assíncrono ATIVO EM ALTO: em H zera tudo |
| 15 | A | Dado de carga, bit 0 (LSB) |
| 16 | VCC | Alimentação positiva |

Comportamento (fato): contagem na transição L→H de UP (com DOWN em H) ou de DOWN (com UP em H). CLR em H força saídas em L, "independente das entradas de contagem e LOAD" (tem prioridade). ~LOAD=L (com CLR=L) carrega A–D "independentemente dos pulsos de contagem" — assíncrono. ~CO produz pulso L enquanto contagem é máxima (15) e UP está em L; ~BO produz pulso L enquanto contagem é 0 e DOWN está em L. Cascata: ~BO → DOWN e ~CO → UP do próximo estágio.
**Inferido:** como ~CO = NOT(Q=15 AND UP=L), a borda de subida de UP que leva 15→0 também faz ~CO subir — é essa borda que incrementa o estágio seguinte. Idem ~BO no 0→15.
**Surpresa:** CLR deste chip é ativo em ALTO (diferente do 163).
Abs. max: família.

---

## 12. 74HC138 — decodificador/demultiplexador 3→8

Fonte: TI SN74HC138, SCLS107G (out/2021) — https://www.ti.com/lit/ds/symlink/sn74hc138.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | A | Seleção, bit 0 (LSB) |
| 2 | B | Seleção, bit 1 |
| 3 | C | Seleção, bit 2 (MSB) |
| 4 | ~G2A | Habilitação ativa em L (A) |
| 5 | ~G2B | Habilitação ativa em L (B) |
| 6 | G1 | Habilitação ativa em H |
| 7 | ~Y7 | Saída 7: L quando selecionada |
| 8 | GND | Terra |
| 9 | ~Y6 | Saída 6 |
| 10 | ~Y5 | Saída 5 |
| 11 | ~Y4 | Saída 4 |
| 12 | ~Y3 | Saída 3 |
| 13 | ~Y2 | Saída 2 |
| 14 | ~Y1 | Saída 1 |
| 15 | ~Y0 | Saída 0 |
| 16 | VCC | Alimentação positiva |

Tabela (fato): se G1=L ou ~G2A=H ou ~G2B=H → todas as saídas H. Com G1=H, ~G2A=~G2B=L → apenas ~Y(n) = L, n = 4C + 2B + A. Combinacional.
Abs. max: família.

---

## 13. 74HC139 — 2 decodificadores 2→4

Fontes: TI SN74HC139, SCLS108E (fev/2022) — https://www.ti.com/lit/ds/symlink/sn74hc139.pdf (descrição); pinagem e tabela em TI CD74HC139 — https://www.ti.com/lit/ds/symlink/cd74hc139.pdf; pinagem confirmada em onsemi MC74HC139A — https://www.onsemi.com/download/data-sheet/pdf/mc74hc139a-d.pdf. Consultados em 20/11/2026. (A figura do SN74HC139 é vetorial, sem texto; nomes abaixo seguem o padrão TI SN: G, A, B, Y0–Y3.)

| Pino | Nome | Descrição |
|---|---|---|
| 1 | ~1G | Habilitação do decodificador 1 (ativa em L) |
| 2 | 1A | Seleção bit 0 do decodificador 1 |
| 3 | 1B | Seleção bit 1 do decodificador 1 |
| 4 | ~1Y0 | Saída 0 do dec. 1 (L quando selecionada) |
| 5 | ~1Y1 | Saída 1 do dec. 1 |
| 6 | ~1Y2 | Saída 2 do dec. 1 |
| 7 | ~1Y3 | Saída 3 do dec. 1 |
| 8 | GND | Terra |
| 9 | ~2Y3 | Saída 3 do dec. 2 |
| 10 | ~2Y2 | Saída 2 do dec. 2 |
| 11 | ~2Y1 | Saída 1 do dec. 2 |
| 12 | ~2Y0 | Saída 0 do dec. 2 |
| 13 | 2B | Seleção bit 1 do decodificador 2 |
| 14 | 2A | Seleção bit 0 do decodificador 2 |
| 15 | ~2G | Habilitação do decodificador 2 (ativa em L) |
| 16 | VCC | Alimentação positiva |

Função (fato): ~G=H → as 4 saídas daquele canal em H. ~G=L → só ~Y(n)=L, n = 2B + A (CD74HC139: E=0, A1A0=00 → Y0=0 … 11 → Y3=0).
Abs. max: família.

---

## 14. 74HC151 — multiplexador 8→1 com saídas complementares

Fonte: TI SN74HC151, SCLS110F (fev/2022) — https://www.ti.com/lit/ds/symlink/sn74hc151.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | D3 | Entrada de dado 3 |
| 2 | D2 | Entrada de dado 2 |
| 3 | D1 | Entrada de dado 1 |
| 4 | D0 | Entrada de dado 0 |
| 5 | Y | Saída: cópia da entrada selecionada |
| 6 | ~W | Saída invertida (W = NOT Y) |
| 7 | ~G | Strobe/habilitação ativa em L |
| 8 | GND | Terra |
| 9 | C | Seleção, bit 2 (MSB) |
| 10 | B | Seleção, bit 1 |
| 11 | A | Seleção, bit 0 (LSB) |
| 12 | D7 | Entrada de dado 7 |
| 13 | D6 | Entrada de dado 6 |
| 14 | D5 | Entrada de dado 5 |
| 15 | D4 | Entrada de dado 4 |
| 16 | VCC | Alimentação positiva |

Tabela (fato): ~G=H → Y=L, W=H. ~G=L → Y = D(4C+2B+A), W = NOT Y. Combinacional.
Abs. max: **±35 mA / ±70 mA** (fato; a tabela TI tem um erro tipográfico de unidade em VCC — "mA" no lugar de "V").

---

## 15. 74HC157 — 4 multiplexadores 2→1

Fonte: TI SN74HC157, SCLS113F (fev/2025) — https://www.ti.com/lit/ds/symlink/sn74hc157.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | A/~B | Seleção comum: L escolhe as entradas A; H escolhe B |
| 2 | 1A | Canal 1, entrada A |
| 3 | 1B | Canal 1, entrada B |
| 4 | 1Y | Canal 1, saída |
| 5 | 2A | Canal 2, entrada A |
| 6 | 2B | Canal 2, entrada B |
| 7 | 2Y | Canal 2, saída |
| 8 | GND | Terra |
| 9 | 3Y | Canal 3, saída |
| 10 | 3B | Canal 3, entrada B |
| 11 | 3A | Canal 3, entrada A |
| 12 | 4Y | Canal 4, saída |
| 13 | 4B | Canal 4, entrada B |
| 14 | 4A | Canal 4, entrada A |
| 15 | ~G | Strobe ativo em L: em H força todas as saídas em L |
| 16 | VCC | Alimentação positiva |

Tabela (fato): ~G=H → Y=L; ~G=L, A/~B=L → Y=A; ~G=L, A/~B=H → Y=B. Saídas não invertidas, não 3-state.
Abs. max: **±35 mA / ±70 mA** (fato).

---

## 16. CD74HC4511 — latch/decodificador/driver BCD → 7 segmentos

Fonte: TI CD74HC4511, SCHS279E (ago/2022) — https://www.ti.com/lit/ds/symlink/cd74hc4511.pdf — consultado em 20/11/2026 (pinagem lida da figura renderizada).

| Pino | Nome | Descrição |
|---|---|---|
| 1 | D1 | Entrada BCD, bit 1 |
| 2 | D2 | Entrada BCD, bit 2 |
| 3 | ~LT | Teste de lâmpada: em L acende todos os segmentos |
| 4 | ~BL | Apagamento: em L apaga todos os segmentos |
| 5 | LE (desenhado como ~LE) | Latch: em L transparente; em H guarda o código BCD |
| 6 | D3 | Entrada BCD, bit 3 (MSB) |
| 7 | D0 | Entrada BCD, bit 0 (LSB) |
| 8 | GND | Terra |
| 9 | e | Saída do segmento e (ativa em H) |
| 10 | d | Saída do segmento d |
| 11 | c | Saída do segmento c |
| 12 | b | Saída do segmento b |
| 13 | a | Saída do segmento a |
| 14 | g | Saída do segmento g |
| 15 | f | Saída do segmento f |
| 16 | VCC | Alimentação positiva |

Tabela de função (fato):

| LE | ~BL | ~LT | D3..D0 | a b c d e f g | Display |
|---|---|---|---|---|---|
| X | X | L | X | 1 1 1 1 1 1 1 | 8 (teste) |
| X | L | H | X | 0 0 0 0 0 0 0 | apagado |
| L | H | H | 0000 | 1 1 1 1 1 1 0 | 0 |
| L | H | H | 0001 | 0 1 1 0 0 0 0 | 1 |
| L | H | H | 0010 | 1 1 0 1 1 0 1 | 2 |
| L | H | H | 0011 | 1 1 1 1 0 0 1 | 3 |
| L | H | H | 0100 | 0 1 1 0 0 1 1 | 4 |
| L | H | H | 0101 | 1 0 1 1 0 1 1 | 5 |
| L | H | H | 0110 | 0 0 1 1 1 1 1 | 6 (sem a — sem "rabicho") |
| L | H | H | 0111 | 1 1 1 0 0 0 0 | 7 |
| L | H | H | 1000 | 1 1 1 1 1 1 1 | 8 |
| L | H | H | 1001 | 1 1 1 0 0 1 1 | 9 (sem d — sem "rabicho") |
| L | H | H | 1010–1111 | 0 0 0 0 0 0 0 | apagado ("display apagado para todo código ilegal, BCD > 1001") |
| H | H | H | X | depende do BCD presente quando LE estava em L | retido |

Prioridade (fato pela tabela): ~LT=L vence tudo (inclusive ~BL e LE); depois ~BL=L; depois latch/decodificação. O latch guarda o **código BCD** (entradas), não os segmentos: ~LT e ~BL continuam agindo com LE=H.
Saídas ativas em H (fonte de corrente → display de **catodo comum**) — **inferido** da tabela (segmento aceso = H) e do texto "high-output sourcing capability".

Corrente (fato): "transistores de saída de tamanho padrão, capazes de fornecer (em níveis VOH padrão) até 7,5 mA a 4,5 V (HCT); os tipos HC até 10 mA a 6 V". Abs. max: **IO ±25 mA por pino, VCC/GND ±50 mA** — igual à família (não é o driver de 25 mA do CD4511B CMOS). **Inferido:** com 7 segmentos acesos o total por VCC limita a média a ~7 mA por segmento (50/7); exige resistor por segmento.
**Surpresa:** a figura de pinagem TI desenha o pino 5 com barra (~LE), mas a tabela diz LE=L → transparente, LE=H → retém. Para o modelo, usar a tabela: o "~" do desenho só indica que o latch é transparente em L.

---

## 17. 74HC595 — registrador de deslocamento 8 bits com latch de saída 3-state

Fonte: TI SN74HC595, SCLS041J (out/2021) — https://www.ti.com/lit/ds/symlink/sn74hc595.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | QB | Saída paralela B (3-state) |
| 2 | QC | Saída paralela C |
| 3 | QD | Saída paralela D |
| 4 | QE | Saída paralela E |
| 5 | QF | Saída paralela F |
| 6 | QG | Saída paralela G |
| 7 | QH | Saída paralela H |
| 8 | GND | Terra |
| 9 | QH' | Saída serial para cascata (último estágio do deslocador; não é 3-state) |
| 10 | ~SRCLR | Clear do deslocador: em L zera o registrador de deslocamento (assíncrono) |
| 11 | SRCLK | Clock do deslocador: desloca na borda de subida |
| 12 | RCLK | Clock do latch: copia o deslocador para as saídas na borda de subida |
| 13 | ~OE | Habilita saídas QA–QH: em H ficam em alta impedância |
| 14 | SER | Entrada serial de dados |
| 15 | QA | Saída paralela A (primeiro estágio) |
| 16 | VCC | Alimentação positiva |

Tabela (fato): ~OE=H → QA–QH desabilitadas (Z); ~OE=L → habilitadas. ~SRCLR=L → registrador de deslocamento zerado. SRCLK ↑ com ~SRCLR=H → primeiro estágio recebe SER; demais recebem o estágio anterior. RCLK ↑ → dados do deslocador vão para o registrador de armazenamento (saídas). "Ambos os clocks são disparados na borda de subida. Se ligados juntos, o deslocador fica sempre um pulso à frente do registrador de saída."
**Inferido** (diagrama lógico e tabela): ~SRCLR não limpa o registrador de armazenamento — as saídas só ficam zero após um RCLK ↑ com o deslocador zerado. QH' segue o 8º estágio do deslocador (não o latch) e não é afetado por ~OE.
Abs. max: **±35 mA / ±70 mA** (fato). Saídas QA–QH: ±6 mA @ 4,5 V; QH': ±4 mA (padrão).

---

## 18. 74HC165 — registrador de deslocamento 8 bits, carga paralela

Fonte: TI SN74HC165, SCLS116H (dez/2015) — https://www.ti.com/lit/ds/symlink/sn74hc165.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | SH/~LD | Deslocar/carregar: L carrega A–H (assíncrono); H permite deslocar |
| 2 | CLK | Clock: desloca na borda de subida (CLK INH em L) |
| 3 | E | Entrada paralela E |
| 4 | F | Entrada paralela F |
| 5 | G | Entrada paralela G |
| 6 | H | Entrada paralela H (último estágio) |
| 7 | ~QH | Saída serial invertida |
| 8 | GND | Terra |
| 9 | QH | Saída serial |
| 10 | SER | Entrada serial: entra no primeiro estágio a cada deslocamento |
| 11 | A | Entrada paralela A (primeiro estágio) |
| 12 | B | Entrada paralela B |
| 13 | C | Entrada paralela C |
| 14 | D | Entrada paralela D |
| 15 | CLK INH | Inibe o clock: em H não desloca |
| 16 | VCC | Alimentação positiva |

Tabela (fato): SH/~LD=L → carga paralela (independe de CLK, CLK INH e SER — assíncrona, por nível); SH/~LD=H, CLK INH=H → sem mudança; SH/~LD=H, CLK=H → sem mudança; SH/~LD=H, CLK INH=L, CLK ↑ → desloca em direção a QH (SER entra em A). "As funções de CLK e CLK INH são intercambiáveis": CLK=L e CLK INH ↑ também desloca.
**Inferido:** clock efetivo = CLK OR CLK INH, ativo na borda de subida. QH = estágio H; ~QH = NOT QH.
Abs. max: família.

---

## 19. 74HC164 — registrador de deslocamento 8 bits, entrada serial com AND

Fonte: TI SN74HC164, SCLS115G (set/2015) — https://www.ti.com/lit/ds/symlink/sn74hc164.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | A | Entrada serial 1 (com B forma um AND) |
| 2 | B | Entrada serial 2 |
| 3 | QA | Saída paralela A (primeiro estágio) |
| 4 | QB | Saída paralela B |
| 5 | QC | Saída paralela C |
| 6 | QD | Saída paralela D |
| 7 | GND | Terra |
| 8 | CLK | Clock: desloca na borda de subida |
| 9 | ~CLR | Clear assíncrono: em L zera todas as saídas |
| 10 | QE | Saída paralela E |
| 11 | QF | Saída paralela F |
| 12 | QG | Saída paralela G |
| 13 | QH | Saída paralela H (último estágio) |
| 14 | VCC | Alimentação positiva |

Tabela (fato): ~CLR=L → todas L (assíncrono); ~CLR=H, CLK=L → mantém; CLK ↑ → QA = A AND B, QB..QH = estágio anterior. Sem 3-state, sem saída serial separada (QH serve de cascata).
Abs. max: família.

---

## 20. 74HC85 — comparador de magnitude de 4 bits

Fonte: TI CD74HC85, SCHS136F (fev/2022) — https://www.ti.com/lit/ds/symlink/cd74hc85.pdf — consultado em 20/11/2026 (pinagem lida da figura renderizada). Não existe SN74HC85 na TI (URL retornou HTML).

| Pino | Nome | Descrição |
|---|---|---|
| 1 | B3 | Palavra B, bit 3 (MSB) |
| 2 | (A<B) IN | Entrada de cascata "menor" (vinda do estágio menos significativo) |
| 3 | (A=B) IN | Entrada de cascata "igual" |
| 4 | (A>B) IN | Entrada de cascata "maior" |
| 5 | (A>B) OUT | Saída: H se A > B |
| 6 | (A=B) OUT | Saída: H se A = B |
| 7 | (A<B) OUT | Saída: H se A < B |
| 8 | GND | Terra |
| 9 | B0 | Palavra B, bit 0 (LSB) |
| 10 | A0 | Palavra A, bit 0 (LSB) |
| 11 | B1 | Palavra B, bit 1 |
| 12 | A1 | Palavra A, bit 1 |
| 13 | A2 | Palavra A, bit 2 |
| 14 | B2 | Palavra B, bit 2 |
| 15 | A3 | Palavra A, bit 3 (MSB) |
| 16 | VCC | Alimentação positiva |

Tabela verdade (fato): se A ≠ B (comparando do bit 3 para o 0), saídas = (A>B: H, A<B: L, A=B: L) ou (L, H, L), ignorando as entradas de cascata. Se A = B, as saídas dependem das entradas de cascata (IN>, IN<, IN=):

| IN A>B | IN A<B | IN A=B | OUT A>B | OUT A<B | OUT A=B |
|---|---|---|---|---|---|
| H | L | L | H | L | L |
| L | H | L | L | H | L |
| L | L | H | L | L | H |
| X | X | H | L | L | H |
| H | H | L | L | L | L |
| L | L | L | H | H | L |

Uso (fato): um único chip ou cascata série usa as 3 primeiras linhas (no estágio menos significativo, ligar IN A=B em H, IN A>B e IN A<B em L). As 3 últimas descrevem expansão paralela.
**Inferido (equações que reproduzem a tabela quando A=B):** OUT= = IN=; OUT> = NOT IN= AND NOT IN<; OUT< = NOT IN= AND NOT IN>.
Abs. max: família (IO ±25 mA, ICC ±50 mA).

---

## 21. 74HC244 — 8 buffers 3-state em 2 grupos de 4

Fonte: TI SN74HC244, SCLS130I (fev/2026) — https://www.ti.com/lit/ds/symlink/sn74hc244.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | ~1OE | Habilita grupo 1: em L saídas ativas; H alta impedância |
| 2 | 1A1 | Entrada 1 do grupo 1 |
| 3 | 2Y4 | Saída 4 do grupo 2 |
| 4 | 1A2 | Entrada 2 do grupo 1 |
| 5 | 2Y3 | Saída 3 do grupo 2 |
| 6 | 1A3 | Entrada 3 do grupo 1 |
| 7 | 2Y2 | Saída 2 do grupo 2 |
| 8 | 1A4 | Entrada 4 do grupo 1 |
| 9 | 2Y1 | Saída 1 do grupo 2 |
| 10 | GND | Terra |
| 11 | 2A1 | Entrada 1 do grupo 2 |
| 12 | 1Y4 | Saída 4 do grupo 1 |
| 13 | 2A2 | Entrada 2 do grupo 2 |
| 14 | 1Y3 | Saída 3 do grupo 1 |
| 15 | 2A3 | Entrada 3 do grupo 2 |
| 16 | 1Y2 | Saída 2 do grupo 1 |
| 17 | 2A4 | Entrada 4 do grupo 2 |
| 18 | 1Y1 | Saída 1 do grupo 1 |
| 19 | ~2OE | Habilita grupo 2 (ativa em L) |
| 20 | VCC | Alimentação positiva |

Tabela (fato): ~OE=L → Y = A (não inverte); ~OE=H → Z.
Abs. max: **±35 mA / ±70 mA** (fato).

---

## 22. 74HC125 — 4 buffers 3-state com enable individual

Fonte: TI SN74HC125, SCLS104F (abr/2021) — https://www.ti.com/lit/ds/symlink/sn74hc125.pdf — consultado em 20/11/2026.

| Pino | Nome | Descrição |
|---|---|---|
| 1 | ~1OE | Habilita buffer 1 (ativa em L) |
| 2 | 1A | Entrada do buffer 1 |
| 3 | 1Y | Saída do buffer 1 (3-state) |
| 4 | ~2OE | Habilita buffer 2 |
| 5 | 2A | Entrada do buffer 2 |
| 6 | 2Y | Saída do buffer 2 |
| 7 | GND | Terra |
| 8 | 3Y | Saída do buffer 3 |
| 9 | 3A | Entrada do buffer 3 |
| 10 | ~3OE | Habilita buffer 3 |
| 11 | 4Y | Saída do buffer 4 |
| 12 | 4A | Entrada do buffer 4 |
| 13 | ~4OE | Habilita buffer 4 |
| 14 | VCC | Alimentação positiva |

Tabela (fato): ~OE=L → Y = A; ~OE=H → Z.
Abs. max: **±35 mA / ±70 mA** (fato).

---

## Lista de URLs usadas (todas consultadas em 20/11/2026)

- https://www.ti.com/lit/ds/symlink/sn74hc02.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc04.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc10.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc11.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc14.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc74.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc273.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc373.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc374.pdf
- https://www.onsemi.com/download/data-sheet/pdf/mc74hc374a-d.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc163.pdf
- https://www.ti.com/lit/ds/symlink/cd74hc163.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc193.pdf
- https://www.ti.com/lit/ds/symlink/cd74hc193.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc138.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc139.pdf
- https://www.ti.com/lit/ds/symlink/cd74hc139.pdf
- https://www.onsemi.com/download/data-sheet/pdf/mc74hc139a-d.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc151.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc157.pdf
- https://www.ti.com/lit/ds/symlink/cd74hc4511.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc595.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc165.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc164.pdf
- https://www.ti.com/lit/ds/symlink/cd74hc85.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc244.pdf
- https://www.ti.com/lit/ds/symlink/sn74hc125.pdf

Não acessíveis: nexperia.com (403 Access Denied para cliente automatizado); st.com (timeout). Não foram necessários.
