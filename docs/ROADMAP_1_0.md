# Caminho para a 1.0.0 — estado em 16/11/2026 (v0.37.0)

> **Visão consolidada:** recursos atuais, histórico e pendências da 1.0. Conferida com o código da v0.37.0 e o [CHANGELOG](../CHANGELOG.md).

Documento de continuidade para quem retomar o trabalho (pessoa, Codex ou Claude). Fatos verificados
no repositório, decisões e pendências. Detalhes de cada item: `CONTINUIDADE.md`, `DECISOES.md`,
`META_1_0.md`, `CATALOGO_COMPONENTES.md` e `QUALIDADE_DE_USO.md`. A 1.0 **não** foi atingida.

## 1. Histórico desde 14/11/2026

Primeiro commit em 14/11 12:21 (protótipo SDL/OpenGL). Até 15/11: 100+ commits e 38 tags.

| Período | Versões | O que entrou |
|---|---|---|
| 14/11 manhã | 0.1–0.14 | Protótipo de circuito fixo, janela SDL/OpenGL, salvar/carregar, histórico (removido na 0.31.0). |
| 14/11 tarde | 0.15–0.16 | Bancada livre: fios com joelhos, breadboard, zoom, rotação; DC experimental via ngspice. |
| 14/11 noite | 0.17.x | Integração do trabalho do Claude: cálculo automático, play/pausa; portas, clock, DFF, 74HC00. Refatorações: AppState/SimState, entrada/desenho/comandos separados, revisões monótonas. |
| 15/11 | 0.18–0.19 | Slots de projeto (F3), biblioteca com miniaturas, seleção por área, copiar/colar, blocos reutilizáveis (Ctrl+G). |
| 15/11 | 0.20–0.21 | Registro de componentes (IDs, pinos, funções); editor isolado do interior de blocos; nomes UTF-8. |
| 15/11 | 0.22–0.25 | Grade/encaixe configuráveis, joelhos por duplo clique, escala de peças, tema escuro, fonte suavizada, novos CIs (74HC08/32/86), potenciômetro e botão. |
| 15/11 | 0.26–0.27 | Terminais flexíveis com contato direto; **sessão ngspice contínua** (capacitor e DFF mantêm estado). |
| 15/11 (Codex, sem commit; integrado pelo Claude) | 0.28.0 | 14 componentes analógicos e sensores com slider de ambiente; revisão de arte por close-ups; brilho do LED em tempo real com potenciômetro. |
| 15/11 (Claude) | 0.28.0 | Correção do travamento ao desligar fonte com capacitor carregado (contatos 1 mΩ/10 GΩ). |
| 15/11 (Claude) | 0.29.0 | Menus redesenhados, diálogo de configurações único, preferências persistentes, arte de capacitor/fonte/interruptor/fototransistor; `app_layout` compartilhado. |
| 15/11 (Claude) | 0.30.0 | Diálogo com 15 exemplos verificados; correção do 74HC00 que abortava na sessão contínua. |
| 15/11 (Claude) | 0.31.0 | Lâmpada; remoção do protótipo legado; este documento. |
| 15/11 (Claude) | 0.32.0 | Várias medições e estado por peça; 17 dispositivos, sensores e instrumentos; displays no corpo de blocos; 18 exemplos. |
| 15–16/11 | 0.33–0.34.1 | Ajustes de contato e corrente; placas programáveis e LCD I2C; transparência; correções de entradas lógicas e da biblioteca de chips. |
| 16/11 | 0.35.0 | Interface em português e inglês; catálogo de textos e revisão de nomes. |
| 16/11 | 0.36.0 | CMake, ZIP portátil e instalador NSIS por usuário; dependências fixadas, fontes e hashes. Pré-release publicada no GitHub. |
| 16/11 | 0.37.0 | Rotação de chips por R e contatos diretos entre terminais, breadboards e portas de chips. Formato 29. |

## 2. O que existe hoje

- **Editor**: componentes, fios com joelhos, breadboard, terminais flexíveis, grade/encaixe, escala,
  rotação, seleção por área, copiar/colar, desfazer/refazer, blocos com editor isolado, slots.
- **Simulação**: ngspice em processo isolado por contexto; sessão contínua com `alter`/`resume`
  (C, L e DFF mantêm estado); corrente nos fios com direção; proteção didática contra queima.
- **Catálogo (58 tipos)**: LED RGB, 7 segmentos, DIP, DPST, relé, motores CC/vibração/encoder, servo, piezo, gerador de função, multímetro, osciloscópio, sensores PIR/gás/IR/distância, e ainda fonte, baterias (9 V, AA, moeda), interruptor, botão, resistor, potenciômetro,
  capacitor, indutor, LED, lâmpada, diodo, NPN/PNP, LDR, flex, FSR, inclinação, fotodiodo,
  fototransistor, TMP36, umidade do solo, breadboard, AND/NAND/OR/XOR/NOT/NOR/XNOR, clock, DFF,
  74HC00/08/32/86, Arduino Uno, ESP32, ATtiny85 e LCD 16x2 I2C.
- **Interface**: cabeçalho, biblioteca com abas, inspector técnico, configurações persistentes,
  18 exemplos, tema escuro, dois idiomas e rodapé com dicas por contexto.
- **Qualidade**: build estrito (`-Wall -Wextra -Werror`), testes nativos, autoteste GUI e
  capturas reprodutíveis (`--capture-*`, `tools/capture-components.ps1`).
- **Distribuição**: ZIP e instalador Windows experimentais, com dependências, licenças,
  fontes e hashes. Ainda sem assinatura digital ou validação numa VM limpa.

## 3. Revisão geral do código (origem em 15/11; estado revisto em 16/11/2026)

Feito nesta revisão: geometria da interface centralizada (`app_layout`), mapeamento de
configurações único (`app_settings`), preferências em módulo puro testado (`app_preferences`),
remoção das teclas S/L legadas e do protótipo inteiro não usado.

Dívidas encontradas, em ordem de impacto:

1. **`app_render.c`**: primitivas, widgets, componentes e painéis no mesmo
   arquivo. Extrair `render_primitives`, `render_parts` e `render_ui` (refatoração pura, validar
   com capturas idênticas byte a byte).
2. **`write_netlist`** em `netlist.c`: dispositivos já têm `netlist_devices.c`; ainda separar
   as famílias restantes (fontes, passivos, semicondutores, portas e CIs).
3. **Autoteste GUI em duas funções grandes** (`dev_scenario_queue`/`validate`):
   migrar para uma tabela de fases `{ação, verificação}`; hoje inserir uma fase no meio exige renumerar.
4. ~~Uma corrente por peça~~: resolvido na 0.32.0 (`component_devices`, até 8 medições e `states`).
   Falta proteção de queima por elemento (hoje RGB/7 segmentos não "queimam").
5. **Limites fixos**: 256 peças, 512 fios, 16 joelhos por fio, 8 blocos, 9 slots. A meta 1.0 exige
   capacidades dinâmicas.
6. **Tipos persistidos como códigos numéricos** (formato 32); a migração para IDs textuais já foi
   decidida e deve usar um número de formato novo, mantendo leitura das versões anteriores.
7. Funções ainda acima de 100 linhas: `workbench_read_project`, `input_edit_key`, `part_draw`,
   `input_zoom`, `analyze` em `spice_process.c`.

## 4. O que falta para a 1.0.0, em ordem de dependência

### 4.1 Infraestrutura (antes do catálogo restante)
- [x] Elementos internos por peça e estado por peça (0.32.0). Falta proteção de queima por elemento (RGB/7 segmentos).
- [ ] Capacidades dinâmicas (peças, fios, joelhos, blocos) sem copiar `Workbench` inteiro por valor.
- [ ] IDs textuais no formato de arquivo.
- [ ] Sessão contínua que preserve estado ao mudar topologia/frequência (hoje reinicia).
- [x] Histórico da sessão contínua limitado à última amostra no ngspice (0.42.0).
- [ ] Divisão de `app_render.c` e `write_netlist` (seção 3).

### 4.2 Catálogo pendente (`CATALOGO_COMPONENTES.md`)
Feito na 0.32.0: saídas, 7 segmentos (visível no corpo de blocos), DIP 4 vias, DPST, relé,
sensores PIR/gás/IR/distância e os três instrumentos. Ainda falta:
- ~~LCD 16x2 I2C~~ e microcontroladores: feitos na 0.34.0 (subconjunto Arduino, editor lateral).
- Sensor ultrassônico com trigger/echo (o de distância atual é IR analógico, declarado).
- DIP de tamanho configurável (hoje 4 vias); servo por largura de pulso real.
- CIs: inventário verificável do Tinkercad (555, 74HC02/04/14/595, 4511, L293D, LM741...).
- Microcontrolador: feito (0.34.0); falta servo por pulso e ultrassônico usando-o.

### 4.3 Uso e acabamento (`QUALIDADE_DE_USO.md`)
- [ ] Escala completa da interface (hoje só o tamanho do texto muda).
- [ ] Inspector que não cubra peças (recolher/mover quando sobrepõe a seleção).
- [x] Ritmo observado e escolhido visíveis na interface (0.42.0); o ritmo real depende do circuito.
- [x] Pacotes experimentais com runtime, host, licenças e fontes (0.36.0).
- [ ] Validar numa VM Windows limpa, assinar os artefatos e testar atualização entre versões.
- [ ] Automatizar a publicação ao enviar tags; hoje há um comando explícito por versão.

## 5. Próximo passo recomendado

1. Proteção de queima por elemento (RGB e 7 segmentos) e divisão de `app_render.c`.
2. CIs do inventário Tinkercad com pinagem conferida (começar por 74HC04, 74HC02 e 555).
3. Exemplos com placas programáveis; ultrassônico e servo por pulso sobre o microcontrolador.
4. Capacidades dinâmicas e IDs textuais no formato de arquivo.

Comandos de verificação: `./build.ps1 -GuiTests` (gera o executável único `build/build.exe`);
revisão de arte: `./tools/capture-components.ps1 -Destination build/component-review/<nome>`.
