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
