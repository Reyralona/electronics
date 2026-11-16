# Caminho para a 1.0.0 — estado em 15/11/2026 (v0.31.0)

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

## 2. O que existe hoje

- **Editor**: componentes, fios com joelhos, breadboard, terminais flexíveis, grade/encaixe, escala,
  rotação, seleção por área, copiar/colar, desfazer/refazer, blocos com editor isolado, slots.
- **Simulação**: ngspice em processo isolado por contexto; sessão contínua com `alter`/`resume`
  (C, L e DFF mantêm estado); corrente nos fios com direção; proteção didática contra queima.
- **Catálogo (37 tipos)**: fonte, baterias (9 V, AA, moeda), interruptor, botão, resistor, potenciômetro,
  capacitor, indutor, LED, lâmpada, diodo, NPN/PNP, LDR, flex, FSR, inclinação, fotodiodo,
  fototransistor, TMP36, umidade do solo, breadboard, AND/NAND/OR/XOR/NOT/NOR/XNOR, clock, DFF,
  74HC00/08/32/86.
- **Interface**: cabeçalho, biblioteca com abas, inspector técnico, configurações persistentes,
  15 exemplos, tema escuro, rodapé com dicas por contexto.
- **Qualidade**: build estrito (`-Wall -Wextra -Werror`), ~40 testes nativos, autoteste GUI com
  189 fases, capturas reprodutíveis (`--capture-*`, `tools/capture-components.ps1`).

## 3. Revisão geral do código (15/11/2026)

Feito nesta revisão: geometria da interface centralizada (`app_layout`), mapeamento de
configurações único (`app_settings`), preferências em módulo puro testado (`app_preferences`),
remoção das teclas S/L legadas e do protótipo inteiro não usado.

Dívidas encontradas, em ordem de impacto:

1. **`app_render.c` com 2,5 mil linhas**: primitivas, widgets, componentes e painéis no mesmo
   arquivo. Extrair `render_primitives`, `render_parts` e `render_ui` (refatoração pura, validar
   com capturas idênticas byte a byte).
2. **`write_netlist` (~400 linhas)** em `netlist.c`: dividir por família (fontes, passivos,
   semicondutores, portas, CIs) antes de crescer o catálogo.
3. **Autoteste GUI em duas funções gigantes** (`dev_scenario_queue`/`validate`, 550 e 350 linhas):
   migrar para uma tabela de fases `{ação, verificação}`; hoje inserir uma fase no meio exige renumerar.
4. **Uma corrente por peça** (`SimAutoResult.currents[i]`) e um único `on` por peça: bloqueia LED RGB,
   sete segmentos, DIP, LCD, relé com vários contatos (ver seção 4).
5. **Limites fixos**: 64 peças, 128 fios, 16 joelhos por fio, 8 blocos, 9 slots. A meta 1.0 exige
   capacidades dinâmicas.
6. **Tipos persistidos como códigos numéricos** (formato 25); a migração para IDs textuais já foi
   decidida e deve usar um número de formato novo, mantendo leitura 1–25.
7. Funções ainda acima de 100 linhas: `workbench_read_project`, `input_edit_key`, `part_draw`,
   `input_zoom`, `analyze` em `spice_process.c`.

## 4. O que falta para a 1.0.0, em ordem de dependência

### 4.1 Infraestrutura (antes do catálogo restante)
- [ ] Elementos internos por peça: correntes, estados e proteção indexados por elemento; persistência.
- [ ] Capacidades dinâmicas (peças, fios, joelhos, blocos) sem copiar `Workbench` inteiro por valor.
- [ ] IDs textuais no formato de arquivo.
- [ ] Sessão contínua que preserve estado ao mudar topologia/frequência (hoje reinicia) e vetores do
      solver limitados (hoje crescem até reiniciar).
- [ ] Divisão de `app_render.c` e `write_netlist` (seção 3).

### 4.2 Catálogo pendente (`CATALOGO_COMPONENTES.md`)
- Saídas: LED RGB, motor de vibração, motor CC, motor CC com encoder, micro servo, piezo.
- Displays: sete segmentos e LCD 16x2 I2C, **visíveis no exterior de chips personalizados**.
- Chaves: DIP e DPST configuráveis; relé.
- Sensores: infravermelho, distância (trigger/echo), PIR, gás.
- CIs: inventário verificável dos CIs do Tinkercad com pinagem conferida em datasheet.
- Instrumentos com pontas reais: multímetro, osciloscópio, gerador de função.

### 4.3 Uso e acabamento (`QUALIDADE_DE_USO.md`)
- [ ] Escala completa da interface (hoje só o tamanho do texto muda).
- [ ] Inspector que não cubra peças (recolher/mover quando sobrepõe a seleção).
- [ ] Tempo simulado visível como razão (hoje ~1 ms simulado por segundo real; ajustar duração).
- [ ] Empacotamento distribuível (runtime ngspice, `spice-live-host.exe`, licenças).

## 5. Próximo passo recomendado

1. Refatorar elementos internos por peça (4.1, primeiro item) com testes no ngspice.
2. Com isso, LED RGB e sete segmentos (este já tem o desenho de dígitos em `seven_segment_digit`).
3. Exposição de displays no exterior de blocos, depois LCD.
4. Instrumentos (multímetro primeiro, por reaproveitar a leitura de tensão/corrente).

Comandos de verificação: `./build.ps1 -OutputName bancada-next -GuiTests` (não fecha a Bancada aberta);
revisão de arte: `./tools/capture-components.ps1 -Destination build/component-review/<nome>`.
