# Continuidade - 14/11/2026

## Origem e integracao
Claude trabalhou no worktree `../electronics-claude`, branch `claude/investigar-calculo-indisponivel`.
A revisao de qualidade foi commitada em `ddff801`; a implementacao estava local, sem commit.
Foi copiada para `feature/quality-integration` sem modificar aquele worktree.
O exemplo NAND pendente foi preservado em `2a08b88`; a revisao foi incorporada em `4c1285a`.

## Direcao duravel
Seguir QUALIDADE_CODIGO.md: extrair desenvolvimento/capturas e estado da aplicacao antes de ampliar catalogo;
registro de componentes e identificadores persistentes; limites configuraveis/dinamicos;
logs e netlist inspecionaveis; backend de passo real. Refatoracoes separadas de funcionalidades.
Nao marcar 1.0 enquanto META_1_0.md tiver criterios nao atendidos.
Autorizacao do usuario: continuar autonomamente, branches e commits; perguntar apenas decisoes importantes.

## Integracao revisada
Calculo automatico com debounce e snapshots, play/pausa, sobrecarga didatica estatica,
grade cobrindo zoom/pan, giro por mouse e caminho do solver relativo ao executavel.
Corrigidos buffer de mensagem que impedia -Werror e uso de resultados de montagem obsoleta.
Transientes NAO usam a checagem de queima DC: essa aproximacao classificava indutores como curto
antes da analise e nao representava estados dos clocks. Ainda falta avaliacao termica/transitoria.
O modelo LED passou a incluir resistencia serie de 2 ohms e nos tem fuga numerica de 1 TOhm.
Tres exemplos F2, incluindo NAND DIP na breadboard, com 14 contatos e LED validado no transiente.

## Verificacao
`./build.ps1 -OutputName bancada-quality -GuiTests`: compilacao estrita e testes passaram.
`./build/test_workbench_examples.exe ./build/spice-local/ucrt64/bin/ngspice_con.exe`: passou.
Mesma chamada com test_spice_process.exe e test_spice_job.exe: passaram.
Captura --capture-auto revisada em build/quality-before.bmp.

## Pendencias conhecidas
workbench_window.c ainda concentra eventos, desenho e cenarios de desenvolvimento.
Calculo dinamico ainda em lote; repeticao da curva nao preserva estado continuo entre ciclos.
Queima estatica e heuristica reversivel, nao previsao de dano fisico; parametros ainda nao editaveis.
Fallback sem instrumentacao precisa comunicar indisponibilidade da corrente.
Limites fixos, runtime nao empacotado e catalogo parcial continuam impedindo a meta 1.0.

## Extracao inicial de desenvolvimento
`dev_capture.c/.h` agora possui reconhecimento dos modos, injecao de eventos SDL e gravacao BMP.
Cenarios e fases do autoteste ainda estao na janela; extracao completa permanece pendente.
Build estrito e autoteste passaram apos a extracao. Captura --capture-auto identica antes/depois
(SHA256 340A6047DFAF84EDB9DAE2B6F9CFC903621024FA2C002C36984E751E2E285C28).
Captura do terceiro exemplo revisada em build/quality-nand.bmp.

## Correcoes e validacao final deste incremento
Fallback transititorio sem sensores preserva a causa da indisponibilidade de correntes na mensagem da UI.
Regressoes: indutor diretamente na fonte gera transiente sem queima DC; caminhos ideais redundantes
permitem a curva sem inventar correntes e apresentam aviso. test_sim_auto passou com ngspice.
`tools/test-workbench-ngspice.ps1` e `tools/test-xspice-gates.ps1` passaram (RC, RL, fontes, LED e portas).

Ponto de retorno: v0.17.0. Proxima prioridade: concluir extracao de cenarios/autoteste e introduzir AppState/SimState, mantendo capturas e testes. Nao retomar ampliando o monolito ou presumindo que o plano de qualidade ja foi concluido.

## Estado mais recente: 0.17.1
Ao integrar main foram encontrados mais commits documentais do Claude (ate 052b4af).
Preservadas revisao P0 das DLLs e politica de branches curtas de VERSIONAMENTO.md.
A tag v0.17.0 foi corrigida apos uma tentativa de fast-forward recusada; aponta ao merge validado.
Branches de integracao concluidas foram apagadas; worktree electronics-claude segue intacto.
Executavel canonico: build/bancada.exe, gerado por ./build.ps1 -GuiTests (passou).
P0: setup-ngspice.ps1 baixa GCC runtime e winpthread explicitamente, verifica DLLs;
falhas do solver mostram codigo Windows/saida. Preparacao nova testada em build/spice-quality-runtime.
./tools/test-runtime-isolated.ps1 -Root build/spice-quality-runtime/ucrt64 passou com PATH minimo.
Ainda nao e pacote distribuivel: licencas e empacotamento final permanecem pendentes.
Proxima tarefa continua sendo separar estado/cenarios da janela, seguindo QUALIDADE_CODIGO.md.

## Estado mais recente: 0.17.2 - separacao da aplicacao
AppState possui montagem, selecao, configuracoes, trabalho assincrono e SimState.
SimState armazena resultados/reproducao sem SDL/OpenGL; ainda nao e uma maquina de estados consolidada.
app_simulation.c separa polling, aceitacao de resultados, debounce, invalidacao, reproducao e LEDs.
dev_scenarios.c contem cenarios de captura e todas as 79 fases do autoteste, fora de main.
spice_trace_nearest passou ao modulo de curvas; selecao automatica de sonda esta em sim_state.c.
.clang-format registra estilo de 120 colunas e instrucoes separadas; aplicado aos modulos extraidos e janela.
Formatter local descartavel: build/format-tool/clang_format/data/bin/clang-format.exe (23.1.5).

Verificacao: ./build.ps1 -GuiTests passou, incluindo test_app_simulation sem janela, DC/transiente
obsoleto e selecao de amostras. test-runtime-isolated.ps1 passou. Captura NAND permanece identica
(SHA256 3AC311CAD802DD3EE35FB5FE8298907906BDE2A2BEC184CEFBEE152D104B113C).
Durante extracao houve falha reproduzivel de zoom sintetico (direcao invertida).
Inicializacao explicita com SDL_zero e SDL_MOUSEWHEEL_NORMAL corrigiu; tres repeticoes passaram.
Nao atribuir causa interna ao SDL sem evidencia adicional.

Proximo: extrair entrada/desenho da janela e consolidar transicoes de SimState com testes sem janela.
Ainda faltam revisoes de montagem em vez de memcmp, registro de componentes, capacidades dinamicas,
parametros/logs inspecionaveis e backend de passo real. Meta 1.0 continua nao atendida.

## Estado mais recente: 0.17.3 - entrada, comandos e desenho
Em 15/11/2026, retomada a branch refactor/input-render com alteracoes locais preservadas.
workbench_window.c cuida do ciclo de vida; app_input despacha eventos para handlers de texto,
zoom, pan, giro e arraste; app_commands separa comandos de teclado com testes sem janela.
app_render recebe AppState const e divide chrome, bancada, inspetor, status, configuracoes e editor.
Comparacao local: manter eventos/desenho em main preservaria o acoplamento; extracao mecanica
preserva a ordem e permite validar comandos sem contexto OpenGL. Nenhuma dependencia nova.

Verificacao: ./build.ps1 -GuiTests; captura --capture-example2 revisada visualmente e identica
? referencia quality-nand.bmp (SHA256 3AC311CAD802DD3EE35FB5FE8298907906BDE2A2BEC184CEFBEE152D104B113C).
Teste de comandos cobre exemplos, valor, giro, exclusao de componente/fio, undo/redo e limites de reproducao.
Ainda ha funcoes grandes: input_click, draw_workspace e part_draw. SimState ainda precisa consolidar
transicoes; revisoes de montagem ainda usam memcmp. Esses itens antecedem ampliacao do catalogo.
A meta 1.0 permanece ativa e nao atendida; modelos eletricos e limites existentes foram preservados.
