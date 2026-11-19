# Comparação técnica: CIs de média escala e computador de 8 bits

Consulta: 19/11/2026. Escopo: branch `claude/computador-8-bits`, a partir da 0.46.0.

## Fatos

- Antes desta branch: limites de 256 peças, 512 fios e 420 pinos por peça (`workbench.h`). Chips
  personalizados não economizam peças: o solver recebe as peças elementares ([CHIPS_RECURSIVOS.md](CHIPS_RECURSIVOS.md)).
- `Workbench` media 227 KB e é copiado por valor (histórico com 2 × 24 cópias no heap, ~20 variáveis
  locais). Os executáveis usam pilha de 64 MB; a thread de análise usa o mesmo tamanho padrão.
- Portas soltas e o 74HC00/08/32/86 já usam o [XSPICE](https://ngspice.sourceforge.io/xspice.html):
  ponte `adc_bridge`, primitiva digital, `dac_bridge` e duas chaves para VCC/GND por saída.
- Teste local com o ngspice do `setup-ngspice.ps1` (19/11/2026): `d_and` aceita vetor de entradas com
  inversão por `~` (`[~a b]`), e `d_dff` com set/reset assíncronos funciona na mesma netlist.
- Medição da 0.43.0 ([CHANGELOG](../CHANGELOG.md)): decodificador de ~30 portas a pouco mais de 1×;
  contador de 4 bits com decodificador a ~0,6×. Cada porta solta paga pontes analógicas próprias.
- Pinagem do SN74HC161 conferida no datasheet TI SCLS297D (rev. set/2003), PDIP-16 vista de cima:
  1 CLR, 2 CLK, 3–6 A–D, 7 ENP, 8 GND, 9 LOAD, 10 ENT, 11 QD, 12 QC, 13 QB, 14 QA, 15 RCO, 16 VCC.

## Hipóteses (a medir nesta branch)

- Um CI com toda a lógica interna em primitivas digitais custa pontes só nos pinos. Um contador
  inteiro deve custar menos que as ~15 portas soltas equivalentes.
- Um computador no estilo SAP-1 (Malvino, *Digital Computer Electronics*) com CIs de média escala cabe
  em ~25 CIs e ~350 fios, abaixo até dos limites antigos. Os limites maiores servem a montagens só com portas.
- Pinagens de SN74HC173, SN74HC245, SN74HC283 e AT28C16 seguem a pinagem padrão da família (JEDEC/fabricantes).
  O texto dos PDFs TI (CD74HC173, CD74HC283, SN74HC245) e Microchip (doc0540) não pôde ser extraído
  nesta máquina. **Conferir visualmente antes da release.**

## Alternativas

| Tema | Opção | Vantagem | Custo/risco |
|---|---|---|---|
| Limites | Subir tetos para 1024/2048 | 2 linhas; formato e cópias inalterados | `Workbench` ~910 KB; histórico ~44 MB por contexto de edição |
| Limites | Listas dinâmicas | Sem teto | Refatoração de ~170 usos, cópias, arquivo e solver |
| CIs | Lógica digital XSPICE + pontes nos pinos | Rápido; reaproveita o padrão das portas | Saída tri-state precisa de habilitação própria |
| CIs | Transistores/portas analógicas (B-sources) | Comportamento elétrico mais rico | Lento para dezenas de CIs |
| Memória | EEPROM AT28C16 sintetizada em portas digitais a partir do conteúdo | Programa pré-carregado; leitura exata | Endereços listados viram portas; gravação (WE) não modelada |
| Memória | `d_ram` do XSPICE | RAM real com escrita | Sem conteúdo inicial: exige programar à mão por chaves |
| Controle | Microcódigo em duas EEPROMs | Poucos CIs, fácil de ler e alterar | Exige o conteúdo da EEPROM editável |
| Controle | Lógica fixa com portas | Sem memória | Muitas peças e fios |

## Decisões

- Limites: 1024 peças e 2048 fios (decisão do responsável em 19/11/2026). Listas dinâmicas continuam pendentes para a 1.0.
- Novos tipos: 74HC161, 74HC173, 74HC245, 74HC283 e AT28C16. Desenho DIP genérico de N pinos com o
  mesmo passo do 74HC00. Formato de projeto 35 por causa dos tipos novos.
- A EEPROM é só leitura no modelo: o conteúdo vem do texto da peça (hexadecimal), e endereços não
  listados leem `FF`, como uma memória apagada. WE é ignorado e não grava. O limite fica declarado no guia.
- Computador de exemplo: SAP-1 com ROM no lugar da RAM (LDA, ADD, SUB, OUT, HLT), barramento de 8 bits,
  microcódigo em EEPROM e saída em LEDs. Um teste roda o programa no ngspice e confere o valor.
