# Revisão de qualidade de código e direção de arquitetura

> **Revisão de código** (14/11/2026, base 0.16.0) e direção de arquitetura que o código novo deve seguir. Parte dos itens já foi feita; veja [ROADMAP_1_0.md](ROADMAP_1_0.md).

Data: 14/11/2026. Base revisada: commit `697ec4b` (`feature/simulation-evolution`, versão 0.16.0), sem as alterações locais não commitadas.
Autor: Claude (revisão externa), a pedido do responsável pelo projeto. Público: Codex.
Leia junto com `VISAO.md`, `META_1_0.md`, `DECISOES.md` e `VERSIONAMENTO.md`. Este documento **não muda a visão**: ele trata de como o código precisa evoluir para sustentá-la.

## Estado da revisão em 16/11/2026 (0.36.0)

As seções P0–P9 descrevem o código de 14/11. Não são uma lista de falhas atuais.
As convenções da seção 6 continuam válidas. Para as próximas tarefas, use o [roadmap](ROADMAP_1_0.md).

| Item original | Estado atual |
|---|---|
| P0: DLLs ausentes | Preparação corrigida; pacotes incluem runtime e licenças. Distribuição estável ainda exige validação adicional. |
| P1: janela monolítica | Estado, eventos, comandos, renderização e cenários extraídos. Persistem funções grandes em outros módulos. |
| P2: registro de peças | Registro inicial com IDs, pinos, nomes e faixas. Geometria, emissores e parâmetros ainda não estão todos no registro. |
| P3–P5: capacidades, parâmetros e API | Limites fixos e parâmetros embutidos continuam. Ainda falta uma API unificada de análise. |
| P6: cálculo padrão | ngspice automático é o caminho padrão. A API booleana antiga ainda existe no modelo. |
| P7: transiente em lote | Sessão persistente implementada. Topologia/frequência ainda reiniciam o estado; vetores do solver crescem. |
| P8–P9: estilo e manutenção | Código extraído, revisões monótonas e caminhos relativos implementados. Legado removido na 0.31.0. |

As perguntas da seção 7 também são históricas: cálculo automático, extração dos módulos e remoção
do protótipo já ocorreram. O modelo SPICE livre continua uma proposta, sem implementação.

## 0. Pedido do responsável (a meta deste documento)

> "Está indo num caminho que estou gostando, mas tem que ser algo muito bem feito e que me permita experimentar com qualquer coisa que está no código, sem limitações (a não ser de hardware)."

Tradução operacional, para usar como critério de aceite em todo trabalho futuro:

1. **Nenhum limite arbitrário silencioso.** Todo limite é de um destes três tipos:
   - **(a) Hardware/memória:** alocação dinâmica; falha só quando faltar memória, com mensagem clara.
   - **(b) Configurável:** existe um padrão documentado, mas o valor é ajustável em configurações/modo avançado e salvo no projeto.
   - **(c) Limitação de modelo:** documentada e **visível na interface**, nunca escondida atrás de "cálculo indisponível".
2. **Tudo o que o código calcula pode ser inspecionado.** Netlist gerado, saída bruta do ngspice, avisos, parâmetros de modelo e estados.
3. **Tudo o que é parâmetro de modelo é dado, não constante em `printf`.** Modelo do LED, limiares das portas, atrasos, `Ron` do CI e limiar visual de 1 µA.
4. **Adicionar um componente é uma mudança local**, não uma caça a `PART_*` espalhados por seis arquivos.

## 1. Fatos verificados

- `./build.ps1` compila com `-std=c11 -Wall -Wextra -Werror` e todos os testes passam (executado em 14/11/2026, com o runtime `build/spice-local`).
- Tamanho de `src/`: cerca de 3.500 linhas. Porém [src/workbench_window.c](../src/workbench_window.c) tem **80 KB em 935 linhas**, com linhas de até **997 caracteres**. `main()` começa na linha 288 e vai até o fim do arquivo: cerca de 650 linhas e mais de 40 variáveis locais de estado.
- Referências a `PART_*` por arquivo: `workbench_window.c` 122, `netlist.c` 55, `workbench.c` 45, `workbench_file.c` 15, `workbench_examples.c` 9, `spice_process.c` 4.
- O formato de arquivo grava o tipo como **inteiro do enum** e subiu **15 versões**, a maioria só para aceitar um tipo novo de componente (ver [src/workbench_file.c:46](../src/workbench_file.c#L46)).
- O caminho `"build/spice-local/ucrt64/bin/ngspice_con.exe"` aparece **10 vezes** em `workbench_window.c` e é relativo ao diretório de trabalho, não ao executável.
- Há dois simuladores: o booleano (`bench_simulate`) e o SPICE. [src/workbench.c:203](../src/workbench.c#L203) desativa o booleano na presença de resistor, capacitor, indutor, porta ou CI. Isso produz o "CÁLCULO INDISPONÍVEL" que o usuário encontra na maioria das montagens: ele precisa saber que deve apertar `P`.
- Código legado (`window.c`, `circuit.c`, `simulation.c`, `project.c`, `history.c`) convive em `src/` e só é compilado com `-Legacy`. Existem dois sistemas de histórico.

## 2. O que está bom e deve ser preservado

- **Camadas corretas:** `Workbench` (modelo) → `netlist.c` (tradução) → `spice_process.c` (processo isolado) → `spice_job.c` (thread) → UI. O modelo não depende de SDL nem de OpenGL e tem testes próprios.
- **Robustez:** timeout, snapshot que descarta resultado obsoleto, validação de `isfinite`, parser estrito e rejeição de ciclos de fios ideais em vez de exibir valor arbitrário.
- **Honestidade do modelo:** limites documentados, testes contra soluções analíticas (RC, RL), migração de formatos antigos.
- **Disciplina de processo:** branches, CHANGELOG, DECISOES datado, capturas reproduzíveis.

Nada abaixo pede para reescrever o projeto. São refatorações incrementais sobre uma base que funciona.

## 3. Problemas, por prioridade

### P0. O runtime ngspice só funciona com o MSYS2 no PATH (causa do "cálculo indisponível" relatado)
Verificado em 14/11/2026: `ngspice_con.exe` depende de `libgcc_s_seh-1.dll`, `libgomp-1.dll`, `libstdc++-6.dll` e `libwinpthread-1.dll`. Nenhuma delas é extraída por `tools/setup-ngspice.ps1`; todas existem só em `C:\msys64\ucrt64\bin`. O `build.ps1` coloca esse diretório no PATH, então os testes passam. Ao abrir o `bancada.exe` fora dessa sessão, `P` e `T` resultam em "ngspice falhou", e a montagem fica em "CÁLCULO INDISPONÍVEL". O `bancada.exe` também depende de `SDL3.dll`, que não está no PATH do sistema.
Correção local já aplicada (fora do git): as quatro DLLs foram copiadas para `build/spice-local/ucrt64/bin` e `SDL3.dll` para `build/`. Com o PATH limpo, DC e transitório funcionam.
*Correção durável a fazer:* `setup-ngspice.ps1` deve extrair também o pacote de runtime do GCC (`mingw-w64-ucrt-x86_64-gcc-libs` e `-libwinpthread`), e `build.ps1` deve copiar `SDL3.dll` para `build/`. Adicionar um teste que rode o ngspice com PATH mínimo. A mensagem "ngspice falhou" deve incluir o código de saída (por exemplo, `0xC0000135` significa DLL ausente).

### P1. `workbench_window.c` é um monólito (bloqueia todo o resto)
Renderização, entrada, máquina de estados da simulação, regras de exibição de LED, mais de 20 modos `--capture-*` e um autoteste de GUI com mais de 70 fases vivem dentro de `main()`. Consequências:
- Uma regra como "o estado da simulação é válido?" está espalhada (ex.: linhas 572, 764–781, 790). É daí que nascem inconsistências de UI.
- Cada feature nova aumenta o arquivo e o risco de conflito de merge.
- Não é possível testar lógica de interação sem abrir janela.

### P2. Não existe registro de componentes
Cada tipo é definido implicitamente por `if/switch` em pelo menos seis arquivos: pinos, geometria, desenho, nome, valor padrão, faixa, netlist, persistência e inspeção. Adicionar um componente exige editar tudo, e o formato de arquivo sobe de versão a cada vez. Isso contraria a meta 1.0 ("catálogo abrangente de componentes interoperáveis").

### P3. Limites fixos que impedem experimentar
| Limite | Onde | Tipo atual | Deveria ser |
|---|---|---|---|
| 64 componentes | `WORKBENCH_MAX_COMPONENTS`; também acoplado a `uint64_t leds_on` em `SpiceTrace` | fixo | dinâmico (a) |
| 128 fios | `WORKBENCH_MAX_WIRES`; acoplado a `wires_forward[..][2]` (128 bits) | fixo | dinâmico (a) |
| 420 pinos **por peça** | `WORKBENCH_MAX_PINS` (por causa da breadboard) | desperdício de memória e arrays enormes | pinos por tipo, via registro |
| 16 joelhos por fio | `WIRE_MAX_KNEES` | fixo | dinâmico (a) |
| 2048 amostras e passo/duração ≤ 500 | `SPICE_TRANSIENT_MAX_SAMPLES`, `spice_process.c:24`, UI | fixo | configurável (b) |
| Saída ngspice ≤ 16 MiB, timeout 10 s | `spice_process.c:20,41` | fixo | configurável (b) |
| Desfazer: 24 passos | `BENCH_HISTORY_LIMIT` | fixo | configurável (b) |
| Valores de 1e-12 a 1e12 para tudo | `workbench.c`, `netlist.c`, UI | um limite único para todos | faixa por parâmetro, no registro |
| Uma sonda por vez | API de transitório | fixo | várias sondas por análise |
| Qualquer `Warning:` do ngspice invalida o resultado | `spice_process.c` | rejeição silenciosa | mostrar o aviso e deixar o usuário decidir |
| Mensagem de erro com 128 caracteres; "ngspice falhou" sem detalhe | `SpiceJob.message`, `spice_process.c` | informação perdida | log completo acessível na UI |

### P4. Parâmetros de modelo embutidos no código
Exemplos em `netlist.c:118–125`: `LED_GENERIC D(IS=1e-18 N=2)`, `adc_bridge(in_low=1.5 in_high=3.5)`, atrasos de 1 ns, `SW(Ron=50 Roff=1e12 ...)`, faixa de 4,5–5,5 V do 74HC00, além do limiar visual de 1 µA na UI. Nada disso pode ser ajustado sem recompilar. Para um simulador de experimentação, esses valores devem ser **parâmetros nomeados**, com padrão documentado, editáveis no inspetor ou em modo avançado e salvos no projeto.

### P5. API de análise cresce por combinação
Para DC, transitório de tensão, de corrente, de fio e instrumentado existem funções paralelas em três camadas: `netlist_write_*`, `spice_*` e `spice_job_start_*`. Por baixo, `analyze()` recebe 13 parâmetros, e há inteiros mágicos (`mode`, `trace_current` = 0/1/2). Cada nova medição multiplica funções.

### P6. Dois simuladores e o estado "cálculo indisponível"
O simulador booleano só cobre fonte, chave, LED e fio, e virou um beco sem saída de UX. Decisão necessária (ver §6): SPICE como caminho padrão, com análise automática (debounce) ao editar, e o modelo booleano rebaixado a "modo conectividade" explícito ou removido.

### P7. Transitório só em lote
O processo externo calcula a curva inteira e a UI "reproduz" depois. O `META_1_0.md` pede tempo simulado com passo, pausa e clock. O experimento com a DLL compartilhada (`stop`/`resume`, registrado em DECISOES) é o caminho. Para isso a análise precisa estar atrás de uma interface de backend (§4.4).

### P8. Estilo que dificulta revisão
Várias instruções por linha, ternários aninhados em 3–4 níveis (ex.: `workbench_window.c:383`) e linhas de centenas de caracteres. Diffs ficam ilegíveis para humanos, e revisão humana é o que o responsável quer fazer.

### P9. Menores
- Caminho do ngspice: resolver relativo ao executável (`SDL_GetBasePath`), com variável de ambiente ou configuração para sobrescrever.
- `memcmp` do `Workbench` inteiro a cada frame para detectar mudança: trocar por um contador `revision` incrementado em toda mutação.
- `build.ps1` é uma sequência linear de chamadas gcc e só funciona em MSYS2. Separar testes unitários (sem ngspice) de testes de integração (com ngspice).
- Mover o legado para `legacy/` ou removê-lo, depois de registrar a decisão.

## 4. Arquitetura-alvo (incremental, não reescrita)

### 4.1 Registro de componentes (`component_registry.c/.h`)
Uma tabela de descritores, um por tipo:
```c
typedef struct {
    const char *id;            /* "resistor", "led", "74hc00": estável, usado no arquivo */
    const char *display_name;
    int pin_count;
    const PinInfo *pins;       /* nome, função, posição relativa */
    const ParamInfo *params;   /* nome, unidade, padrão, min, max, editável */
    int param_count;
    NetlistResult (*emit)(NetlistCtx *, const Part *);  /* linhas SPICE + modelos */
    void (*draw)(const Part *, const DrawState *);       /* fica na camada de UI */
    /* ficha didática (VISAO.md §6) pode viver aqui também */
} ComponentInfo;
```
- `Part` passa a ter `params[]` (ou um valor por `ParamInfo`) em vez de um único `double value`.
- O desenho fica num registro paralelo na UI, indexado pelo mesmo `id`, para o modelo continuar sem OpenGL.
- **Componente "SPICE livre":** pinos definidos pelo usuário mais texto de subcircuito/modelo. É o atalho mais barato para "experimentar com qualquer coisa que o ngspice suporta", sem esperar o catálogo. Precisa de aviso claro de que o modelo é do usuário.

### 4.2 Formato de projeto com identificadores estáveis
Novo formato (versão 16) com tipo por **string `id`** e parâmetros `chave=valor`. Componentes novos não exigem nova versão de formato; parâmetros desconhecidos geram aviso em vez de rejeição. As versões 1–15 continuam legíveis por migração, que já existe.

### 4.3 Separação da aplicação
```
app_state.h       struct AppState { Workbench, View, Selection, Settings, SimState, History, ... }
app_input.c       eventos SDL → comandos (testável sem janela)
app_commands.c    mutações do modelo + histórico + revision++
sim_state.c       fonte única da verdade: ociosa / calculando / válida / inválida(motivo)
render_*.c        desenho; só lê AppState
dev_capture.c     modos --capture-* e autoteste (fora de main)
main.c            init, loop: input → update → render
```
`SimState` responde a uma única pergunta, num único lugar: "o que mostrar e por quê". A UI exibe o motivo real (por exemplo: "ngspice não encontrado", "ciclo de fios ideais", "aviso do solver: …").

### 4.4 Análise unificada e backend substituível
```c
typedef enum { ANALYSIS_OP, ANALYSIS_TRAN } AnalysisKind;
typedef struct { ProbeKind kind; int target; int pin; } Probe;  /* tensão, corrente de peça, corrente de fio */
typedef struct {
    AnalysisKind kind;
    double step, stop;          /* configuráveis, sem 500 fixo */
    Probe *probes; int probe_count;
    int instrument_wires;
    SimLimits limits;           /* timeout, bytes, amostras: vindos das configurações */
} AnalysisRequest;
typedef struct SimBackend {     /* processo externo hoje; DLL compartilhada depois */
    int  (*start)(struct SimBackend *, const Workbench *, const AnalysisRequest *);
    int  (*poll)(struct SimBackend *, AnalysisResult *);
    int  (*step)(struct SimBackend *, double until);   /* NULL no backend em lote */
    void (*cancel)(struct SimBackend *);
} SimBackend;
```
`AnalysisResult` usa arrays dinâmicos (amostras × sondas), não bitmasks de 64/128 bits, e guarda `netlist_text` e `solver_log` para inspeção.

### 4.5 Modo avançado / desenvolvimento (já previsto no META 1.0)
- Ver e copiar o netlist gerado; ver o log bruto do ngspice; exportar `.cir` para rodar fora do app.
- Editar parâmetros de modelo e limites (§0.1b) com indicação de "valor fora do padrão".
- Ver todas as tensões de nó e correntes da última análise, não só a sonda.

## 5. Plano em incrementos (cada um é uma branch, com comportamento preservado salvo indicação)

Regra geral: **refatoração e feature nunca no mesmo commit.** Em refatorações, os testes existentes e as capturas `--capture-*` (comparadas antes e depois) servem de rede de proteção.

1. **Extrair capturas e autoteste** para `dev_capture.c`; centralizar o caminho do ngspice (relativo ao executável, com override).
   *Aceite:* `main()` perde as linhas de captura; capturas idênticas antes e depois; testes verdes.
2. **`AppState` + divisão de `main()`** em input/update/render; criar `SimState` como fonte única do estado exibido; trocar o `memcmp` por `revision`.
   *Aceite:* nenhuma função acima de cerca de 100 linhas em `workbench_window.c`; autoteste e capturas iguais.
3. **Registro de componentes** (§4.1), migrando um tipo por commit, começando pelos simples (resistor, LED), com `params[]` no `Part`.
   *Aceite:* adicionar um componente fictício de teste exige tocar só o registro, o desenho e um teste.
4. **Formato 16 com `id` em string** (§4.2), com migração das versões 1–15 testada por ida e volta.
5. **`AnalysisRequest` / `SimBackend`** (§4.4) envolvendo o processo atual; várias sondas por análise; limites vindos de configuração; avisos do ngspice exibidos em vez de descartados.
6. **Capacidades dinâmicas:** remover os limites fixos da tabela P3, com teste de estresse (por exemplo, 1.000 componentes e 5.000 fios) e medição de tempo de frame registrada.
7. **Parâmetros de modelo editáveis e modo avançado** (§4.5); componente "SPICE livre".
8. **SPICE como padrão, com análise automática** ao editar (debounce e cancelamento), e decisão sobre o simulador booleano.
9. **Backend pela DLL compartilhada** para passo/pausa/clock interativos (P7), mantendo o backend por processo como alternativa.
10. **Legado** para `legacy/` ou remoção, conforme a decisão registrada.

Os passos 1 e 2 vêm antes de qualquer feature nova: sem eles, cada feature aumenta o custo de todo o resto.

## 6. Convenções propostas para código novo e tocado

- Uma instrução por linha; linhas de até cerca de 120 colunas; sem ternários aninhados (use `if` ou `switch`).
- Funções curtas, com um propósito cada (cerca de 80 linhas como alerta, não como regra cega).
- Sem caminhos, limites ou parâmetros de modelo literais fora de um único lugar nomeado.
- Toda mensagem de falha ao usuário carrega a **causa real** disponível; nenhuma falha silenciosa.
- Módulos de modelo e simulação sem SDL/OpenGL, com teste unitário. Testes que precisam do ngspice ficam separados e são identificados como tais.
- Manter `-Wall -Wextra -Werror`. Avaliar adicionar `-fsanitize=address,undefined` num alvo de teste (verificar suporte no MSYS2 UCRT64 antes).
- Continuar registrando decisões em `DECISOES.md` e limites em `docs/GUIA.md`, como já é feito.

## 7. Decisões pendentes do responsável

1. O simulador booleano: remover, ou manter como "modo conectividade" explícito?
2. Análise automática ao editar: sim (recomendado) ou manter `P` manual?
3. Componente "SPICE livre" para experimentação: incluir já no incremento 7?
4. Código legado: mover para `legacy/` ou apagar (o histórico do git preserva)?
5. Ordem: confirmar que os incrementos 1 e 2 (sem features novas) vêm antes da próxima funcionalidade.
