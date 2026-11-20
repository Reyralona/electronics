# Auditorias do repositório e bateria de uso

Registro consolidado em 18/11/2026. Base: `main`, commit `02fe084`, versão 0.44.1.
Objetivo: reunir falhas verificadas e uma bateria de uso para corrigir e validar em conjunto.
Este documento não declara o repositório livre de erros nem a meta 1.0 concluída.

## Como usar

- IDs são permanentes; mudanças de prioridade não renumeram os achados.
- **Corrigido** exige código integrado e teste de regressão. **Confirmado** exige caso concreto.
- Hipóteses e limites documentados ficam separados dos erros confirmados.
- Antes de corrigir: transformar o caso em teste, observar a falha e verificar o caso oposto.
- A bateria combina testes de modelo, fluxo de comandos, solver e ações reais na interface.
  Um teste sem janela não comprova foco, desenho, legibilidade ou descoberta das ações.
- Carga considerada: uma pessoa local, até 256 peças e 512 fios. Duas instâncias e falhas de disco
  entram como testes de fronteira; não são presumidas como carga normal.

## Rodadas e evidências

| Rodada | Base | Resultado |
|---|---|---|
| R1 | 0.44.0, antes de `03a4bf1` | Cinco falhas confirmadas; corrigidas em `03a4bf1`, integradas em `02fe084`, tag `v0.44.1`. |
| R2 | `02fe084` | Quatro novos achados confirmados por programas temporários. Nenhuma correção aplicada. |
| R3 | `02fe084`, pasta de trabalho isolada | Continuação por áreas; achados adicionais e lacunas registrados abaixo. |

R2: `git pull --ff-only origin main` retornou `Already up to date`.
`./build.ps1 -GuiTests -OutputName audit-review` passou: suíte completa, ngspice,
18 exemplos contínuos e autoteste gráfico com PATH mínimo. Os testes existentes não cobrem
os quatro achados de R2. Programas temporários foram removidos após a verificação.

## R1 — corrigidos

| ID | Falha e reprodução | Correção e teste |
|---|---|---|
| R1-01 | Recursão com `int a[230]` alcançava uma reserva além dos 1.024 valores da pilha. O erro final ocultava uma escrita anterior. | Validar a reserva inteira antes da escrita; `tests/test_mcu_stack.c` verifica a fronteira e a mensagem de erro. |
| R1-02 | `random(0.5)` encerrava o processo com `0xC0000094`. | Truncar/validar limites antes do resto; `random_bounds` em `tests/test_mcu_lang.c`. |
| R1-03 | Fonte de 20.000 bytes virava rascunho de 16.379 bytes; Executar substituía o original cortado. | Recusar código que não cabe após normalização; `check_long_code_flow` em `tests/test_app_commands.c`, incluindo salvar/reabrir e limite aceito. |
| R1-04 | Com a simulação pausada, o programa seguia executando com relógio congelado. | Não executar o interpretador durante a pausa; `paused_program` em `tests/test_app_simulation.c`, com retomada. |
| R1-05 | Alterar DIP durante a pausa mantinha a leitura antiga marcada com a revisão nova. | Comparar `Part.states` ao preservar resultados; `paused_dip_change` em `tests/test_app_simulation.c`, com movimento puramente visual como caso oposto. |

As cinco regressões falharam antes da correção. Na correção de R1, o comando
`./build.ps1 -GuiTests -OutputName audit-fixes` passou. Nomes alternativos foram necessários
porque o executável de desenvolvimento estava aberto; as cópias foram removidas.

## R2 — confirmados, pendentes

### R2-01 — estouro de divisão inteira fecha o aplicativo — P0

- Local: `src/mcu_lang.c`, `arithmetic`, conversões para `long long`, `OP_DIV` e `OP_MOD`.
- Caso: `void setup(){long x=-9223372036854775808; Serial.println(x/-1);} void loop(){delay(1);}`.
- Observado: compila; o processo encerra com `0xC0000095`. Há risco de perder trabalho não salvo.
- Correção mínima: validar conversões inteiras e recusar divisão/resto cujo resultado não cabe,
  com erro visível no programa. Validar não finitos antes das conversões também.
- Aceite: o caso informa erro sem fechar Bancada; divisão inteira e decimal comuns continuam corretas.

### R2-02 — rascunho antigo sobrescreve programa de projeto carregado — P0

- Locais: `src/app_editing.c:app_projects_load`, `src/app_code_editor.c:app_code_validate/app_code_apply`.
- Fluxo: abrir código da placa A; pelo botão Projetos carregar outro circuito com placa B no
  mesmo índice; clicar Executar. A validação verifica apenas se o índice ainda é editável.
- Observado: o programa B foi carregado, o rascunho continuou sendo A e Executar substituiu B por A.
- Correção mínima: resolver o rascunho antes de substituir o projeto e invalidar sua associação
  com a placa antiga. Não aplicar o rascunho ao circuito já substituído.
- Aceite: carregar B com rascunho limpo e alterado; cancelar mantém A; confirmar não altera o código B.
  Cobrir também carregar um exemplo e undo/redo enquanto o editor está aberto.

### R2-03 — carregar projeto reutiliza execução antiga da placa — P1

- Locais: `src/app_editing.c:app_projects_load`, `src/mcu_runtime.c:instance_current/instance_prepare`.
- Fluxo: executar `setup(){pinMode(13,OUTPUT);}` com loop que escreve HIGH e espera; salvar e
  carregar a mesma montagem; continuar. O modelo dos pinos reinicia, mas o interpretador é reutilizado.
- Observado: `io_mode[13]` passou de 1 (OUTPUT) para 2 (INPUT_PULLUP); `setup()` não foi repetido.
- Correção mínima: reiniciar os programas ao carregar ou substituir toda a montagem. Não reiniciar
  placas por simples edição visual. Revisar também troca de índices após exclusão de peças.
- Aceite: reabrir executa setup uma vez, reinicia variáveis e configura os pinos corretamente;
  mover a peça preserva a execução e pausar/retomar continua sem reinício.

### R2-04 — chamadas ignoram assinatura da função — P1

- Locais: `src/mcu_lang.c:name_reference`, `function_definition`, `check_calls` e `OP_CALL`.
- Caso: `int echo(int x){return x;}`. `echo()` compila e imprime 0; `echo(2.5)` imprime 2.50.
- Problema: faltam validação da quantidade de argumentos e conversão ao tipo do parâmetro.
- Correção mínima: validar chamadas, inclusive funções definidas depois, e aplicar os tipos declarados.
- Aceite: argumentos ausentes/excedentes geram erro de compilação; `echo(2.5)` retorna inteiro 2;
  parâmetro float preserva a fração. Verificar também setup/loop com assinaturas inválidas.

## Bateria de uso planejada

| Fluxo | Sequência e resultado esperado | Cobertura e pendências |
|---|---|---|
| Programar placa | Criar, editar, executar, pausar, retomar, resetar; Serial e pinos seguem o estado indicado. | R1-01/02/04; acrescentar R2-01/04 e validação visual. |
| Trocar projeto com código aberto | Abrir A, alterar rascunho, carregar B, cancelar/confirmar, executar, salvar e reabrir. | R2-02/03; testar fonte e rascunho diferentes e iguais. |
| Montar e reorganizar | Peças, contatos diretos, breadboard, pernas, fios, dobras, rotação, espelho, cópia e exclusão; undo/redo conserva conectividade. | Testes existentes de seleção, terminais, pernas, espelho; ampliar fronteiras. |
| Criar chip | Agrupar, nomear pinos, entrar, consultar, editar, aplicar, salvar modelo, reabrir, aninhar, desagrupar e excluir da biblioteca. | `test_chip_library_flow`, `test_recursive_chips`, `test_manual_ports`; confirmar isolamento visível. |
| Instrumentar | Medir tensão/corrente/resistência, trocar sonda/modo, abrir/fechar gráfico, pausar e mudar circuito. | `test_devices`, `test_live_graph_flow`, `test_app_live`; R1-05 e invalidação. |
| Persistir | Salvar, fechar, reabrir; arquivo inválido e falha de gravação conservam montagem e arquivo anteriores. | Testes de arquivos/preferências; faltam falhas de disco controladas. |
| Usar pacote | ZIP em pasta com espaços, instalação, atualização, desinstalação; projetos sobrevivem. | Scripts existentes; não executados de novo nesta auditoria. |

## Mapa de revisão e limites

- Revisão direta mais profunda: interpretador, runtime das placas, editor de código, carregamento,
  histórico, aplicação/extração de chips, seleção/cópia, arquivos de projeto e sessão do solver.
- Revisão por amostragem: modelos elétricos, renderização, roteamento de entrada, textos,
  configuração CMake, preparação/empacotamento/publicação e testes associados.
- Ainda sem evidência para encerrar: desempenho perto do limite, falhas de disco/memória, todas
  as combinações de instrumentos, inspeção visual em diferentes escalas e Windows limpo.
- Limites conhecidos, sem novo achado: mudanças de topologia/frequência reiniciam a sessão;
  modelos são didáticos; o editor tem limite de 16 KiB. Ver `docs/CONTINUIDADE.md` e `docs/GUIA.md`.
- Instabilidade já registrada: `test_chip_library_flow` às vezes falha ao salvar. Não atribuir uma
  causa sem capturar o erro real; manter separada dos erros reproduzidos acima.

## R3 — continuação

### R3-01 — números científicos e tokens inválidos são interpretados incorretamente — P1

- Local: `src/mcu_lang.c:next/lex_number`. O token numérico não inclui o sinal do expoente e
  `lex_number` não exige que a conversão consuma o token inteiro.
- Casos confirmados: `float x=1e-3; Serial.println(x);` imprime `-2.00`, embora x devesse valer 0.001 (a impressão padrão seria `0.00`);
  `Serial.println(123abc);` compila e imprime `123`, em vez de informar número inválido.
- Correção mínima: reconhecer expoente com sinal e validar o fim da conversão. Recusar tokens
  numéricos inválidos sem transformar o restante em outro cálculo.
- Aceite: expoentes positivos/negativos, maiúsculos/minúsculos, hexadecimais, binários e números
  malformados; preservar a subtração comum `1-3` como -2.
- Reprodução: programa temporário compilado com `gcc -std=c11 -Wall -Wextra -Werror -Isrc`,
  ligando `src/mcu_lang.c` e `src/ui_text.c`; saída observada diretamente no callback de Serial.

### R3-02 — variável float inicializada com inteiro mantém operações inteiras — P1

- Local: `src/mcu_lang.c:store` e carregamento de variáveis; o armazenamento converte inteiros,
  mas não estabelece a representação float quando o valor de origem é inteiro.
- Caso confirmado: `void setup(){float x=1; Serial.println(x/2);} void loop(){delay(1);}` imprime
  `0`; a declaração de x deveria fazer a divisão produzir 0.5.
- Correção mínima: aplicar o tipo declarado nas atribuições, incluindo inicialização e vetores,
  sem inferi-lo apenas do valor que foi atribuído.
- Aceite: `float x=1` e `float x=1.0` fazem o mesmo cálculo; `int x=1` continua fazendo divisão inteira.
- Relacionado, sem duplicar ID: também foi confirmado que `int f(){return 2.5;}` retorna `2.50`.
  Incluir conversão de retorno no escopo de R2-04, junto das conversões de parâmetros.

### R3-03 — fechar editor após falha de memória descarta a edição e oculta a causa — P1

- Local: `src/app_code_editor.c:app_code_close/app_code_apply`.
- Fluxo: abrir programa, alterar rascunho, fechar editor; a cópia necessária ao histórico falha.
- Evidência: injeção controlada de falha apenas em `bench_snapshot_clone` chamado pelo editor.
  Resultado: `code_part=-1`, `code_dirty=0`, fonte antiga intacta e status “Editor de código fechado.”.
  Reabrir substitui o rascunho perdido pelo programa antigo. Não foi provocada falta real de memória.
- Correção mínima: distinguir “sem alterações” de “falhou ao aplicar”; em falha, manter o editor
  aberto, o rascunho alterado e a mensagem da causa. Revisar os chamadores que trocam erro por
  “nenhuma alteração”, inclusive o botão Executar e Ctrl+Enter.
- Aceite: falha controlada conserva texto e erro; nova tentativa bem-sucedida aplica o texto e
  fecha; fechar sem alterações continua funcionando.

### R3-04 — gráfico ao vivo muda de alvo após aplicar um chip — P1

- Locais: `src/sim_state.c:sim_live_graph_stale`, `src/workbench_group_edit.c:bench_group_apply`.
- Fluxo: resistor no índice 0 dentro de chip, fonte no índice 1 fora dele; abrir gráfico da fonte;
  entrar no chip, mudar o resistor e aplicar, mantendo a quantidade de peças e fios.
- Evidência: aplicação bem-sucedida reordenou a fonte para índice 0 e o resistor para índice 1.
  A sonda continuou no índice 1 e `sim_live_graph_stale` retornou 0. O gráfico passou a apontar
  para o resistor, sem aviso. A prova verificou o alvo do gráfico; não executou o solver.
- Correção mínima: fechar ou remapear o gráfico em operações que substituem/reordenam peças
  ou fios. Comparar apenas as quantidades não identifica a peça que estava sendo medida.
- Aceite: aplicar chip com gráfico de peça externa aberto mantém o alvo correto ou fecha o
  gráfico com aviso; repetir para corrente de fio, carregar projeto e undo/redo.

### R3-05 — seletor de sonda não alcança os terminais das placas — P1

- Locais: `src/app_input.c:cycle_probe_pin`, `src/workbench_file.c:workbench_settings_valid`.
- Fluxo: selecionar Uno, abrir Configurações, clicar repetidamente no terminal da sonda.
- Evidência: Uno tem 23 terminais; após 23 avanços o conjunto visitado foi apenas 0, 1 e 2
  (`visited_mask=7`). A função usa 14 apenas para CIs de quatro portas e 3 para todo o restante.
  O validador também rejeitou `probe_pin=14`, embora seja válido para a placa.
- Correção mínima: derivar o número de terminais do componente selecionado e alinhar a
  validação/persistência com essa capacidade. Definir o comportamento quando não há seleção.
- Aceite: alcançar e medir todos os terminais do Uno, ESP32 e dispositivos de mais de três pinos;
  salvar/reabrir a escolha sem impedir salvar o projeto; não oferecer terminal inexistente.

### Verificações complementares de R3

- Foram compilados e executados, nesta pasta isolada, os testes existentes `test_workbench_file`,
  `test_workbench_selection`, `test_workbench_group_edit`, `test_workbench_pack`,
  `test_workbench_history`, `test_app_preferences` e `test_spice_trace`: todos passaram.
  Foram usadas as linhas gcc correspondentes de `build.ps1`; nenhum teste existente foi alterado.
- As reproduções adicionais usaram arquivos temporários em `build/`; não são testes de regressão
  integrados. Devem virar testes permanentes quando as correções forem implementadas.
- Medição exploratória: geração de netlist ao vivo, 256 peças (uma fonte e 255 resistores),
  256 fios, oito repetições: média de 37,12 ms, nenhuma falha. Compilação sem otimização, como
  `build.ps1`. Isso mede somente a tradução do modelo; não mede solver, desenho nem tempo de frame.
  Não foi classificado como bug sem um orçamento de desempenho e um cenário completo comparável.
- Revisados nesta rodada também: transformações e pernas locais no copiar/colar; limites e
  publicação transacional de chips; validação de arquivos; grade/zoom; preferências; scripts de
  empacotamento e instalador. Não há achado novo confirmado nessas partes além dos listados.

## Ordem para a bateria de correção

1. R2-01 e R2-02: impedir encerramento e sobrescrita de programa.
2. R3-03: conservar edições quando a aplicação do rascunho falha.
3. R2-03 e R3-04: alinhar a identidade de placas, pinos e sondas após substituição de montagem.
4. R3-01, R3-02 e R2-04: validar léxico, tipos e chamadas do interpretador em conjunto.
5. R3-05: completar a seleção e a persistência de terminais dos instrumentos.
6. Executar a bateria de uso planejada e repetir as regressões de R1, sem assumir que uma
   correção em lote elimina a necessidade de provar cada falha antes da alteração.

Estado desta consolidação: **14 achados, cinco corrigidos e nove confirmados pendentes**.
O objetivo da próxima implementação é cobrir esses nove em uma bateria coerente; não há garantia
prévia de esgotar todos os erros do repositório. A branch de interface deve ser confrontada com
este registro antes de implementar problemas sobrepostos.


## Atualizacao em 20/11/2026 - branch 0.51.2

O texto das rodadas acima e historico. O estado consolidado atual esta em ROADMAP_1_0.md.

- R2-01/02/03/04, R3-01/02 e a parte restante de R3-05 receberam correcoes e testes permanentes nesta branch; testes especificos passaram, suite final ainda em andamento.
- Novo R4-01: criar chip perdia conexoes na metade extra da protoboard larga. Corrigido usando a contagem real de contatos; regressao exercita as duas metades e retira a placa.
- R3-03 e parcial: falha ao aplicar ja nao descarta o rascunho ao fechar, mas a injecao de falha e os chamadores que ocultam o erro continuam pendentes.
- R3-04 continua pendente. Esta rodada nao declara que todos os problemas possiveis foram encontrados ou corrigidos.
