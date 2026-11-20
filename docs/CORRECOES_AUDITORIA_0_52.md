# Auditoria Ponytail da 0.52.0 — correções e retomada

Data: 20/11/2026. Base: `fac78e2`, main da 0.52.0. Branch: `fix/ponytail-audit-0.52`.
Pedido: corrigir os onze pontos, sem integrar/publicar por enquanto. Não alterar saves pessoais.

## Estado de execução

| Item | Estado | Evidência / próximo passo |
|---|---|---|
| 1. Corrente de alimentação do 555/LM358 | Pendente | Regressão com carga e fonte CV/CC limitada |
| 2. Identidade do runtime ao excluir placa | Pendente | Regressão: remover Uno e substituir por código igual |
| 3. Gráfico após aplicar chip | Pendente | Regressão: fonte muda de índice sem mudar contagens |
| 4. ATtiny sem alimentação | Pendente | Medir VCC, interromper e reiniciar execução |
| 5. Histórico sob falha de memória | Pendente | Injetar falha, garantir transação |
| 6. Erro de envio ocultado | Pendente | Preservar causa no botão e Ctrl+Enter |
| 7. Relógio das placas | Pendente | Usar tempo aceito do solver, limitar latência de saída |
| 8. Conectividade do LCD | Pendente | Cache por revisão; regressão de desconectar/reconectar |
| 9. Bateria CTest | Pendente | Registrar lógica, lineares e proteção |
| 10. Arquivos temporários concorrentes | Pendente | Pasta por processo; testar execuções simultâneas |
| 11. Roadmap | Pendente | Separar implementado de validação restante |

Atualizar esta tabela em cada commit. Registrar comandos reais, resultados, limites e falhas abaixo.
Não marcar item concluído apenas por compilar. Refatoração e mudança funcional em commits separados.

## Comparação técnica antes da implementação

Fontes oficiais consultadas em 20/11/2026:

- [Manual ngspice](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf), fontes comportamentais e fontes controladas por corrente. O manual corrente é 47+, mas o runtime fixado do projeto continua 45.2; confirmar as expressões no runtime instalado.
- [CMake add_test](https://cmake.org/cmake/help/latest/command/add_test.html): compilar um executável não o registra no CTest; `WORKING_DIRECTORY` controla a pasta de execução.

Fatos: o 555 usa uma fonte comportamental referida ao GND; sua corrente não atravessa VCC. O runtime é indexado pela posição das peças. O solver e firmware avançam por relógios diferentes. Os testes usam pastas compartilhadas.

Alternativas: reescrever todos os modelos por transistores aumentaria custo e escopo; manter modelo funcional e contabilizar a corrente de carga conserva o comportamento e corrige a alimentação. Identidades persistentes globais exigiriam migração de formato; um marcador de identidade apenas em runtime pode resolver instâncias sem mudar arquivos. Cache de conectividade por revisão reutiliza o mecanismo de invalidação existente. Diretórios por processo isolam testes sem serializar todo o desenvolvimento.

Decisão inicial: incrementos mínimos com regressões antes/depois, preservar formato 38 e modelo sem SDL/OpenGL. Hipóteses a verificar: convergência da realimentação de corrente e latência dos passos de firmware. Limite relevante: avançar firmware apenas por lotes aceitos alinha os relógios, mas pulsos menores que um lote exigem subdivisão ou agendamento; não declarar fidelidade arbitrária de pulsos sem teste.

## Relatório da auditoria entregue ao responsável

O aplicativo é um simulador desktop em C, com edição de circuitos, chips personalizados e cálculo pelo ngspice. Auditada a main da 0.52.0, considerando uma pessoa montando circuitos, programando placas e usando instrumentos simultaneamente. Onze pontos relevantes: quatro tiveram reprodução executável; os demais são falhas ou dívidas identificadas nos caminhos de código. Nenhum arquivo versionado foi alterado durante a auditoria.

### Precisa corrigir

#### 1. O 555 entrega corrente sem cobrá-la da alimentação (`src/netlist_linear.c:81`)

- **O que faz:** calcula a saída usando uma fonte de tensão controlada pela alimentação.
- **Problema:** reproduzidos 31,43 mA na carga, com fonte limitada a 10 mA. A fonte entregava apenas 0,333 mA, referentes ao divisor interno. A saída cria energia sem carregar VCC.
- **Correção mínima:** fazer a corrente entregue à carga passar pela alimentação. Revisar também o LM358, cuja saída usa estrutura semelhante.
- **Se ficar:** estudos de consumo, fontes limitadas e queda de alimentação podem ensinar comportamento errado.

#### 2. Excluir uma placa pode transferir seu programa em execução para outra (`src/mcu_runtime.c:181`)

- **O que faz:** associa cada programa à posição da peça na lista.
- **Problema:** removida uma Uno, outra com código igual ocupou sua posição. O runtime reaproveitou a execução anterior; setup não rodou e o pino 13 permaneceu como entrada.
- **Correção mínima:** invalidar instâncias afetadas por exclusões/substituições; depois introduzir identidade estável por placa.
- **Se ficar:** placas aparentemente iguais funcionam diferentemente conforme a ordem das edições.

#### 3. Editar um chip pode fazer o gráfico medir outra peça (`src/sim_state.c:235`)

- **O que faz:** fecha o gráfico quando muda a quantidade de peças ou fios.
- **Problema:** aplicar edição interna reorganiza índices mantendo quantidades. Reproduzido gráfico de fonte apontando para resistor sem ser invalidado.
- **Correção mínima:** fechar o gráfico quando a edição aplicada reorganizar a bancada.
- **Se ficar:** leitura aparentemente válida representa outro componente.

#### 4. O ATtiny executa código sem alimentação (`src/mcu_runtime.c:215`)

- **O que faz:** executa programas enquanto a simulação roda.
- **Problema:** ATtiny sem fonte executou setup e escreveu no Serial; não verifica alimentação.
- **Correção mínima:** condicionar execução à tensão medida e definir reinício após queda/retorno.
- **Se ficar:** estados internos e temporizações avançam com o chip desligado.

#### 5. Falta de memória pode eliminar desfazer sem aviso (`src/app_editor.c:256`, `src/app_editing.c:372`)

- **O que faz:** registra cópias anteriores para desfazer.
- **Problema:** caminhos alteram a bancada e ignoram falha de bench_history_record. Operação anuncia sucesso sem recuperação.
- **Correção mínima:** garantir histórico antes de publicar ou restaurar estado anterior ao falhar.
- **Se ficar:** sob pressão de memória perde-se recuperação de edições. Conferido por leitura; auditoria não injetou falta de memória.

#### 6. Falha ao enviar código aparece como sem alterações (`src/app_code_editor.c:226`)

- **O que faz:** Executar e Ctrl+Enter aplicam o rascunho.
- **Problema:** app_code_apply retorna zero para nada mudou e para falha; chamadores apagam a causa.
- **Correção mínima:** preservar mensagem de falha e distinguir de rascunho inalterado.
- **Se ficar:** usuário acredita ter enviado programa enquanto placa executa o anterior.

### Deveria corrigir

#### 7. Placas e circuito elétrico usam relógios diferentes (`src/app_simulation.c:427`)

- **O que faz:** delay e execução seguem tempo real, circuito segue tempo simulado.
- **Problema:** a 0,2x um segundo do programa corresponde aproximadamente a 0,2 segundo do circuito; velocidade altera relação firmware/componentes.
- **Correção mínima:** avançar placas pelo tempo simulado e coordenar saídas com passos elétricos.
- **Se ficar:** pulsos, contadores e capacitores dependem do desempenho do computador. Dívida de modelo já documentada.

#### 8. Cada operação do LCD pode reconstruir toda a conectividade (`src/mcu_runtime.c:135`, `src/netlist.c:930`)

- **O que faz:** confere SDA e SCL entre placa e LCD.
- **Problema:** cada consulta aloca aproximadamente 3,4 MB e reconstrói conexões; uma operação pode fazer duas consultas repetidas sem alteração na montagem.
- **Correção mínima:** reutilizar conectividade enquanto revisão não mudar.
- **Se ficar:** atualizar LCD gasta processamento fora do solver. Auditoria não mediu milissegundos.

#### 9. CTest não executa toda a bateria dos CIs (`CMakeLists.txt:158`)

- **O que faz:** valida pelo fluxo CMake.
- **Problema:** test_logic_chips é compilado mas não registrado; testes de lineares e proteção dependem de build.ps1.
- **Correção mínima:** registrar executáveis e testes reutilizando listas existentes.
- **Se ficar:** CTest verde pode não detectar regressão no 555/proteções.

#### 10. Suítes concorrentes disputam arquivos temporários (`src/app_packing.c:18`)

- **O que faz:** testes usam build/test-blocks.
- **Problema:** suíte completa e pacote simultâneos podem remover arquivos do outro; ocorreu na release.
- **Correção mínima:** pasta exclusiva por execução passada aos testes.
- **Se ficar:** falhas intermitentes confundem investigação.

### Bom ajustar

#### 11. Roadmap lista entregas existentes como pendências (`docs/ROADMAP_1_0.md:93`)

- **O que faz:** orienta o restante da 1.0.
- **Problema:** proteção RGB/sete segmentos e 555/4511/L293D aparecem entre pendências sem separar implementação de validação restante.
- **Correção mínima:** marcar entregas e explicitar verificações/comportamentos faltantes.
- **Se ficar:** planejamento perde precisão e favorece trabalho duplicado.

### Verificação original e limites

Passaram test_eeprom, test_mcu_lang, test_workbench_group_edit e test_sim_state. Reproduções adicionais em build/audit-0.52. Não houve comparação completa de todos os CIs com datasheets, auditoria de vulnerabilidades de dependências, VM limpa ou leitura integral de cada função. A suíte completa não foi repetida na auditoria. Prioridade recomendada: 1–4, transações e relógio, antes de migração para C++.

## Diário e retomada

- Branch criada limpa a partir de main. Este documento registra relatório e plano antes das mudanças.
- Próximo passo: regressões permanentes para alimentação, identidade/runtime e gráfico; depois aplicar correções em commits pequenos.

### Item 1

Fonte de corrente comportamental debita de VCC a corrente positiva medida na sa?da; corrente absorvida retorna ao GND. Teste novo falhou antes e passou ap?s a corre??o. Fonte CV/CC existente tem ganho finito: teste admite 0,3 mA de excesso no limite de 10 mA (medido no LM358: 10,20 mA). Comandos: linha gcc de test_linear_chips em build.ps1; ./build/test_linear_chips.exe build/spice-local/ucrt64/bin/ngspice_con.exe. Modelos continuam funcionais, sem corrente de repouso completa ou t?rmica.
