# Caminho para a 1.0.0 - revisao em 20/11/2026

Base publicada: 0.51.1. Trabalho atual: 0.51.2, branch `fix/audit-and-slot2-performance`, criada da main.
A 1.0 ainda nao foi atingida. Este documento diferencia recursos entregues, erros reproduzidos e criterios de aceite pendentes; aumentar a versao nao substitui esses criterios.

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

| 20/11 | 0.48-0.51.1 | EEPROM gravavel, protoboard larga, selecao conjunta, notas e catalogo reorganizado. Pre-release 0.51.1 publicada. |
| 20/11, branch atual | 0.51.2 | Oito correcoes da auditoria, regressoes e otimizacao do circuito contador/adder/decoders. Ainda nao publicada. |

## 2. Estado atual

- Editor: fios e pernas ajustaveis, protoboards de 30/60 colunas, contato direto, rotacao, espelho, selecao por area ou Ctrl+clique, valores em conjunto e chips personalizados com interior isolado.
- Notas: texto e cores configuraveis, redimensionamento proporcional, copia, historico e persistencia. Nove categorias no catalogo, sem duplicar ou omitir os 66 tipos registrados (incluindo Nota).
- Simulacao: ngspice/XSPICE em host isolado, sessao continua, controles ao vivo, ritmo escolhido/observado e graficos. Movimento sem alterar conexoes preserva o estado; mudancas de topologia/frequencia ainda podem reiniciar a sessao.
- CIs: portas didaticas, 74HC00/08/32/86/161/173/245/283 e AT28C16. A EEPROM **le e grava pelo barramento**, com bytes persistidos no projeto; ja nao e somente leitura. Limites em [MODELO_EEPROM.md](MODELO_EEPROM.md).
- Placas: Uno, ESP32 e ATtiny85, com um subconjunto documentado da linguagem Arduino; LCD I2C e instrumentos ja existem. Isso nao equivale a executar firmware arbitrario ou a reproduzir todos os detalhes do hardware.
- Arquivos: formato 37, leitura de versoes antigas, gravacao por arquivo temporario e substituicao. Tipos ainda usam codigos numericos.
- Distribuicao: instalador e ZIP com runtime, fontes completas, licencas e hashes. [Pre-release 0.51.1](https://github.com/Reyralona/electronics/releases/tag/v0.51.1), sem assinatura digital.

## 3. Oito correcoes desta rodada

Os testes novos falharam antes das correcoes e passaram individualmente depois. A suite completa final ainda esta em verificacao nesta branch.

| Item da auditoria | Correcao | Evidencia permanente |
|---|---|---|
| 1 / R2-01 | Conversoes/aritmetica inteiras validam intervalo; divisao e resto extremos param com erro em vez de encerrar o processo. | `test_mcu_lang`: `integer_boundaries_stop_safely` |
| 2 / R2-02 | Carregar projeto valido resolve o rascunho na placa antiga e fecha sua associacao; arquivo invalido conserva o rascunho; undo recupera o programa editado. Exemplos e historico tambem fecham a associacao anterior. | `test_app_commands`: `check_project_replaces_editor_and_runtime` |
| 3 / novo R4-01 | Criar chip considera todos os contatos da protoboard larga. O circuito continua conectado depois de retirar a placa. | `test_workbench_pack`: `wide_board_connections_survive_packing`, duas metades da placa |
| 4 / R2-03 | Reabrir um projeto reinicia as placas a partir de setup; mover a placa preserva a execucao. | `test_app_commands`: fluxo de substituicao e runtime |
| 5 / R3-01 | Expoentes com sinal sao reconhecidos e tokens numericos malformados sao recusados. | `test_mcu_lang`: `numeric_and_type_regressions`, `signatures_and_tokens_rejected` |
| 6 / R3-02 | Inicializacao, atribuicao, vetores e casts aplicam o tipo numerico declarado. | `test_mcu_lang`: divisoes float e inteiras como casos opostos |
| 7 / R2-04 | Numero de argumentos e tipos de parametros/retornos sao aplicados, incluindo funcoes definidas depois da chamada; setup/loop exigem assinatura valida. | `test_mcu_lang`: chamadas validas/invalidas e conversoes |
| 8 / R3-05 | Validacao de configuracoes aceita o intervalo de terminais suportado; a escolha 14 do Uno salva e reabre. | `test_workbench_file`: `high_probe_terminal_roundtrip`; ciclo de selecao ja usa a contagem do componente |

A auditoria original continua em [AUDITORIA_REPOSITORIO.md](AUDITORIA_REPOSITORIO.md). Nao confundir estas oito correcoes com encerramento de todos os achados anteriores:

- **R3-04 continua pendente:** identidade do alvo de graficos apos reordenacao/aplicacao de chips; testar tambem carregar/undo/redo e correntes de fios.
- **R3-03 parcialmente tratado:** fechar o editor conserva rascunho se a aplicacao falhar. Faltam a regressao por falha de memoria injetada e revisar os botoes Executar/Ctrl+Enter que confundem erro com ausencia de alteracao.
- A instabilidade ocasional de salvar a biblioteca nao tem causa confirmada. Registrar o erro de sistema antes de atribuir uma causa.

## 4. Desempenho medido e limites

O circuito de referencia do slot 2 esta em `tests/fixtures/counter-adder-seven-segment.elec`: 235 pecas e 333 fios internos, clock de 50 Hz, contador, adder e decoders personalizados. Os blocos visiveis ocultam uma rede maior; ha tres grupos de decoder no arquivo, um fora da montagem principal.

- Conectividade para protecao contra curto e reutilizada enquanto a revisao nao muda. Editar fios/controles invalida o cache; um teste abre, fecha e reabre o curto.
- Conversao de entrada e compartilhada por no; a saida baixa usa `low=logic_ok-high`, eliminando fontes redundantes. Mantidos alimentacao, impedancia, limiares, passo e cadencia.
- Comparacao na mesma copia a 50 Hz, sem interface: **0,295x antes / 0,709x depois**, cerca de **2,4x**. Valores locais, nao promessa para qualquer maquina. Ainda nao e tempo real nesse caso.
- O teste conserva as 21 leituras anteriores dos dois displays, com e sem correntes nos fios. Isso prova equivalencia da otimizacao; nao certifica a tabela-verdade dos decoders montados pelo usuario.
- `test_counter_decoder` imprime a taxa, excluindo o primeiro lote. Nao ha assert de tempo dependente da maquina. Repetir com build de desenvolvimento e Release antes de comparar numeros.

Fontes, alternativas e comandos: [PLANO_CORRECOES_E_DESEMPENHO.md](PLANO_CORRECOES_E_DESEMPENHO.md). O proximo ganho deve partir de novo perfil, incluindo solver e interface; nao reduzir precisao nem pular ciclos para exibir 1x.

## 5. O que realmente falta para a 1.0, em ordem

### 5.1 Confiabilidade antes de ampliar o catalogo

- Encerrar R3-04 e a parte restante de R3-03 com regressoes que falhem antes.
- Testar salvar/reabrir sob falha de disco e memoria; verificar que a montagem e o arquivo anterior sobrevivem.
- Revisar identidade de placas, sondas e programas ao excluir/reordenar pecas. Reiniciar ao carregar resolve esse fluxo, nao prova todas as mutacoes.
- Conservar estado eletrico ao alterar topologia/frequencia, conforme a meta; explicitar os reinicios enquanto essa capacidade nao existir.

### 5.2 Capacidade e formato

- Substituir capacidades fixas por armazenamento dinamico: hoje 1024 pecas, 2048 fios e 16 joelhos por fio, alem dos limites do interpretador/editor e historico.
- Migrar tipos persistidos para IDs textuais estaveis, com versao nova e leitura dos arquivos antigos testada.
- Medir memoria, tempo de frame e taxa simulada em circuitos de referencia crescentes. Nao considerar ampliar constantes como conclusao dessa meta.

### 5.3 Modelos, catalogo e instrumentos

- Protecao por elemento para RGB/sete segmentos; registrar limites e diferencas do hardware real.
- Servo por largura de pulso, ultrassonico trigger/echo e DIP configuravel. O sensor IR de distancia atual nao substitui ultrassonico.
- Inventario oficial verificavel dos CIs restantes, pinagem e modelos: 555, familias 74HC restantes, 4511, L293D, LM741 etc. Lista anterior e planejamento, nao comprovacao de catalogo completo.
- Sondas posicionaveis independentes, medicao diferencial e de clock/pulso; netlist e diagnostico inspecionaveis. Usar circuitos de referencia para validar leituras.
- Avaliar modelos compactos dos CIs restantes e redes digitais densas com medidas antes/depois, mantendo comportamento e limites explicitos.

### 5.4 Uso, distribuicao e manutencao

- Validar leitor de tela, monitores/DPI reais e inspector sem cobrir a selecao. Foco de teclado sozinho nao encerra acessibilidade.
- VM limpa Windows 10/11, atualizacao entre versoes publicadas, assinatura e pipeline de release por tag. Instalacao/desinstalacao isoladas ja tem testes; nao substituem VM limpa.
- Dividir renderizacao/entrada/netlist somente em refatoracoes separadas de funcionalidades, protegidas por testes/capturas. Evitar nova camada sem necessidade demonstrada.
- Manter roteiro humano de montar, programar, criar chip, inspecionar, salvar, reabrir, excluir e desfazer, incluindo cancelamentos e falhas.

## 6. Verificacoes desta branch

- `./build.ps1 -GuiTests`: suite final em andamento; log `build/audit-fixes/full-tests.log`.
- `./build/test_counter_decoder.exe tests/fixtures/counter-adder-seven-segment.elec build/spice-local/ucrt64/bin/ngspice_con.exe`: equivalencia e taxa, sem alterar saves pessoais.
- Nao publicar 1.0 ate satisfazer [META_1_0.md](META_1_0.md), [QUALIDADE_DE_USO.md](QUALIDADE_DE_USO.md) e os aceites de distribuicao.
