# Datasheets: chips analógicos/mistos e limites de dano

Consultado em 20/11/2026. Fontes: só sites oficiais dos fabricantes (ou da Arduino/Fluke/Rigol/Keysight para placa e instrumentos).
O texto vem de PDFs baixados e extraídos com `pdftotext`. Em algumas tabelas TI o layout ficou embaralhado. Quando um valor
foi reconstruído de uma tabela embaralhada, isso está marcado com **(reconstr.)**.

Convenções:
- **Fato**: número/frase do datasheet citado.
- **Inferência**: conclusão minha a partir do datasheet, para o modelo ideal. Não é texto do fabricante.
- Pino ativo-baixo usa `~` no início do nome.

---

## Parte A — chips novos

### A1. NE555 (TI)

Fonte: TI SLFS022K, "xx555 Precision Timers", rev. março 2026 — https://www.ti.com/lit/ds/symlink/ne555.pdf

#### Pinagem (DIP-8 / P)

| Pino | Nome | Descrição (pt-BR) |
|---|---|---|
| 1 | GND | Terra; referência de todas as tensões. |
| 2 | ~TRIG | Disparo: abaixo de 1/2·V(CONT) (≈1/3 VCC) seta o flip-flop, a saída vai para alto e a descarga abre. |
| 3 | OUT | Saída totem-pole; fornece ou drena até 200 mA. |
| 4 | ~RESET | Reset ativo-baixo: força a saída e a descarga para baixo; tem prioridade sobre tudo. |
| 5 | CONT | Tensão de controle: fixa o limiar (2/3 VCC por divisor interno); aceita capacitor de desacoplamento ou tensão externa. |
| 6 | THRES | Limiar: acima de V(CONT) (≈2/3 VCC) reseta o flip-flop, a saída vai para baixo e a descarga conduz. |
| 7 | DISCH | Descarga em coletor aberto para GND; conduz enquanto a saída está baixa. |
| 8 | VCC | Alimentação, 4,5 V a 16 V (NE555). |

Observação: o datasheet chama o pino 2 de "TRIG" e o 4 de "RESET" ("Active low reset input"). O disparo é ativo por nível baixo, por isso aparece aqui como `~TRIG`.

#### Funcionamento (fato, Tabela 6-1 e descrição)

| RESET | TRIG | THRES | OUT | Descarga |
|---|---|---|---|---|
| Baixo (< 0,3–1 V) | X | X | Baixo | Ligada |
| > máx | < 1/3 VCC | X (TRIG vence) | Alto | Desligada |
| > máx | > 1/3 VCC | > 2/3 VCC | Baixo | Ligada |
| > máx | > 1/3 VCC | < 2/3 VCC | Mantém o estado anterior | Mantém |

- Prioridade (nota da Figura 6-1): "RESET can override TRIG, which can override THRES."
- Limiares: limiar = V(CONT), com CONT aberto ≈ 2/3 VCC; disparo = 1/2·V(CONT) ≈ 1/3 VCC. CONT forçado externamente desloca os dois limiares.
- "Whenever the output is low, a low-impedance path is provided between DISCH and GND."
- A descarga e a capacidade de dreno da saída são maiores com VCC alto.

Fórmulas (fato, seção 6.3):
- Monoestável: `tw ≈ 1,1 · RA · C`.
- Astável: `tH ≈ 0,693·(RA+RB)·C`, `tL ≈ 0,693·RB·C`, `T ≈ 0,693·(RA+2RB)·C`, `f ≈ 1,44/((RA+2RB)·C)`.
  O capacitor oscila entre ≈0,33 VCC e ≈0,67 VCC; o tempo independe de VCC.
- Frequência máxima recomendada: 100 kHz ("To reduce distortion, use at maximum frequency of 100 kHz").
- Faixa de uso citada: de 10 µs a horas.
- RA+RB máximo, limitado pela corrente de THRES: 3,4 MΩ com VCC = 5 V e 10 MΩ com VCC = 15 V (nota 1 da tabela EC).
- Com TRIG aterrado, o tempo de armazenamento do comparador pode chegar a 10 µs, o que limita o pulso monoestável mínimo.

#### Valores máximos absolutos (fato)

| Parâmetro | Valor |
|---|---|
| VCC | 18 V |
| Entradas CONT, RESET, THRES, TRIG | até VCC |
| Corrente de saída IO | ±225 mA **(reconstr.)** |
| TJ | 150 °C |
| Armazenamento | −65 a 150 °C |
| ESD | HBM ±1500 V, CDM ±500 V |

#### Condições recomendadas (fato)

| Parâmetro | NE555 |
|---|---|
| VCC | 4,5 a 16 V |
| IO | ±200 mA |
| TA | 0 a 70 °C |

#### Características elétricas do NE555 a 25 °C (fato; colunas reconstruídas da tabela)

| Parâmetro | Condição | Mín | Típ | Máx |
|---|---|---|---|---|
| Nível THRES | VCC 15 V | 8,8 | 10 | 11,2 V |
| Nível THRES | VCC 5 V | 2,4 | 3,3 | 4,2 V |
| Corrente THRES | — | | 30 | 250 nA |
| Nível TRIG | VCC 15 V | 4,5 | 5 | 5,6 V |
| Nível TRIG | VCC 5 V | 1,1 | 1,67 | 2,2 V |
| Corrente TRIG | TRIG = 0 V | | 0,5 | 2 µA |
| Nível RESET | — | 0,3 | 0,7 | 1 V |
| Corrente RESET | RESET = VCC / 0 V | | 0,1 / −0,4 | 0,4 / −1,5 mA |
| DISCH desligado | — | | 20 | 100 nA |
| DISCH ligado | VCC 5 V, 8 mA | | 0,15 | 0,4 V |
| CONT aberto | VCC 15 V / 5 V | 9 / 2,6 | 10 / 3,3 | 11 / 4 V |
| VOL | 15 V: 10 / 50 / 100 mA | | 0,1 / 0,4 / 2 | 0,25 / 0,75 / 2,5 V |
| VOL | 15 V, 200 mA | | 2,5 | V |
| VOL | 5 V: 5 / 8 mA | | 0,1 / 0,15 | 0,35 / 0,4 V |
| VOH | 15 V, −100 mA | 12,75 | 13,3 | V |
| VOH | 15 V, −200 mA | | 12,5 | V |
| VOH | 5 V, −100 mA | 2,75 | 3,3 | V |
| ICC (saída baixa, sem carga) | 15 V / 5 V | | 10 / 3 | 15 / 6 mA |
| ICC (saída alta, sem carga) | 15 V / 5 V | | 9 / 2 | 13 / 5 mA |

**Inferência para o modelo ideal**: limiares exatos de 2/3·VCC e 1/3·VCC (ou V(CONT) e V(CONT)/2), sem histerese extra, e RESET tratado
como lógico (< 0,7 V = ativo). Para avisar de dano: VCC > 16 V (fora do recomendado), VCC > 18 V (queima),
|IO| > 200 mA (aviso) e > 225 mA (dano), entrada acima de VCC (dano).

---

### A2. LM358 (TI)

Fonte: TI SLOS068AB, rev. outubro 2024 (família LM158/258/358/2904, incluindo LM358B) — https://www.ti.com/lit/ds/symlink/lm358.pdf

#### Pinagem (DIP-8)

| Pino | Nome | Descrição (pt-BR) |
|---|---|---|
| 1 | OUT1 | Saída do amplificador 1. |
| 2 | IN1− | Entrada inversora do amplificador 1. |
| 3 | IN1+ | Entrada não inversora do amplificador 1. |
| 4 | V− | Alimentação negativa ou GND (fonte simples). |
| 5 | IN2+ | Entrada não inversora do amplificador 2. |
| 6 | IN2− | Entrada inversora do amplificador 2. |
| 7 | OUT2 | Saída do amplificador 2. |
| 8 | V+ | Alimentação positiva. |

#### Máximos absolutos (fato)

| Parâmetro | LM358 / LM358A | LM358B / LM358BA |
|---|---|---|
| VS = V+ − V− | 32 V (±16 V) | 40 V (±20 V) |
| Entrada diferencial | ±32 V | ±40 V **(reconstr.)** |
| Tensão em qualquer entrada | −0,3 a 32 V | −0,3 a 40 V **(reconstr.)** |
| Curto da saída para GND (TA ≤ 25 °C, VS ≤ 15 V) | ilimitado | ilimitado |
| TJ | 150 °C | 150 °C |

Nota 3 (fato): "Short circuits from outputs to VS can cause excessive heating and eventual destruction."

#### Recomendado (fato)

| | LM358/A | LM358B/BA |
|---|---|---|
| VS | 3 a 30 V | 3 a 36 V |
| Modo comum | V− a V+ − 2 V (em toda a temperatura) | V− a V+ − 2 V |
| TA | 0 a 70 °C | −40 a 85 °C |

#### Características (fato; LM358, VS = 5 V, 25 °C, salvo indicação)

| Parâmetro | Valor |
|---|---|
| VOS | 2 mV típ, 7 mV máx |
| IB | −20 nA típ, −250 nA máx |
| Faixa de modo comum | V− até V+ − 1,5 V (25 °C); V+ − 2 V na faixa de temperatura |
| AOL | 100 V/mV típ (VS 15 V, RL ≥ 2 kΩ) |
| GBW | 0,7 MHz (LM358); 1,2 MHz (LM358B) |
| Slew rate | 0,3 V/µs (LM358); 0,5 V/µs (LM358B) |
| Excursão até o trilho positivo | VS 5 V, RL ≥ 2 kΩ: 1,5 V abaixo de V+ (máx); VS 30 V, RL ≥ 10 kΩ: 2 V típ / 3 V máx; RL = 2 kΩ: 4 V |
| Excursão até o trilho negativo | VS 5 V, RL ≥ 10 kΩ: 5 mV típ, 20 mV máx |
| Corrente de saída (fonte) | VS 15 V, VO = 0: −20 mA mín, −30 mA típ (−60 mA máx) |
| Corrente de saída (dreno) | VS 15 V, VO = 15 V: 10 mA mín, 20 mA típ; com VO = 200 mV: 12 µA mín, 30 µA típ |
| Curto-circuito ISC | ±40 mA típ, ±60 mA máx (VS 10 V, VO = VS/2) |
| IQ por amplificador | 350 µA típ, 600 µA máx (VS 5 V) |

LM358B (fato): excursão até V+ de 1,4 V típ e 1,61 V máx com IOUT = 5 mA; ISC de ±40 mA.

Comportamento de entrada (fato, 7.3.3): "Inputs may exceed VS up to the maximum VS without device damage. At least one input must be in the
valid input common-mode range for the output to be the correct phase. If both inputs exceed the valid range, then the output phase is undefined.
If either input more than 0.3 V below V−, then input current should be limited to 1 mA and the output phase is undefined."

**Inferência para o modelo ideal**: a saída fica limitada a [V− + ~0 V, V+ − 1,5 V]. Ganho alto com GBW de 0,7 MHz e SR de 0,3 V/µs (versão
não-B). Corrente de saída limitada a ±40 mA (curto). Para avisar de dano: VS > 32 V; entrada < −0,3 V com mais de 1 mA; curto da saída para V+.

---

### A3. LM393 (TI)

Fonte: TI SLCS005AH, rev. abril 2025 — https://www.ti.com/lit/ds/symlink/lm393.pdf

#### Pinagem (DIP-8)

| Pino | Nome | Descrição (pt-BR) |
|---|---|---|
| 1 | 1OUT | Saída do comparador 1 em coletor aberto (precisa de pull-up). |
| 2 | 1IN− | Entrada inversora do comparador 1. |
| 3 | 1IN+ | Entrada não inversora do comparador 1. |
| 4 | GND | Terra / alimentação negativa. |
| 5 | 2IN+ | Entrada não inversora do comparador 2. |
| 6 | 2IN− | Entrada inversora do comparador 2. |
| 7 | 2OUT | Saída do comparador 2 em coletor aberto. |
| 8 | VCC | Alimentação positiva. |

#### Máximos absolutos (fato; tabela embaralhada, por isso **(reconstr.)**)

| Parâmetro | Não-B (LM393/LM393A) | LM393B |
|---|---|---|
| VCC | 36 V | 38 V |
| Entrada diferencial | ±36 V | ±38 V |
| Tensão de entrada | −0,3 a 36 V | −0,3 a 38 V |
| Corrente de entrada IIK | −50 mA | — |
| Tensão de saída | 36 V | 38 V |
| Corrente de saída IO | 20 mA | 25 mA |
| Curto da saída para GND | ilimitado | ilimitado |
| TJ | 150 °C | 150 °C |

Notas (fato): "Short circuits from outputs to VCC can cause excessive heating and eventual destruction." Corrente negativa de entrada "flows through
parasitic diode to ground ... can cause the output to be incorrect. Normal operation resumes when input current is removed."

#### Recomendado (fato)

| | Não-B | B |
|---|---|---|
| VS | 2 a 30 V (versões V: 2 a 32 V) | 2 a 36 V |
| Modo comum (entrada) | 0 a VCC − 2 V (faixa de temperatura); 0 a VCC − 1,5 V a 25 °C | 0 a V+ − 2 V |
| TA (LM393) | 0 a 70 °C | −40 a 85 °C |

#### Características (fato; LM393, VCC = 5 V, 25 °C)

| Parâmetro | Valor |
|---|---|
| VIO | 2 mV típ, 5 mV máx |
| IIB | −25 nA típ, −250 nA máx |
| VOL com IOL = 4 mA | 130 mV típ, 400 mV máx (700 mV na faixa de temperatura) |
| IOL com VOL = 1,5 V | 6 mA mín |
| IOH (fuga, saída "alta") | 0,1 nA típ com VOH = 5 V; 1 µA máx com VOH = 30 V |
| AVD | 200 V/mV típ |
| Tempo de resposta | 1,3 µs (degrau de 100 mV com 5 mV de overdrive); 0,3 µs (degrau TTL); RL de 5,1 kΩ para 5 V |
| ICC (os dois) | 0,45 mA típ, 1 mA máx (5 V); 2,5 mA máx (30 V) |

Nota (fato): "only one input needs to be in the valid common mode range, and the other input can go up the maximum VCC level and the comparator
provides a proper output state. Either or both inputs can go to maximum VCC level without damage."

**Inferência para o modelo ideal**: com IN+ > IN−, a saída fica em alta impedância (o pull-up externo decide o nível); com IN+ < IN−, o transistor
conduz e a saída vai a ~0 V (≤ 0,4 V com 4 mA). Para avisar de dano: corrente de dreno > 20 mA ou saída curto-circuitada direto para VCC.

---

### A4. LM7805 / LM340 (TI)

Fonte: TI SNOSBT0L, "LM340, LM340A and LM7805 Family", rev. setembro 2016 — https://www.ti.com/lit/ds/symlink/lm340.pdf
(O datasheet ST L78 em https://www.st.com/resource/en/datasheet/l78.pdf não pôde ser baixado: o servidor recusou acesso automático.)

#### Pinagem (TO-220, NDE, vista de frente)

| Pino | Nome | Descrição (pt-BR) |
|---|---|---|
| 1 | INPUT | Entrada não regulada (7,5 V a 35 V). |
| 2 | GND | Terra / referência. |
| 3 | OUTPUT | Saída regulada de 5 V. |

Fato: "Tab/Case is Ground or Output" (depende do encapsulamento). **Inferência**: no TO-220 a aba é GND (pino 2), como no TO-3, cuja carcaça é o terra.
Isso não está escrito por encapsulamento no texto extraído.

#### Máximos absolutos (fato)

| Parâmetro | Valor |
|---|---|
| Tensão DC de entrada | 35 V |
| Dissipação interna | limitada internamente |
| TJ máx | 150 °C |
| Armazenamento | −65 a 150 °C |
| ESD | HBM ±2000 V |

Térmico (fato): TO-220: θJA = 54 °C/W, θJC = 4 °C/W. PDmax = (TJmax − TA)/θJA. "If the die temperature rises above 150 °C, the device goes into
thermal shutdown."

#### Características do LM7805 (fato; VI = 10 V, 0 °C ≤ TJ ≤ 125 °C)

| Parâmetro | Valor |
|---|---|
| VO | 4,8 / 5 / 5,2 V (25 °C, 5 mA–1 A); 4,75–5,25 V (PD ≤ 15 W, 7,5 V ≤ VIN ≤ 20 V) |
| Regulação de linha | 3 mV típ, 50 mV máx |
| Regulação de carga | 10 mV típ, 50 mV máx (5 mA–1,5 A) |
| IQ | 8 mA máx (25 °C); 8,5 mA na faixa de temperatura |
| Dropout | 2 V típ (25 °C, IO = 1 A) |
| Corrente de curto-circuito | 2,1 A típ (25 °C) |
| Corrente de pico de saída | 2,4 A típ (25 °C) |
| VIN mínimo para manter a regulação | 7,5 V (25 °C, IO ≤ 1 A) |
| Rejeição de ripple | 62 dB mín, 80 dB típ |

#### Riscos de aplicação (fato, seção 8.1)

- Entrada curto-circuitada com capacitor de saída grande (> 10 µF): o capacitor descarrega pelo diodo interno e pode destruir o regulador. Use diodo de proteção da saída para a entrada.
- "Because the output of the device does not sink current, forcing the output high can cause damage."
- GND solto: a saída sobe para a entrada não regulada e pode danificar a carga.
- Transientes acima de 35 V, ou mais de 0,8 V abaixo do terra, danificam o regulador.

**Inferência para o modelo ideal**: VOUT = min(5 V, VIN − 2 V), limite de corrente de ~2,1 A, desligamento térmico com TJ ≥ 150 °C, regulador que só fornece corrente.
Para avisar de dano: VIN > 35 V; VOUT > VIN (com capacitor grande); entrada < −0,8 V.

---

### A5. ULN2003A (TI)

Fonte: TI SLRS027T, rev. março 2025 — https://www.ti.com/lit/ds/symlink/uln2003a.pdf

#### Pinagem (DIP-16 / N)

| Pino | Nome | Descrição (pt-BR) |
|---|---|---|
| 1–7 | 1B–7B | Entrada (base) do Darlington do canal 1–7; resistor série de 2,7 kΩ interno. Nível alto liga o canal. |
| 8 | E | Emissor comum de todos os canais; normalmente ligado ao GND. |
| 9 | COM | Catodo comum dos diodos de roda-livre; ligar à alimentação da carga indutiva. |
| 10 | 7C | Coletor (saída de dreno) do canal 7. |
| 11 | 6C | Coletor do canal 6. |
| 12 | 5C | Coletor do canal 5. |
| 13 | 4C | Coletor do canal 4. |
| 14 | 3C | Coletor do canal 3. |
| 15 | 2C | Coletor do canal 2. |
| 16 | 1C | Coletor do canal 1. |

Funcionamento (fato): saída Darlington NPN em coletor aberto, invertendo (entrada alta faz a saída drenar). A base tem resistor série de 2,7 kΩ e resistores de 7,2 kΩ e 3 kΩ para o emissor.
O diodo de cada saída vai do coletor para COM.

#### Máximos absolutos (fato, 25 °C)

| Parâmetro | Valor |
|---|---|
| VCE (saída) | 50 V |
| Tensão reversa do diodo de clamp | 50 V |
| Tensão de entrada VI | 30 V |
| Corrente de pico de coletor (por canal) | 500 mA |
| Corrente do clamp IOK | 500 mA |
| Corrente total no terminal E | −2,5 A |
| TJ | 150 °C |

Recomendado: VCE de 0 a 50 V; TJ de −40 a 125 °C.

#### Características do ULN2003A (fato, 25 °C)

| Parâmetro | Condição | Valor |
|---|---|---|
| VI(on) máx | VCE = 2 V; IC = 200 / 250 / 300 mA | 2,4 / 2,7 / 3 V |
| VCE(sat) | II = 250 µA, IC = 100 mA | 0,9 típ / 1,1 máx V |
| VCE(sat) | II = 350 µA, IC = 200 mA | 1 típ / 1,3 máx V |
| VCE(sat) | II = 500 µA, IC = 350 mA | 1,2 típ / 1,6 máx V |
| ICEX | VCE = 50 V | 50 µA máx (100 µA a 70 °C) |
| VF do clamp | IF = 350 mA | 1,7 típ / 2 máx V |
| II | VI = 3,85 V | 0,93 típ / 1,35 máx mA |
| II(off) | IC = 500 µA | 50 µA mín, 65 µA típ |
| IR do clamp | VR = 50 V | 50 µA (100 µA a 70 °C) |

**Inferência para o modelo ideal**: um canal liga quando a entrada ultrapassa ~2,4–3 V (TTL/CMOS de 5 V servem); a saída é uma chave
para E com queda de ~1 V. Limite contínuo por canal: depende da dissipação; o datasheet dá 500 mA como pico (e figuras de ciclo de trabalho).
Para avisar de dano: IC > 500 mA; soma no pino E > 2,5 A; VCE > 50 V; carga indutiva sem COM ligado (o pico de tensão pode exceder 50 V).

---

### A6. L293D (TI)

Fonte: TI SLRS008D, "L293x Quadruple Half-H Drivers", rev. janeiro 2016 — https://www.ti.com/lit/ds/symlink/l293.pdf

#### Pinagem (DIP-16 / NE)

| Pino | Nome | Descrição (pt-BR) |
|---|---|---|
| 1 | 1,2EN | Habilita os canais 1 e 2 (ativo alto); baixo deixa 1Y/2Y em alta impedância. |
| 2 | 1A | Entrada lógica do canal 1 (não inversora). |
| 3 | 1Y | Saída de potência do canal 1. |
| 4 | GND | Terra e dissipação de calor. |
| 5 | GND | Terra e dissipação de calor. |
| 6 | 2Y | Saída de potência do canal 2. |
| 7 | 2A | Entrada lógica do canal 2. |
| 8 | VCC2 | Alimentação dos motores (saídas), de VCC1 a 36 V. |
| 9 | 3,4EN | Habilita os canais 3 e 4 (ativo alto). |
| 10 | 3A | Entrada lógica do canal 3. |
| 11 | 3Y | Saída de potência do canal 3. |
| 12 | GND | Terra e dissipação de calor. |
| 13 | GND | Terra e dissipação de calor. |
| 14 | 4Y | Saída de potência do canal 4. |
| 15 | 4A | Entrada lógica do canal 4. |
| 16 | VCC1 | Alimentação da lógica interna, 4,5 a 7 V (5 V). |

#### Tabela-verdade (fato, por canal)

| A | EN | Y |
|---|---|---|
| H | H | H (puxa para VCC2) |
| L | H | L (puxa para GND) |
| X | L | Z (desligado) |

"In the thermal shutdown mode, the output is in the high-impedance state, regardless of the input levels."
Saída totem-pole com dreno Darlington e fonte pseudo-Darlington. **L293D**: diodos de clamp internos nas saídas (o L293 não tem).

#### Máximos absolutos (fato)

| Parâmetro | Valor |
|---|---|
| VCC1 | 36 V |
| VCC2 | 36 V |
| VI (entradas) | 7 V |
| VO | −3 V a VCC2 + 3 V |
| Corrente de pico, L293D (não repetitiva, t ≤ 100 µs) | ±1,2 A |
| Corrente contínua, L293D | ±600 mA |
| Corrente de pico, L293 (t ≤ 5 ms) / contínua | ±2 A / ±1 A |
| TJ | 150 °C |

#### Recomendado (fato)

| Parâmetro | Mín | Máx |
|---|---|---|
| VCC1 | 4,5 V | 7 V |
| VCC2 | VCC1 | 36 V |
| VIH (VCC1 ≤ 7 V) | 2,3 V | VCC1 |
| VIL | −0,3 V | 1,5 V |
| TA | 0 °C | 70 °C |

#### Características (fato)

| Parâmetro | Condição (L293D) | Valor |
|---|---|---|
| VOH | IOH = −0,6 A | VCC2 − 1,8 mín, VCC2 − 1,4 típ |
| VOL | IOL = 0,6 A | 1,2 típ, 1,8 máx V |
| VOKH (clamp alto) | IOK = −0,6 A | VCC2 + 1,3 V típ |
| VOKL (clamp baixo) | IOK = 0,6 A | 1,3 V típ |
| ICC1 / ICC2 | por estado das saídas | tabela embaralhada na extração; conferir no PDF (até 60 mA máx em ICC1, saídas baixas) |

**Inferência para o modelo ideal**: uma meia-ponte por canal; Y = VCC2 − 1,4 V (alto) ou 1,2 V (baixo) com 0,6 A; em Z quando EN = 0.
Para avisar de dano: |IO| > 600 mA contínuo (L293D) ou > 1,2 A de pico; VCC1/VCC2 > 36 V; VI > 7 V.

---

### A7. SRAM 62256 (32K×8)

Fontes:
- Alliance Memory AS6C62256, rev. 1.2 (23/03/2016) — https://www.alliancememory.com/wp-content/uploads/AS6C62256-23-March-2016-rev1.2.pdf
- Infineon (Cypress) CY62256N, 001-06511 — https://www.infineon.com/dgdl/Infineon-CY62256N_256-Kbit_(32_K_8)_Static_RAM-DataSheet-v11_00-EN.pdf?fileId=8ac78c8c7d0d8da4017d0ebee07732ce

#### Pinagem (DIP-28, JEDEC; idêntica nos dois)

| Pino | Nome | Descrição (pt-BR) |
|---|---|---|
| 1 | A14 | Endereço, bit 14 (mais alto). |
| 2 | A12 | Endereço, bit 12. |
| 3 | A7 | Endereço, bit 7. |
| 4 | A6 | Endereço, bit 6. |
| 5 | A5 | Endereço, bit 5. |
| 6 | A4 | Endereço, bit 4. |
| 7 | A3 | Endereço, bit 3. |
| 8 | A2 | Endereço, bit 2. |
| 9 | A1 | Endereço, bit 1. |
| 10 | A0 | Endereço, bit 0. |
| 11 | DQ0 | Dado bit 0 (entrada/saída tri-state). |
| 12 | DQ1 | Dado bit 1. |
| 13 | DQ2 | Dado bit 2. |
| 14 | VSS | Terra. |
| 15 | DQ3 | Dado bit 3. |
| 16 | DQ4 | Dado bit 4. |
| 17 | DQ5 | Dado bit 5. |
| 18 | DQ6 | Dado bit 6. |
| 19 | DQ7 | Dado bit 7. |
| 20 | ~CE | Habilita o chip (ativo baixo); alto = espera, saídas em Z. |
| 21 | A10 | Endereço, bit 10. |
| 22 | ~OE | Habilita a saída (ativo baixo) na leitura. |
| 23 | A11 | Endereço, bit 11. |
| 24 | A9 | Endereço, bit 9. |
| 25 | A8 | Endereço, bit 8. |
| 26 | A13 | Endereço, bit 13. |
| 27 | ~WE | Habilita a escrita (ativo baixo); tem prioridade sobre ~OE. |
| 28 | VCC | Alimentação. |

#### Tabela-verdade (fato, AS6C62256)

| Modo | ~CE | ~OE | ~WE | DQ |
|---|---|---|---|---|
| Espera | H | X | X | Z |
| Saída desabilitada | L | H | H | Z |
| Leitura | L | L | H | DOUT |
| Escrita | L | X | L | DIN |

#### Limites

| Parâmetro | AS6C62256 | CY62256N |
|---|---|---|
| VCC de operação | 2,7–5,5 V | 4,5–5,5 V (5 V ± 10%) |
| Máx. tensão em terminal | −0,5 a 7,0 V | VCC: −0,5 a 7,0 V; entradas/saídas: −0,5 a VCC + 0,5 V |
| Corrente DC de saída | 50 mA | 20 mA (saída em nível baixo) |
| Dissipação | 1 W | — |
| VIH / VIL | 2,4 V a VCC + 0,5 / −0,5 a 0,6 V | (ver datasheet) |
| VOH / VOL | 2,4 V @ −1 mA / 0,4 V @ 2 mA | 2,4 V @ −1 mA / 0,4 V @ 2,1 mA |
| Retenção de dados | VCC ≥ 1,5 V | — |

#### Tempos (fato) — grau mais lento comum: CY62256N-70 (70 ns); AS6C62256 só tem -55

| Parâmetro | AS6C62256-55 | CY62256N-55 | CY62256N-70 |
|---|---|---|---|
| tRC ciclo de leitura (mín) | 55 | 55 | 70 ns |
| tAA endereço → dado (máx) | 55 | 55 | 70 ns |
| tACE ~CE → dado (máx) | 55 | 55 | 70 ns |
| tOE/tDOE ~OE → dado (máx) | 30 | 25 | 35 ns |
| tOH/tOHA retenção após troca de endereço (mín) | 10 | 5 | 5 ns |
| tCLZ/tLZCE (mín) | 10 | 5 | 5 ns |
| tOLZ/tLZOE (mín) | 5 | 5 | 5 ns |
| tCHZ/tHZCE (máx) | 20 | 20 | 25 ns |
| tOHZ/tHZOE (máx) | 20 | 20 | 25 ns |
| tWC ciclo de escrita (mín) | 55 | 55 | 70 ns |
| tCW/tSCE ~CE → fim da escrita (mín) | 50 | 45 | 60 ns |
| tAW endereço → fim da escrita (mín) | 50 | 45 | 60 ns |
| tAS/tSA setup do endereço (mín) | 0 | 0 | 0 ns |
| tWP/tPWE largura de ~WE (mín) | 45 | 40 | 50 ns |
| tWR/tHA recuperação (mín) | 0 | 0 | 0 ns |
| tDW/tSD setup do dado (mín) | 25 | 25 | 30 ns |
| tDH/tHD hold do dado (mín) | 0 | 0 | 0 ns |
| tWHZ/tHZWE (máx) | 20 | 20 | 25 ns |
| tOW/tLZWE (mín) | 5 | 5 | 5 ns |

Os valores do AS6C62256 foram **(reconstr.)** da tabela; os do CY62256N vêm do texto bruto, em ordem, e conferem com o resumo da Infineon.

**Inferência para o modelo ideal**: memória assíncrona; a escrita ocorre na borda de subida de ~WE ou ~CE (o que vier primeiro, "fim da escrita").
Com ~CE = L e ~WE = H, o dado sai após max(tAA, tACE, tOE). Em simulação lógica sem tempo, basta a tabela-verdade.
Para avisar de dano: VCC > 7 V; disputa de barramento (DQ dirigido de fora enquanto ~OE = L e ~WE = H), com corrente > 20–50 mA.

---

### A8. AT28C16 (Microchip/Atmel) — só limites

Fonte: Atmel doc 0540B (10/98), hospedado pela Microchip — https://ww1.microchip.com/downloads/en/DeviceDoc/doc0540.pdf

| Parâmetro | Valor (fato) |
|---|---|
| Temperatura sob polarização | −55 a +125 °C |
| Armazenamento | −65 a +150 °C |
| Todas as entradas (inclui NC) em relação ao GND | −0,6 a +6,25 V |
| Todas as saídas | −0,6 a VCC + 0,6 V |
| ~OE e A9 | −0,6 a +13,5 V (12 V ± 0,5 V servem para apagar o chip e ler a identificação) |
| VCC de operação | 5 V ± 10% |
| VOL | 0,4 V máx @ IOL = 2,1 mA |
| VOH | 2,4 V mín @ IOH = −400 µA |
| VIL / VIH | 0,8 V máx / 2,0 V mín |
| ICC ativo | 30 mA (com.) / 45 mA (ind.), f = 5 MHz |

**Lacuna**: o datasheet não tem uma linha separada de "VCC máximo absoluto" nem um valor máximo absoluto de corrente de saída.
A tabela só cobre "All Input Voltages ... −0,6 V to +6,25 V" e a corrente de teste de 2,1 mA/−400 µA.
**Inferência**: tratar VCC > 6,25 V como dano, e > 5,5 V como fora da especificação.

---

## Parte B — limites de dano das peças já presentes

### B1. ATmega328P (Arduino Uno)

Fonte: Microchip DS40002061B (2020), "ATmega48A/PA/88A/PA/168A/PA/328/P" —
https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/ATmega48A-PA-88A-PA-168A-PA-328-P-DS-DS40002061B.pdf

| Parâmetro (máx. absoluto, seções 29.1/30.1) | Valor |
|---|---|
| Tensão em qualquer pino exceto RESET | −0,5 V a VCC + 0,5 V |
| Tensão no RESET | −0,5 V a +13,0 V |
| Tensão máxima de operação (VCC) | 6,0 V |
| Corrente DC por pino de I/O | 40,0 mA |
| Corrente DC nos pinos VCC e GND | 200,0 mA |

Recomendado / teste (fato, Tabela 30-1):
- VCC de 1,8 a 5,5 V (a faixa de VCC permitida para 16 MHz vem do gráfico de velocidade e não foi verificada aqui).
- VOL ≤ 0,9 V @ 20 mA (5 V, 85 °C); VOH ≥ 4,2 V @ −20 mA (5 V, 85 °C).
- VIL ≤ 0,3·VCC; VIH ≥ 0,6·VCC (VCC de 2,4 a 5,5 V). Pull-up de 20–50 kΩ.
- Somas (nota 3/4): ΣIOH ≤ 150 mA para {C0–C5, D0–D4, ADC7, RESET} e ≤ 150 mA para {B0–B5, D5–D7, ADC6, XTAL1, XTAL2};
  ΣIOL ≤ 100 mA para {C0–C5, ADC7, ADC6}, ≤ 100 mA para {B0–B5, D5–D7, XTAL1, XTAL2}, e ≤ 100 mA para {D0–D4, RESET}.
- "Pins are not ensured to source/sink current greater than the listed test condition" (20 mA a 5 V).

#### Placa Arduino Uno R3 (docs.arduino.cc)

| Item | Valor | Fonte |
|---|---|---|
| Corrente máxima por pino de I/O | 20 mA ("MAXIMUM current per I/O pin is 20mA") | https://content.arduino.cc/assets/Pinout-UNOrev3_latest.pdf |
| Corrente máxima no pino +3,3 V | 50 mA | mesmo PDF |
| Fusível USB | polyfuse MF-MSMF050-2, 500 mA | https://docs.arduino.cc/resources/schematics/A000066-schematics.pdf |
| Regulador de 5 V | NCP1117ST50T3G (esquemático) / "SPX1117M3-L-5" (lista do datasheet) | esquemático e https://docs.arduino.cc/resources/datasheets/A000066-datasheet.pdf |
| VIN (jack) | 7–12 V recomendado; "can get up to 20V, but this is its absolute maximum rating" | datasheet A000066 (rev. 5, 12/11/2026) |
| VUSB | máx 5,5 V | datasheet A000066 |

Observação: a página da loja Arduino (store.arduino.cc) também cita 40 mA como limite de dano por pino, o que coincide com o máximo absoluto do ATmega.
A Arduino não publica um número de corrente do pino 5V. **Inferência**: com USB, o limite é o polyfuse de 500 mA menos o consumo da placa;
com jack, o limite é a dissipação do NCP1117 ((VIN − 5 V)·I; o NCP1117 limita a ~1 A). Isso não está numerado em documento oficial da Arduino.

### B2. ATtiny85

Fonte: Atmel 2586Q (08/2013), hospedado pela Microchip — https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf

| Parâmetro (máx. absoluto, 21.1) | Valor |
|---|---|
| Pino exceto RESET | −0,5 V a VCC + 0,5 V |
| RESET | −0,5 V a +13,0 V |
| VCC máximo de operação | 6,0 V |
| Corrente DC por pino de I/O | 40,0 mA |
| Corrente DC em VCC e GND | 200,0 mA |

Recomendado / teste (fato, Tabela 21-1): VCC de 1,8 a 5,5 V; VOL ≤ 0,6 V @ 10 mA (5 V); VOH ≥ 4,3 V @ −10 mA (5 V).
**ΣIOL ≤ 60 mA e ΣIOH ≤ 60 mA para todas as portas**. As condições de teste são de 10 mA a 5 V (metade das do ATmega328P).
O pino RESET usado como I/O tem driver fraco.

### B3. ESP32 (ESP32-WROOM-32)

Fonte: Espressif "ESP32-WROOM-32 Datasheet v3.8" — https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf
(chip: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf). O módulo está marcado como **NRND** (não recomendado para projetos novos).

| Parâmetro | Valor (fato) |
|---|---|
| VDD33 máx absoluto | −0,3 a 3,6 V |
| Corrente de saída de I/O acumulada (máx abs) | 1100 mA |
| VDD33 recomendado | 3,0 / 3,3 / 3,6 V |
| Corrente da fonte externa | ≥ 0,5 A |
| VIH | 0,75·VDD a **VDD + 0,3 V** |
| VIL | −0,3 a 0,25·VDD |
| IOH (VDD 3,3 V, VOH ≥ 2,64 V, drive máx) | 40 mA típ (domínios CPU/RTC); 20 mA (VDD_SDIO) |
| IOL (VOL = 0,495 V, drive máx) | 28 mA típ |
| Pull-up / pull-down | 45 kΩ típ |

Nota 2 (fato): com vários pinos fornecendo corrente no mesmo domínio, a corrente por pino cai "from around 40 mA to around 29 mA".
**5 V tolerante? Não** (inferência direta: VIH máx = VDD + 0,3 V ≈ 3,6 V; não há outra tensão máxima para I/O). IOH/IOL são valores **típicos**:
o datasheet não dá um máximo absoluto por pino.

### B4. Diodos

1N4148 — Vishay doc. 81857, rev. 1.6 (07/11/2024) — https://www.vishay.com/docs/81857/1n4148.pdf

| Parâmetro | Valor |
|---|---|
| VRRM (pico repetitivo reverso) | 100 V |
| VR (reverso contínuo) | 75 V |
| IFSM (surto, tp = 1 µs) | 2 A |
| IFRM (pico repetitivo) | 500 mA |
| IF (contínua) | 300 mA |
| IF(AV) (média) | 150 mA |
| Ptot | 440 mW (l = 4 mm, TL = 45 °C); 500 mW (TL ≤ 25 °C) |
| RthJA | 350 K/W |
| VF máx | 1 V @ 10 mA |
| trr | 4 ns (IF = IR = 10 mA) |

1N4001 — Vishay doc. 88503 (29/04/2020) — https://www.vishay.com/docs/88503/1n4001.pdf

| Parâmetro | Valor |
|---|---|
| VRRM | 50 V (1N4001; 1N4007 = 1000 V) |
| VRMS | 35 V |
| IF(AV) | 1,0 A |
| IFSM (meia senoide de 8,3 ms) | 30 A |
| VF máx | 1,1 V @ 1,0 A |
| IR máx | 5 µA @ VRRM, 25 °C |
| TJ | −50 a +150 °C |

### B5. Transistores (onsemi, TO-92)

2N3904 — onsemi 2N3903/D, rev. 9 (ago/2021) — https://www.onsemi.com/download/data-sheet/pdf/2n3903-d.pdf
2N3906 — onsemi 2N3906/D, rev. 4 (fev/2010) — https://www.onsemi.com/download/data-sheet/pdf/2n3906-d.pdf

| Parâmetro | 2N3904 (NPN) | 2N3906 (PNP) |
|---|---|---|
| VCEO | 40 V | 40 V |
| VCBO | 60 V | 40 V |
| VEBO | 6,0 V | 5,0 V |
| IC contínua | 200 mA | 200 mA |
| PD @ TA = 25 °C | 625 mW (derate de 5 mW/°C) | 625 mW (250 mW @ 60 °C) |
| PD @ TC = 25 °C | 1,5 W | 1,5 W |
| RθJA | 200 °C/W | 200 °C/W |
| TJ | −55 a +150 °C | −55 a +150 °C |
| hFE mín @ 0,1/1/10/50/100 mA | 40/70/100 (máx 300)/60/30 | 60/80/100 (máx 300)/60/30 |
| VCE(sat) máx @ 10/1 mA; 50/5 mA | 0,2 V; 0,3 V | 0,25 V; 0,4 V |
| VBE(sat) @ 10/1 mA; 50/5 mA | 0,65–0,85 V; 0,95 V | 0,65–0,85 V; 0,95 V |
| fT mín | 300 MHz | 250 MHz |

### B6. LED indicador de 5 mm (Vishay TLHR5400, vermelho, GaAsP/GaP)

Fonte: Vishay doc. 83012, rev. 2.7 (25/01/2024) — https://www.vishay.com/docs/83012/tlhr540x_tlhy540x.pdf

| Parâmetro | Valor |
|---|---|
| VR (reverso) | 6 V |
| IF contínua | 30 mA (Tamb ≤ 65 °C) |
| IFSM (surto) | 1 A (tp ≤ 10 µs) |
| PV | 100 mW (Tamb ≤ 65 °C) |
| Tj | 100 °C |
| RthJA | 350 K/W |
| VF | 2 V típ, 3 V máx @ 20 mA |
| Intensidade | 10 mcd típ @ 10 mA |

O datasheet não traz corrente de pico repetitiva (só o surto de 10 µs). LEDs brancos/azuis (InGaN) costumam ter VR = 5 V e VF ≈ 3 V; isso não foi verificado aqui.

### B7. Capacitor eletrolítico de alumínio (Nichicon)

Fonte: Nichicon "Technical Notes CAT.8101H — General Descriptions / Application Guidelines for Aluminum Electrolytic Capacitors" —
https://www.nichicon.co.jp/english/products/pdf/aluminum.pdf

Fatos:
- "Aluminum electrolytic capacitors are polarized. Make sure no reverse voltage or AC voltage is applied."
- Tensão reversa (2-3-2): "Normally a cathode foil has a withstand voltage of about 1 V because of the natural oxide layer ... If the capacitor
  is exposed to a reverse voltage over the withstand voltage, the internal pressure will rise and activate the pressure relief vent."
  Com tensão reversa alta, a corrente sobe, gera calor (W = Vc·Ic) e gás e abre a válvula "in a short period of time". Com tensão reversa baixa,
  a capacitância cai e o tan δ aumenta.
- Sobretensão (2-3-3): "the leakage current rises sharply when voltages above the rated voltage [are] applied ... the anode foil undergoes insulation
  breakdown, a large amount of current will flow and cause the internal pressure to rise within a short period of time." A energia é ½CV²;
  quanto maior a tensão, mais severa a abertura da válvula e mais provável um curto entre as folhas.
- Pico (DC + ripple) não pode passar da tensão nominal. Em série, usar resistores de equilíbrio.
- Modos de falha: válvula aberta e vazamento de eletrólito (reversa, sobretensão, ripple extremo ou AC); curto (sobretensão, pulsos); circuito aberto.

**Inferência para o simulador**: avisar quando a tensão reversa passar de ~1 V e "estourar" quando passar bastante de ~1 V por tempo/energia;
avisar acima da tensão nominal e estourar acima dela com corrente de fuga crescente. O fabricante não dá um limiar numérico para a ruptura,
só o mecanismo.

### B8. Relé de 5 V (Omron G5LE)

Fonte: Omron G5LE, Cat. No. K100-E1-08 — https://omronfs.omron.com/en_US/ecb/products/pdf/en-g5le.pdf

Fatos lidos: bobinas de 5, 12 e 24 VDC; contatos de 10 A @ 120 VAC e 8 A @ 30 VDC (resistivo, tipo padrão); máximo de chaveamento de 250 VAC / 125 VDC;
tempo de operação de 10 ms máx; liberação de 5 ms máx.

**Lacuna**: a tabela "Coil Ratings" (corrente nominal, resistência, tensões de operação/liberação) usa fonte CJK e não saiu na extração de texto.
A página do produto devolveu 403. O guia de seleção da Omron fala em ~400 mW para essa classe de bobina.
**Inferência**: com 400 mW a 5 V, ≈ 80 mA e ≈ 62,5 Ω. Conferir a tabela no PDF antes de usar.

### B9. Multímetro portátil (Fluke)

- Fluke 117 (data sheet 2634028 e manual 11x): só tem entrada de **A**, sem mA. Fusível de 11 A, 1000 V FAST (Fluke PN 803293), capacidade mínima
  de interrupção de 17 000 A. Mede até 10 A; "20 A overload for 30 seconds", com > 10 A sem precisão especificada.
  https://media.fluke.com/2dfc6457-2f44-4764-9c44-b10600670b0b_original%20file.pdf ; https://www.fluke.com/en-us/product/electrical-testing/digital-multimeters/fluke-117
- Fluke 87V (referência com entrada de mA/µA): fusível F1 de 440 mA (44/100 A), 1000 V FAST, na entrada mA/µA; fusível F2 de 11 A, 1000 V FAST,
  na entrada A. "10 A continuous, 20 A for 30 seconds maximum" (87V MAX: até 35 °C; de 10 a 20 A, 30 s ligado e 5 min desligado).
  https://assets.fluke.com/manuals/87______umeng0800.pdf

**Inferência para o simulador**: jack mA queima o fusível com > 0,44 A; jack A tolera 10 A contínuos e até 20 A por 30 s, e o fusível de 11 A abre
em sobrecarga sustentada (a curva tempo×corrente do fusível não foi verificada). Medir tensão com as pontas no jack A ou mA é um curto pelo fusível.

### B10. Fonte de bancada (Rigol DP800)

Fonte: Rigol "DP800 Series Programmable Linear DC Power Supply — Data Sheet" —
https://www.rigol.com/dam/global/downloads/brochures/en/data-sheet/dc-powers/DP800_DataSheet_EN.pdf

| Modelo / canal | Faixa | OVP/OCP ajustável |
|---|---|---|
| DP832A CH1 | 0–30 V / 0–3 A | 1 mV–33 V / 1 mA–3,3 A |
| DP832A CH2 | 0–30 V / 0–3 A | 1 mV–33 V / 1 mA–3,3 A |
| DP832A CH3 | 0–5 V / 0–3 A | 1 mV–5,5 V / 1 mA–3,3 A |
| DP831A CH2 / CH3 | 0–30 V/2 A; 0 a −30 V/2 A | |

Também: precisão de OVP/OCP de 0,5% + 0,5 V / 0,5% + 0,5 A; ripple < 350 µVrms / 2 mVpp; proteções OVP/OCP/OTP.

CV/CC (Keysight, blog "Characteristics of DC Power Source Priority Modes", 06/02/2024 —
https://www.keysight.com/blogs/en/tech/bench/2024/02/06/characteristics-of-dc-power-source-priority-modes ; e "The difference between constant
current and current limit" — https://powersupply.blogs.keysight.com/2013/01/the-difference-between-constant-current.html):
- Fato: o modo é determinado pela tensão ajustada, pela corrente ajustada e pela resistência de carga. O cruzamento acontece em `R_carga = V_set / I_set`.
- Fato: em uma fonte CV/CC há transição abrupta entre CV e CC; em uma fonte CV/"current limit", o ponto de cruzamento é menos definido.

**Inferência para o modelo ideal**:
- Se R_carga > Vset/Iset: modo CV, com V = Vset e I = Vset/R.
- Senão: modo CC, com I = Iset e V = Iset·R.
- Se OVP/OCP estiver ligado e V > OVP ou I > OCP: a saída desliga (latch), indicação na UI.

---

## Lacunas e cautelas

1. Várias tabelas TI (NE555, LM358 abs max, LM393 abs max, L293 ICC) ficaram embaralhadas na extração. Os valores marcados **(reconstr.)** foram realinhados com cuidado, mas devem ser conferidos no PDF antes de virar constante no código.
2. ST L7805 não foi baixado (o site recusou acesso automático); a seção usa só o TI LM340/LM7805.
3. AT28C16: não há linha de VCC máximo absoluto nem de corrente de saída máxima absoluta.
4. Omron G5LE: a tabela de bobina não foi lida.
5. Arduino Uno: não há número oficial para a corrente do pino 5V.
6. ESP32: correntes de pino são típicas, sem máximo absoluto por pino.
