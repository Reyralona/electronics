# Continuidade

> **Diário de trabalho interno**, do mais antigo para o mais recente: ponto de retomada entre sessões de desenvolvimento (pessoas e agentes de IA). Entradas antigas descrevem o estado da época. Para usar o programa, veja o [README](../README.md).

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
a referencia quality-nand.bmp (SHA256 3AC311CAD802DD3EE35FB5FE8298907906BDE2A2BEC184CEFBEE152D104B113C).
Teste de comandos cobre exemplos, valor, giro, exclusao de componente/fio, undo/redo e limites de reproducao.
Ainda ha funcoes grandes: input_click, draw_workspace e part_draw. SimState ainda precisa consolidar
transicoes; revisoes de montagem ainda usam memcmp. Esses itens antecedem ampliacao do catalogo.
A meta 1.0 permanece ativa e nao atendida; modelos eletricos e limites existentes foram preservados.

## Estado mais recente: 0.17.4 - funcoes focadas e transicoes sem janela
15/11/2026: app_input separa controles, selecao, busca de terminais, joelhos, colocacao e conexao.
app_render separa aparencias por tipo e passes de grade, placas, fios, contatos, rotulos e sondas.
Todas as funcoes de entrada/desenho ficaram abaixo de 100 linhas; a maioria abaixo de 80.
SimState agora executa invalidacao, reproducao e apresentacao de LEDs sem SDL/OpenGL.
O chamador fornece tempo monotono; test_sim_state liga apenas sim_state, spice_trace e workbench.
Ainda usa flags de validade e snapshots: isso nao encerra a consolidacao da maquina de estados.

Verificacao: ./build.ps1 -GuiTests passou. Comparacao com executavel compilado dos fontes da
v0.17.3: 16 capturas de montagem/configuracao/zoom/aparencias identicas. Mais seis modos de
transiente/correntes/LED/DFF e o exemplo NAND identicos apos mover as transicoes.
Teste de sonda negativa falhou antes da correcao e passou apos exigir probe_pin >= 0.
Correcao em commit separado das refatoracoes. Nenhum modelo eletrico foi alterado.

Proximo: representar validade por um estado unico, centralizar aceitacao/inicio/invalidation de
resultados e substituir comparacoes por revisoes de montagem. Depois, registro de componentes
com IDs persistentes e migracao de formato, conforme QUALIDADE_CODIGO.md.
Nao confundir reproducao de curva com backend de passo real. Meta 1.0 continua nao atendida.

## Estado mais recente: 0.17.5 - revisoes e validade unica
15/11/2026: Workbench tem revisao monotona de runtime. Edicoes da UI usam APIs do modelo;
bench_replace preserva a linha de revisoes ao carregar/substituir/exemplificar/undo/redo.
Nenhum memcmp da montagem permanece no ciclo app_simulation/sim_state; os cinco snapshots da
apresentacao deram lugar a revisoes. Worker e historico conservam snapshots onde necessarios.
SimResultKind substitui dc_valid/trace_valid; aceitar, descartar e limpar passam por SimState.
O ciclo de vida completo da tarefa ainda usa campos de controle separados.

Testes: ./build.ps1 -GuiTests passou; test_workbench_revision cobre alteracoes, no-op, entradas
invalidas, encaixe e substituicoes. Teste com worker concluido simulado confirma descarte depois
de editar/desfazer, mesmo com conteudo restaurado. Testes sem SDL confirmam exclusividade DC/curva.
Comparadas dez capturas finais com a v0.17.4: exemplo NAND, configuracoes, zoom, analogico,
DC, curva, LED ligado/desligado, DFF e calculo automatico; identicas byte a byte.
sizeof medido: Workbench 21.512 -> 21.520 bytes; SimState 290.576 -> 183.040 bytes.
Nao foi medido ganho de FPS. Formato de projeto continua 15; revisao nao e persistida.

Proxima prioridade: registro de componentes e IDs estaveis, seguindo a arquitetura documentada,
sem ampliar catalogo com switches espalhados. Ao tocar o ciclo de tarefas, consolidar causas de
falha/estado ocupado e cancelamento. Capacidades dinamicas e backend de passo real permanecem
obrigatorios para a 1.0. A meta continua ativa e nao atendida.

## Pedido prioritario do usuario e 0.18.0
Usuario pediu slots, biblioteca organizada com thumbnails, selecao por area/copiar/colar/excluir
e packing de selecoes em componentes pequenos reutilizaveis com entradas/saidas.
Confirmacao explicita: save states = slots de montagem/configuracoes, sem instante eletrico.
Slots F3 (9), Ctrl+S/L, miniaturas nativas e selecao por retangulo implementados.
workbench_selection.c e puro, copia partes e fios internos; colagem tem verificacao previa de
capacidade e numericos. app_editing.c organiza comandos e slots. Clipboard e apenas local ao app.
Testes cobrem conexoes/remapeamento, joelhos, mover, exclusao, falha atomica por capacidade,
slots validos/corrompidos e restauracao de configuracoes. ./build.ps1 -GuiTests passou com 88 fases,
incluindo retangulo por mouse, copiar/colar/excluir/undo e escolha de slot.
Capturas --capture-projects e --capture-selection revisadas; screenshots em build/*-preview.bmp.

PROXIMO OBRIGATORIO: packing com portas e instancias reutilizaveis, persistido no projeto/biblioteca.
Nao tratar clipboard como conclusao desse pedido. Prioridade do usuario precede a ordem anterior
que colocava registro de componentes antes de features. Preservar independencia do modelo/solver.
Pesquisa oficial iniciada: manual ngspice 46, secao 2.6, confirma expansao de subcircuitos.
Comparar encapsulamento no modelo (expandido para solver) com emissao .SUBCKT antes de implementar.

## Estado mais recente: 0.19.0 - pedido de packing entregue
15/11/2026: Ctrl+G encapsula selecao em corpo compacto com terminais P1..Pn; Ctrl+Shift+G abre.
Biblioteca Blocos (terceira aba/Tab), oito slots em blocks/block-N.elec, instancias independentes.
Slots F3 e clipboard da 0.18.0 preservam os grupos. Render/hit-test usam terminais expostos;
solver continua recebendo as pecas originais. Layout interno preservado; contatos de fronteira
com breadboards viram fios e grupos de instancias diferentes nao fazem contatos geometricos ocultos.

Formato 16 agora adiciona grupos/membros/portas ao esquema 15. Leitura 1-15 preservada.
A proposta antiga de formato 16 com IDs em string ainda NAO foi implementada: futura migracao
precisa usar outro numero e preservar os blocos existentes. Codec em workbench_pack_file.c.
Modelo em workbench_pack.c, comandos/biblioteca em app_packing.c. Construcao de Part usa campos
nomeados para acomodar metadados sem iniciais incompletas; modelo segue sem SDL/OpenGL.

Verificacao: ./build.ps1 -GuiTests passou (100 fases). test_workbench_pack com ngspice cobre
DC antes/depois, independencia de instancias e alteracao de valor, transiente RC, contatos de
fronteira e breadboards internas sobrepostas. Codec rejeita grupos vazios, portas duplicadas,
indices invalidos e fio externo em pino oculto; destino preservado. Capacidade insuficiente
para materializar contatos nao modifica a montagem. Movimentos invalidos nao corrompem grupos.
Captura --capture-packed revisada em build/packed-preview.bmp: terminais e dois fios externos legiveis.

Limites explicitos: sem blocos aninhados ou encaixe automatico de portas compactas na breadboard;
conexoes externas por fios. Biblioteca 8 slots; 64 pecas/128 fios incluem membros ocultos.
Pedido recente do usuario (slots, thumbnails, selecao/copia e packing basico) implementado.
Meta 1.0 permanece ativa: registro/IDs estaveis, capacidades dinamicas, parametros/logs inspecionaveis,
catalogo validado, passo real e distribuicao continuam pendentes. Proximo: retomar registro de
componentes e preparar persistencia extensivel sem perder grupos ou formatos anteriores.

## Novo pedido de UX do responsavel - 15/11/2026
Requisitos duraveis em QUALIDADE_DE_USO.md, incorporados a META_1_0.md: renomear chips; direcao/
funcao/polaridade legivel em todos os pinos; terminais circulares; multimetro com pontas fisicamente
conectadas e medidor de clock/pulso; interior de chip em editor isolado com salvar/renomear/voltar.
Consultar o interior NAO pode alterar a bancada. O desempacotamento Ctrl+Shift+G atual NAO satisfaz
esse pedido. Acoes principais precisam ser intuitivas e visiveis, alem dos atalhos.

Registro de componentes iniciado em refactor/component-registry: descritores de IDs, nomes, pinos,
valores primarios e capacidades. Ainda em implementacao/teste; nao declarar a migracao pronta.
Compilacao canonica recusou escrita por bancada aberta; usar ./build.ps1 -OutputName bancada-next -GuiTests
para preservar a sessao do usuario. Nao fechar o app do usuario para substituir o executavel.
O novo registro deve sustentar rotulos/funcoes de pinos e o editor de blocos, nao apenas reorganizar tabelas.

## Estado mais recente: 0.20.0 - registro inicial e clareza dos pinos
15/11/2026: component_registry concentra IDs, nomes, contagens de pinos, capacidades, parametros
primarios (chave/unidade/padrao/faixa) e nomes/funcoes dos terminais dos 15 tipos existentes.
Consultas por tipo sao indexadas; mapeamento historico de codigos de arquivo e separado do enum.
Workbench, validacao do netlist, persistencia antiga, palette e inspetor usam os descritores.
Geometria, emissores SPICE e arrays completos de parametros ainda nao migraram. Arquivo continua 16;
IDs/chaves registrados ainda NAO sao o novo formato de persistencia.

Feature separada da refatoracao: rotulos por funcao na selecao; tooltip de pinagem; circulos de
blocos com raio 8/5 igual aos terminais comuns; espacamento de portas 20 unidades. Funcoes herdadas
do componente interno, sem inventar direcao para passivos. Pinos TI SN74HC00 conferidos na tabela
5-1/SCLS181H em 15/11/2026; fonte em DECISOES.md. Hover consultado uma vez por quadro, mantendo
processamento de todos os eventos de arraste. Texto agora diz desempacotar, nao editor isolado.

Verificacao: ./build.ps1 -OutputName bancada-next -GuiTests passou (106 fases), alem do teste de
registro sem SDL. Antes da feature, sete capturas do registro refatorado foram identicas a 0.19.0.
Capturas finais capture-pins-preview.bmp, capture-pin-led-preview.bmp e capture-packed-preview.bmp
em build/ revisadas. Instancia bancada.exe do usuario preservada; nao fechar para substituir build.

PRIORIDADE do pedido recente: editor interno isolado e renomeacao de chips, seguido de instrumentos
com pontas reais (multimetro e clock/pulso). Requisitos/aceites em QUALIDADE_DE_USO.md; consultar
interior nao pode alterar bancada, historico ou estado externo. Desempacotar atual NAO satisfaz.
Persistencia futura precisa preservar nomes/portas/contextos e leitura 1-16. Registro completo,
capacidades dinamicas, passo real e catalogo continuam obrigatorios para a 1.0, ainda nao atendida.


## Editor isolado em validacao ? feature/isolated-chip-editor
Implementacao transacional: AppState filho com bancada/historico/worker independentes. Abrir nao
altera bancada externa; Aplicar publica apenas a instancia alvo com um undo. Salvar modelo grava
biblioteca separadamente. Voltar/fechar com alteracoes pede salvar modelo, descartar ou cancelar.
Nomes UTF-8 (127 bytes provisoriamente, rejeicao explicita), F4/botao Renomear e Enter/Ver interior.
Persistencia agora formato 17 para nomes com comprimento em bytes; leitura 1-16 preservada.
Tipos ainda sao codigos numericos; futuro formato com IDs deve ser 18 ou posterior.
Testes novos: test_workbench_group_edit (inclui ngspice) e test_app_editor (isolamento de jobs,
historico, cancelar, salvar e aplicar). Suite anterior de 106 fases passou com esta implementacao;
GUI ampliada para 121 fases passou apos corrigir drenagem da fila SDL na troca de contexto.
Capturas capture-editor-preview.bmp e capture-editor-name-preview.bmp revisadas: acentos e
contexto isolado legiveis.
Limites atuais: um nivel de editor, sem blocos aninhados, oito modelos, 64 pecas/128 fios.
Excluir ou substituir membro ligado externamente rejeita Aplicar, preservando bancada; copiar
remove proveniencia de portas para impedir reconexao silenciosa ao componente errado.
Multimetro e medidor de clock/pulso ainda pendentes. Meta 1.0 continua ativa.
C++ discutido com usuario: migracao gradual pode ajudar colecoes/ownership; nenhuma migracao
foi iniciada. Concluir incremento isolado antes de misturar mudanca de linguagem.


Novo requisito: escala ajustavel dos componentes, proporcao realista resistor/breadboard,
terminais dobraveis em modo distinto de ligar fios e grid/encaixe configuraveis. Aceites duraveis
em QUALIDADE_DE_USO.md. Geometria precisa persistir e funcionar em editor isolado, clipboard,
historico e solver sem alterar parametros eletricos. Ainda nao implementado.


## Grid: fundacao verificada na feature/configurable-grid
workbench_grid.c/h fornece origem/espacamento, quantizacao em coordenadas do mundo e densidade
visual separada do encaixe. Teste puro cobre negativos, empates, idempotencia, origem deslocada,
zoom/densidade e entradas nao finitas. Integrado ao desenho com valores anteriores: captura
--capture identica byte a byte a bancada.exe 0.21.0 (SHA256 27E67848...).
Compilacao estrita do app e test_workbench_grid passaram. Ainda NAO ha controles de grid/snap;
proximo: preferencias por contexto e persistencia (novo formato 18 se necessario), controles
visiveis, Alt suspende encaixe, prioridade da breadboard, uma translacao para grupos e testes GUI.
Nao publicar como funcionalidade concluida; manter exe canonico 0.21.0 ate verificar incremento.


## Grid configuravel verificado - 0.22.0
Controles visiveis no painel F1, espacamento 1-10000, snap de grade padrao desligado, breadboard
padrao ligado, Alt suspende ambos. Snap em coordenadas do mundo; corpo do chip serve de ancora,
grupos recebem uma translacao. Pecas individuais consultam breadboard antes da grade, sem mutar
modelo durante consulta (bench_breadboard_snap_position). Editor herda preferencias e as mantem
independentes da bancada. Formato 18 grava grid; nomes continuam formato 17, IDs ainda numericos.
Futuro formato textual de IDs deve ser 19 ou posterior. Suite completa e 132 fases GUI passaram;
preview capture-grid-settings.bmp revisado. ./build.ps1 -OutputName bancada-next -GuiTests.
Ainda falta ajustar fluxo de colagem/preview ao grid, alem de escala/terminais flexiveis. Novo
pedido: duplo clique em trecho adiciona joelho; em joelho remove. Priorizar implementacao comum
para fios/terminais, com ordem do caminho, curvas, undo e persistencia. Instrumentos seguem
pendentes. Meta 1.0 permanece ativa.


## Insercao de joelhos em fios - 0.23.0
wire_path_indexed compartilha desenho/proveniencia de trechos; wire_pick_segment projeta clique
no trecho visivel mais proximo, com empate pela ordem. bench_insert_knee insere sem trocar
terminais. input_click_knee tem prioridade sobre inserir, tolerancia acompanha zoom. Pontos
ficam visiveis apos insercao. Sem nova dependencia nem formato; continua 18.
Captura capture-knee-edit.bmp revisada (fio dobrado, controles e instrucao legiveis). Testes
puros cobrem projecao, ordem, curvas, capacidade/rejeicao. GUI ampliada de 132 para 138 fases,
incluindo inserir/remover/undo e salvar/reabrir. Suite completa passou com
./build.ps1 -OutputName bancada-next -GuiTests.
Proximo: geometria/escala e terminais flexiveis com modo explicito; depois instrumentos. Nao
confundir joelhos de fio entregues com dobra de terminal ainda inexistente. Meta 1.0 ativa.


## Refatoracao geometrica verificada - refactor/component-geometry
bench_part_to_world/bench_part_to_local centralizam quartos de volta e translacao. Pinos agora
sao definidos em coordenadas locais; selecao do corpo usa inversa da mesma transformacao em vez
de repetir retangulos orientados. Sem SDL/OpenGL no modelo. Breadboard conserva orientacao atual.
Suite completa e 138 fases GUI passaram; capturas --capture, --capture-rotation e --capture-ci
identicas byte a byte a 0.23.0. Testes verificam ida/volta e pinos em quatro rotacoes.
Ainda nao implementa escala nem dobra; preparar campo de escala, persistencia, desenho e contatos
como proximo incremento funcional separado. Caminhos flexiveis precisam de ownership explicito:
nao adicionar ponteiros a Workbench sem revisar snapshots/historico/clipboard/jobs (copias por valor).
Branch continua ativa; executavel canonico 0.23.0 preservado ate concluir incremento funcional.


## Escala de componentes - 0.24.0
Part.scale usa 0 como identidade legada; bench_part_scale normaliza para 1. bench_set_scale valida
PART_SCALE_MIN/MAX (.01-100), publica revisao e nao altera valor eletrico. Transformacoes locais
incluem escala; renderer e hit testing acompanham. Board e pinos mudam juntos; fios explicitos
continuam ligados por Pin, contatos implicitos sao recalculados. Inspetor Tamanho abre percentual;
APP_EDIT_SCALE separado de valor/tempo/nome/grid. Corpo compacto do grupo nao e redimensionado.
Formato 19 acrescenta float no fim de cada registro de peca; le 1-18 com escala 1. Futuro formato
com IDs textuais deve ser 20 ou posterior. Clipboard/historico/jobs copiam escala por valor.
Teste de resistor 1/7 entre furos adjacentes; inversa em quatro rotacoes, arquivo invalido, roundtrip,
e aplicar bloco reduzido com ngspice passaram. Suite completa e 146 fases GUI passaram.
Recompilacao estrita e 146 fases repetidas apos ajustes visuais/fit passaram; captura
capture-scale-preview.bmp revisada com rotulos sem sobreposicao.
Nao muda padrao de tamanho das pecas novas nem calibra unidades fisicas. Proximo: terminais
flexiveis, modo dedicado com duplo clique para joelhos, presets proporcionais e instrumentos.
Meta 1.0 permanece ativa; nao marcar concluida.


## Caminhos dinamicos - feature/flexible-terminals
bench_path.c/h implementa pontos com crescimento dinamico, clone independente, clear e falhas
com causa; overflow rejeitado sem mudar conteudo. Testes passam com 4096 pontos, clone/edicao
independente, clear repetido, indices invalidos e coordenadas nao finitas. Nao introduzir ponteiros
em Workbench/Part antes de migrar TODAS as copias por valor (historico, jobs, clipboard, packing,
editor e temporarios transacionais) para ownership explicito. Workbench atual continua sem ponteiros.
bench_polyline_pick aceita spans sem alocacao e e usado pelo wire_pick_segment; curva e desempate
mantidos. Testes de cruzamentos e misses preservando resultado. Limite atual de fios AINDA 16;
container dinamico nao significa que editor/solver ja foram migrados. Dobra de terminais ainda ausente.
./build.ps1 -OutputName bancada-next -GuiTests passou com 146 fases;
./tools/test-workbench-ngspice.ps1 passou. Capturas atuais --capture-knee-edit e
--capture-scale identicas byte a byte aos previews revisados das versoes anteriores.
Bancada do usuario aberta; NAO substituir/fechar. Capturas devem usar bancada-next conhecido:
um binario canonico antigo pode ignorar modo novo e abrir janela normal. Processo de teste
27452 foi identificado pela command line --capture-knee-edit e encerrado; sessao do usuario preservada.
Versao publica permanece 0.24.0. Branch continua ativa para migrar snapshots e integrar terminais.


## Fronteiras de snapshots - feature/flexible-terminals
bench_snapshot_clone/clear/move estabelecem ciclo de vida explicito, ainda com Workbench por
valor e SEM ponteiros. Historico usa clone/transferencia/clear, prepara undo/redo antes de publicar
revisao nova e limpa redo descartado/entradas consumidas. bench_history_record agora retorna int;
chamadores ainda precisam tratar falha antes de introduzir alocacao dinamica em Workbench.
SpiceJob clona snapshot ao iniciar; poll/join preservam dados para inspecao do resultado;
spice_job_clear espera thread antes de limpar. app_state_dispose centraliza limpeza do contexto,
historico, worker, bancada e clipboards; usado por janela/editor e testes de AppState.
Suite completa e 146 fases GUI passaram. Testes adicionais cobrem clone/move/self-clone,
limpeza repetida, revisao monotona e descarte de estados. Recompilados e executados depois:
test_app_commands, test_app_simulation, test_workbench_revision e test_spice_job com ngspice; passaram.
Ainda faltam snapshots locais de comandos/input, packing/clipboard, arquivos/exemplos e seu
tratamento transacional de falha. NAO anexar caminhos dinamicos antes de completar essa auditoria.
Dobra de terminais ainda indisponivel; versao publica continua 0.24.0. Branch mantida ativa.


## Packing/clipboard e publicacao transacional - feature/flexible-terminals
bench_replace agora retorna sucesso/falha; modelo preserva revisao quando publicacao falha.
Colagem prepara Workbench e selecao temporarios e publica ambos somente apos sucesso; clipboard
vazio com valid=1 e rejeitado. bench_selection_copy transfere snapshot ao substituir clipboard.
Compactacao, extracao e aplicar interior usam clone/clear de temporarios; erros limpam estados.
Parser de arquivos e construtor de exemplos limpam rascunhos em todos os caminhos e so publicam
settings/selecao quando bancada foi publicada. Formato continua 19, nenhuma geometria dinamica
anexada ao Workbench ainda. Suite completa incluindo parser/exemplos e 146 fases GUI passou:
./build.ps1 -OutputName bancada-next -GuiTests.
Restantes identificados: 14 copias locais em app_editor/app_editing/app_packing/app_input/
app_commands, mais temporarios de carga/modelos. Tambem preparar clone/clear/move de Part nos
subconjuntos (selection e group_apply) e remocao de pecas: Workbench clone sozinho NAO torna
segura uma atribuicao de Part quando terminais possuirem memoria. Copias de Part somente para
render/hit testing podem ser views emprestadas, nunca destruidas como proprietarias.
Ainda tratar falha dos chamadores de bench_history_record/bench_replace antes de adicionar
alocacao ao modelo. Dobra continua pendente; executavel canonico/sessao do usuario preservados.


## Cor dos fios corrigida - 0.24.1
Pedido do usuario: nao recolorir fios ao mostrar corrente. Removida sobreposicao de linha de
wire_current_style em DC/transiente; paleta original preservada. wire_direction_marker recebe
nivel medido, usa tamanho da seta e branco contornado; ponto DC tambem neutro. Legenda atualizada.
Capturas current-colors-off.bmp/current-colors-on.bmp/current-colors-trace.bmp revisadas; pixels
azul (500,301), vermelho (400,151), verde (650,220) identicos ligado/desligado. Suite completa e
146 fases GUI passaram: ./build.ps1 -OutputName bancada-next -GuiTests.
Incrementos preparatorios de snapshots/caminhos dinamicos integrados junto desta correcao;
NAO significam terminais flexiveis prontos. Faltam 14 copias locais UI, lifecycle de Part, falhas
de historico/publicacao e integracao final. Retomar dai em branch nova apos integrar esta.
Sessao do usuario deve ser preservada. Nao substituir bancada.exe se estiver aberto; binario
verificado em bancada-next.exe. Meta 1.0 permanece ativa com instrumentos/backend/catalogo etc.


## Polimento em feature/workbench-polish (ainda sem release)
Novo pedido usuario prioriza concluir terminais, ampliar catalogo validado e polir UI. Implementados
Ctrl+scroll para escala com undo, Alt+clique seleciona sem criar fio, minimo de selecao de 6 pixels,
Segoe UI suavizada via cache Unicode em todos os textos, nome Bancada e icone SDL nativo da janela.
Tema escuro em F1, salvo no formato 20 (le 1-19); paleta fisica dos fios/componentes preservada.
Suite completa e 155 fases GUI passaram. Capturas polish-dark.bmp/polish-light.bmp revisadas;
ultimo ajuste do titulo para BANCADA feito depois da captura. Falta revisao final do binario.
Publica permanece 0.24.1; terminais e catalogo novo AINDA pendentes. Retomar integracao de geometry
owned; usar cleanup GCC de snapshots locais conforme comparacao em DECISOES para encurtar handlers.


## Retomada 0.25.0 ? 15/11/2026
Suite ./build.ps1 -OutputName bancada-next -GuiTests passou, com 171 fases.
Catalogo22 tipos: NOR/XNOR, pot, botao, 74HC08/32/86 adicionados. test_catalog verifica todas
as entradas das quatro portas, ausencia de alimentacao, pot, botao, brilho por resistencia.
Fonte15px configuravel12..20, temaescuro, sem sombra de fio escuro. NAO e escala completa da GUI.
Blocos largura/altura independentes Ctrl+scroll/Ctrl+Shift+scroll, formato22 (le1..21), undo.
Refatoracao de ownership separada no commit bae84ef; Part ainda SEM caminhos dinamicos anexados.
Retencao visual em app_simulation compara snapshot concluido, parametros e endpoints, sem breadboard.
NAO resolve edicao durante worker, contatos breadboard ou alteracoes eletricas. Modelos batch sempre
reiniciam C/L com IC=0; sim_auto retorna sem fonte energizada. DFF tambem reinicia. Proxima prioridade
usuario: sessao eletrica persistente, capacitor descarregando e memoriaDFF. Ha libngspice-0.dll e
include/ngspice/sharedspice.h no runtime local. Comparar sessao persistente com migracao completa de
estado; isolamento do interior requer contexto separado (ngspice global nao pode ser compartilhado
silenciosamente entre bancada e editor). Fonte OFF atualmente e fonte0V, NAO desconexao fisica.
Usuario mencionou capacitor no slot2, mas saves/slot-2.elec tem0pecas/0fios; slot1tem6pecas incluindo
capacitor e LED. Nao alteramos saves. A pasta de execucao pode explicar a divergencia de slots.
Terminais dobraveis AINDA pendentes; nao anunciar como concluidos. Instrumentos e escalaGUI pendentes.
Capturas revisadas build/review-catalog.bmp, review-dark.bmp, review-packed.bmp; referencias oficiais
Tinkercad em build/visual-references, paginas6/7 do guia. Nova aparenciaLEDvertical,pot,botao revisada.
Meta geral havia sido pausada pelo usuario; nao alterar status automaticamente. Pedidos posteriores
sao tarefas autorizadas. Nao fechar processos do usuario para substituir executavel.


Correcao 0.25.1: corpoLED mantido vertical independentemente da rotacao dos contatos.
Captura build/review-upright-led.bmp revisada com LED0/90 graus. Compilacao estrita e171fasesGUI
passaram novamente; modelo eletrico nao alterado desde suite completa0.25.0.


## Perninhas e contato direto 0.26.0
Alt+arrastar terminal/joelho edita perna; Alt+duplo clique insere/remove joelhos.
Part agora TEM BenchTerminals owned, BenchPath por pino. Todas copias owning devem usar
bench_part_clone/snapshot_clone; views locais de renderer/snap sao emprestadas. Commit9fad6d7
separou preparacao de ownership. bench_lead_* usa pontos locais e retorna pontos de mundo.
Ponta arrastada encaixa em outro terminal (9px), contato exato usa tolerancia .01 unidades.
bench_find_contact_after enumera TODOS contatos, usado por netlist/booleano/packing; contatos
internos entre grupos distintos continuam isolados. Cachevisual compara todos contatos para
nao conservar resultado quando uma ponta se desconecta. Arquivo23 grava leads apos groups.
Test_leads verifica40joelhos, undo/redo profundo, arquivo, clipboard e corrente ngspice semfios.
Fixtures eletricas antigas empilhavam pecas em0,0: separadas geometricamente para evitar novos
curtos fisicos; sem mudar valores/ligacoes/asserts eletricos. Suite completa +180fasesGUI passaram.
Captura build/review-leads.bmp revisada. BotaoAlt+duploclique consta no inspector.
Experimento experiments/persistent_spice_probe.py rodou isolado na DLL local: RC2.499887V passou
para0.919650V ao abrir alimentacao, DFFreteveQ quandoD mudou comclockalto e soalterou na borda.
Esta prova NAO esta integrada aUI. Proxima tarefa: driver persistente nativo isolado por contexto,
netlist com contatos/fontes alteraveis e schedulerqueavanca tempo semreiniciar. FontesOFF no
backendbatchainda0V; naoanunciarcontinuidadeconcluida. Usuario pediu explicitamente continuar.


## Retomada 0.27.0 ? continuidade integrada

spice_session.c dirige spice-live-host.exe via pipes. Cada SpiceLive possui um processo ngspice
proprio, sem SDL no modelo; o host carrega a DLL e digital.cm do runtime local. spice_live.c
mantem snapshot anterior, chave normalizada do netlist e tempo solicitado/acumulado.
C/L/XSPICE conservam estado com alter/resume. Contatos Rpower/Rcontact usam 1 uOhm/1 TOhm;
LEDs possuem resistencia de guarda para abrir a peca quando a protecao didatica atua.
Somente topologia/frequencia nova reconstroi sessao. Valores, on/off, pot e movimentos sem
mudanca de conexao nao reiniciam. Circuitos com C/L/clock/DFF usam o backend continuo;
circuitos estaticos seguem batch. Nao afirmar continuidade irrestrita nem marcar 1.0.

SpiceJob continuo reutiliza live e recebe span. Correntes instantaneas usam o display DC
existente, sem reproduzir curvas antigas. Rodape mostra tempo. Resultado obsoleto e descartado
por revisao, mas isso nao reinicia o solver. Falha nao gera tentativas em cada frame: P tenta
recalcular e Ctrl+Espaco reinicia explicitamente. Erros preservam causa, log e exitcode.
Pending reset pertence a UI; reset_live e imutavel durante cada job. Slots, exemplos e load
pedem reset para o proximo job; salvar nao para nem zera. Editor possui outro host. Fonte OFF
abre alimentacao: capacitor pode descarregar. T produz curva separada desde IC=0; P volta ao
calculo ao vivo. Entradas digitais soltas precisam pull-down, validadas com botoes modelados.
Amostras normais atualizam rodape sem sobrescrever feedback de salvar/editar. Falha, burn e
reinicio podem emitir status. Passo solicitado entra no .tran inicial; mudar resolucao exige
Ctrl+Espaco. Proximos trabalhos: migrar estado ao mudar topologia/frequencia, limitar vetores
por streaming, instrumentos e catalogo. Vetores do solver crescem ate encerrar/reiniciar.

Perninhas curvas usam bench_lead_walk/pick e wire_path por joelho, sem limite de 16 pontos.
Renderer e hit testing compartilham amostragem. test_leads testa pontos desenhados e corrente
sem fios contra ngspice. Sondagem dos novos CIs quad chega ao VCC/pino 14.
Validacao: ./build.ps1 -OutputName bancada-next -GuiTests passou, com 183 fases. Testes nativos
cobrem duas sessoes isoladas, RC, fonte OFF, switch, DFF, botoes e protecao. test_app_live cobre
worker, feedback de salvar, pausa, movimento, bateria OFF e reset. PATH minimo tambem passou.
Capturas revisadas: build/review-live-rc.bmp (2.4999 mA), review-live-off.bmp (0.91965 mA com
fonte OFF), review-leads.bmp.

Chamadas pre-init ngSpice_nospinit/nospiceinit causaram 0xC0000005 neste binario e foram
removidas. O host conteve a falha. Nao reintroduzir API apenas pela declaracao do header;
validar comportamento real. Isolamento de configuracao spinit externa continua pendente.
Meta ampla permanece pausada conforme ultima ordem. As tarefas posteriores foram autorizadas.
Nao alterar saves nem encerrar janelas do usuario para atualizar executavel. Tag 0.27.0.


## Ajuste do inspector e terminologia (15/11/2026)
Usuario pediu dados tecnicos no inspector e dicas de controle somente no rodape.
Inspector preserva valores, tamanho, modelo e controles efetivos, sem atalhos entre colchetes.
Rodape contextual mostra tamanho, giro, valor, terminais, joelhos e ambiente conforme selecao;
o tempo ao vivo foi para a primeira linha, preservando a segunda para os controles.
Botoes mantem nome da acao; usar "terminais" em toda a interface. Capturas revisadas:
build/review-footer-pin-led.bmp e build/review-footer-environment.bmp.
Catalogo ampliado anterior ainda na branch feature/catalog-and-review; nao anunciar que
instrumentos, displays, motores, lista completa de CIs ou continuidade topologica acabaram.


## Brilho ao vivo e resultados intermediarios (15/11/2026)
app_simulation agora escolhe live tambem para fontes/baterias em circuitos estaticos.
compatible_live_sample verifica topologia, ignorando valores/on/controle; amostra antiga
aceita durante alteracoes continuas, preservando result_revision original. display_pending
mantem DC/brilho enquanto recalcula e mostra leitura "atualizando". Invalidar ANTES de
schedule_analysis substituir snapshot, para nao aplicar leitura antiga a outra topologia.
Pausa, reset pendente, erro e modo manual nao recebem esta retencao. Comparacao aceita
breadboard (contatos descobertos pelos terminais externos); verifica tipos/grupos/layout.
Regressao em test_app_live:80ajustes em sequencia, >=3amostras durante ajuste, sem frame
apagado; pot com/sem breadboard e resistor com breadboard, depois desconexao real apaga.
O teste novo falhou antes da correcao (pot ficava batch) e passou no backend real.


## Retomada por Claude - 0.28.0 (15/11/2026)
Codex deixou sem commit o catalogo analogico (14 tipos, 36 no total), revisao de arte por close-ups
e brilho ao vivo do potenciometro. Suite completa passou antes de qualquer alteracao; integrado
como commits nesta branch. Pedido seguinte do usuario: slot 1 com capacitor "calculando sem parar".
Causa e decisao em DECISOES.md (razao de contato 1e18 -> 1e13). Regressao check_led_hold.
Git: repositorio pertence ao usuario CodexSandboxOffline; usar `git -c safe.directory=*` sem
alterar configuracao global. Host travado pela Bancada aberta nao impede mais o build.

Pendente do pedido de catalogo (ver CATALOGO_COMPONENTES.md): IR, distancia, PIR, gas, DIP/DPST,
LED RGB, lampada, motores (vibracao, CC, CC com encoder, servo), piezo, sete segmentos e LCD 16x2
I2C aparecendo no exterior do chip, rele, instrumentos (osciloscopio, multimetro, gerador) e
inventario integral de CIs. Proximo incremento sugerido: LED RGB, lampada e piezo (saidas simples
sobre modelos ja validados), depois sete segmentos com exposicao externa em chips.


## Menus e preferencias - feature/ui-polish (15/11/2026)
app_layout.c e a fonte unica da geometria da interface; ao mover algo, mude so ali (testes seguem).
app_settings.c mapeia AppState<->WorkbenchSettings; app_preferences.c persiste saves/preferences.txt.
Widgets em app_render.c (ui_button/toggle/field/stepper/panel); app_render.c passou de 2000 linhas:
divida conhecida, extrair primitivas para modulo proprio quando tocar de novo.
Fila pedida pelo usuario apos este incremento: (1) resto do catalogo do primeiro pedido;
(2) exemplo com erro de calculo; (3) muitos exemplos com menu de titulo; (4) revisao geral do
codigo; (5) documento do que falta para 1.0.0 e historico desde 14/11.


## Estado 0.31.0 (15/11/2026, Claude)
Exemplos (15, dialogo), correcao do 74HC00, lampada, remocao do prototipo legado e ROADMAP_1_0.md.
Proximo obrigatorio antes do resto do catalogo: elementos internos por peca (ROADMAP_1_0.md 4.1).
Worktree ../electronics-claude (branch local claude/investigar-calculo-indisponivel) tem 9 arquivos
nao commitados ja integrados; remover so com confirmacao do usuario. Remoto da branch ja apagado.


## Estado 0.32.0 (15/11/2026, Claude)
Dispositivos multi-elemento entregues (component_devices + netlist_devices, formato 26).
Proximo: queima por elemento, divisao de app_render.c, CIs Tinkercad, decisao de microcontrolador.


## Estado 0.34.0 (16/11/2026, Claude)
Microcontroladores (Uno/ESP32/ATtiny85) + LCD I2C + editor de codigo lateral; transparencia (X);
troca de peca pelo catalogo; portas de blocos por no. Executavel unico: build/build.exe
(`./build.ps1 -GuiTests`). Modulos novos: mcu_lang, mcu_board, mcu_runtime, code_text,
app_code_editor. Proximo: CIs Tinkercad (74HC04/02, 555, 4511, L293D), exemplos com placas,
autoteste GUI do editor, lcd_connected recalcula nos a cada operacao (otimizar se pesar).


## Correcoes 0.34.1 - 16/11/2026 (Claude, sem commit, sobre main c6baa46)
4AND do slot 1 acendia com chaves abertas (divisor 10 GOhm/1 TOhm); entradas de portas/74HC
ganharam 100 MOhm. Chip renomeado/salvo no editor duplicava na biblioteca; agora BenchGroup.model
(formato 28). Ver DECISOES.md. Testes "humanos" (sequencia de acoes do usuario + caso oposto):
test_chip_library_flow.c e check_four_input_and. `./build.ps1 -GuiTests` passou.
Repositorio publico (16/11/2026): documentos de projeto movidos para docs/ (prompt.md -> docs/VISAO.md);
README e para quem chega de fora, docs/GUIA.md e o manual atual. Ao mudar comportamento visivel,
atualize docs/GUIA.md (nao acumule secoes por versao no README). Nao versionar dados pessoais.

## Tradução e revisão visual - 16/11/2026 (feature/i18n)
Revisados os merges de tradução até add2fac e a correção de símbolos e glossário e6f21fc.
Catálogo com 622 textos pt-BR/en-US, selecionados pelas bandeiras em Configurações.
O diff interrompido chamava palette_name sem definir a função; concluído com duas linhas
medidas pela fonte real, usando ui_wrapped existente. Nomes curtos continuam centralizados.
Preservada a revisão local de desmontar/unpack para desagrupar/ungroup.
README e GUIA agora descrevem os idiomas; decisões e limites seguem em TEXTOS e DECISOES.
Verificação: ./build.ps1 -GuiTests passou, incluindo test_ui_text nos dois idiomas e autoteste GUI.
Compilação estrita repetida após o ajuste final da declaração do helper e do modo de captura.
Novo modo de desenvolvimento: --capture-palette=N (0..5), com --language=pt-BR ou en-US
e --capture-output=build/arquivo.bmp. Geradas oito capturas de sensores, saídas, medição e
configurações; revisadas saídas nos dois idiomas, medição pt-BR e configurações en-US.
Artefatos locais em build/i18n-*.bmp. Capturas não são evidência de troca por clique/persistência;
persistência de preferências tem teste próprio. Números exibidos ainda usam ponto decimal.
Meta 1.0 continua não atendida. Após autorização do usuário, incremento preparado para integração
à main como 0.35.0, com VERSION, CHANGELOG e documentação pública atualizados. Sem pacote binário.

## Distribuição experimental 0.36.0 - 16/11/2026
Pesquisa e decisão em DISTRIBUICAO_WINDOWS.md: CMake/Ninja, CPack ZIP e NSIS por usuário,
sem migração ampla do build.ps1. Empacotar.cmd chama tools/package-windows.ps1.
Dependências e fontes MSYS2 fixadas com SHA256 em packaging/dependencies.lock.json; ngspice 45.2
preservado. Fontes completas e patches vão num ZIP separado que deve acompanhar os binários.
O host usa o caminho relativo canônico do codemodel: ngspice resolve contra o executável do
processo, não contra CWD; aspas no argumento não são removidas por esse comando. Não depender
de nomes curtos 8.3 no bundle. Caminhos alternativos mantêm o fallback anterior.
Release otimizado revelou buffer de expressão pequeno; correção em 8349692. Testes antigos com
solver real tinham sobreposição involuntária fonte/porta e expectativa anterior à fuga 100 MOhm;
fixtures e diagnóstico corrigidos em a6c06be, sem mudar os modelos elétricos.
Marcadores: portable.mode usa pasta do executável; installed.mode usa AppData/Bancada/Bancada;
sem marcador preserva CWD de desenvolvimento. Não migrar saves automaticamente nem apagar dados.
NSIS remove somente arquivos distribuídos, registra desinstalação HKCU e não altera PATH.
Verificações reais: ./build.ps1 -GuiTests passou; ./tools/package-windows.ps1 passou com CTest,
DC/transiente digital, RC/DFF/live e captura, PATH mínimo e diretório com espaços. Instalador
testado com chave/atalhos isolados, inclusive diretório de dados e preservação de arquivo extra.
NSIS trata avisos como erros; comando de desinstalação com caminho entre aspas verificado.
Artefatos em build/dist, com build-info.json identificando commit/ferramentas e SHA256SUMS externo.
Não assinados; sem publicação de release. Faltam VM limpa Windows 10/11, assinatura e teste de
atualização entre versões publicadas. MAX_PATH e caminhos fora da página de código permanecem
limites conhecidos. Meta 1.0 continua não atendida.

## Publicação por versão - 16/11/2026
Usuário autorizou GitHub Releases para cada versão. Publicada v0.36.0 como pré-release, com
instalador, portátil, fontes e SHA256SUMS; nomes, tamanhos e digests remotos conferidos.
tools/publish-release.ps1 valida hashes, versão, ancestralidade e código contra a tag antes do upload;
publica somente após conferir os quatro arquivos. -ResumeDraft recupera rascunho já enviado;
releases públicas não são sobrescritas. Script com BOM UTF-8 para Windows PowerShell 5.1.
Drafts precisam ser localizados pela CLI e consultados por ID: endpoint releases/tags não expõe
rascunhos. README aponta para releases; VERSIONAMENTO passa a incluir a publicação explícita.
Não há workflow automático por tag. Não fabricar binários históricos sem teste; próxima versão
segue build/testes, integração/tag e publish-release. Sem assinatura digital ou validação em VM.
