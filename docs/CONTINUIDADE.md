# Continuidade

Estado para retomar o desenvolvimento em 19/11/2026. O histórico detalhado permanece no Git;
este arquivo registra apenas fatos e pendências que continuam relevantes.

## Produto e versão

- Versão integrada na `main`: 0.47.1, incluindo os recursos da 0.47.0, a validação do runtime ngspice
  no build e a auditoria consolidada. Pacotes da pré-release validados em 19/11/2026.
  Formato de projeto 35. Última pré-release publicada: 0.47.1, Windows x64.
  Tag `v0.47.1`, com instalador, ZIP portátil, fontes e hashes conferidos no GitHub.
- 0.47.0: limites de 1024 peças e 2048 fios; CIs 74HC161/173/245/283 e EEPROM AT28C16 (`netlist_logic.c`,
  `rom_image.c`). O exemplo do computador de 8 bits foi retirado (commit 3ca0d8e guarda o gerador).
  Pendente: editor do conteúdo da EEPROM na interface e conferência visual das pinagens além do 74HC161.
- O comportamento atual está em [GUIA.md](GUIA.md); visão e critérios da 1.0 estão em
  [VISAO.md](VISAO.md), [META_1_0.md](META_1_0.md) e [ROADMAP_1_0.md](ROADMAP_1_0.md).
- As cinco correções de `fix/audit-runtime` foram integradas à `main` em 18/11/2026 por pedido
  do responsável, com ponto de retorno `v0.44.1`. Nenhuma release foi publicada nesta integração.
- A branch de interface implementa o [plano de usabilidade](PLANO_INTERFACE.md), sem trocar toolkit ou modelo elétrico.
  `app_controls.c` lista os alvos de foco; geometria e transformação proporcional ficam em `app_layout.c`.
  `gui_font_size` preserva o campo de preferências existente e passa a representar escala integral (15 = 100%).
  Leitor de tela continua pendente: foco de teclado não equivale a um provedor de acessibilidade.

## Simulação

- A sessão contínua mantém o estado de capacitores e lógica entre avanços. Mudanças de
  topologia ou frequência reiniciam a sessão; esta limitação ainda precisa de solução.
- `.save none`, restaurado após `delete all`, limita o histórico do ngspice à amostra
  atual. Gráficos solicitados com `T` usam análise separada e histórico completo.
- O passo máximo do solver ao vivo é 1 ms (ou 1/32 do período de uma senoide), independente do
  passo das Configurações, que vale só para a curva completa (`Shift+T`). `T` abre o gráfico ao vivo,
  amostrado no processo auxiliar pelo callback de dados (comandos `P` e `S` do protocolo).
- Um lote que falha fecha o processo auxiliar; o seguinte recria a sessão em t = 0.
- O ritmo solicitado é uma preferência de 0,05× a 1×. O rodapé mostra também o ritmo
  observado; o solver pode ficar abaixo do solicitado em circuitos exigentes.
- O multímetro em modo resistência tem tensão de teste limitada e avisa sobre fonte
  ativa. O osciloscópio carrega o circuito com entrada de 1 MΩ; o multímetro, 10 MΩ.
- Medições e limites estão em [COMPARACAO_TEMPO_REAL.md](COMPARACAO_TEMPO_REAL.md)
  e [DECISOES.md](DECISOES.md).

## Repositório e distribuição

- Testes de circuito usam fixtures versionadas em `tests/`. Nenhum save pessoal é
  necessário. A auditoria de tags, releases e referências locais está em
  [AUDITORIA_PUBLICACAO.md](AUDITORIA_PUBLICACAO.md).
- `build.ps1` descobre ferramentas no `PATH`, em `MSYS2_ROOT` ou em `C:\msys64`, copia as DLLs do build de desenvolvimento
  e testa a interface com `PATH` mínimo. O pacote de distribuição inclui o runtime.
- O executável local de desenvolvimento é `build/build.exe`; arquivos em `build/` são
  descartáveis e não devem aparecer como dependência de fonte ou fixture.

## Projetos e miniaturas

- A galeria usa slots numerados dinâmicos em páginas de seis; projetos antigos `slot-N.elec` são preservados.
- Carregar projeto válido enquadra corpos, terminais, chips e fios. Miniaturas `slot-N.bmp` são criadas ao salvar ou quando um save antigo aparece na galeria.
- A comparação técnica está em `COMPARACAO_PROJETOS_2026_11_19.md`; `build.ps1` verifica crescimento além do slot 9 e carga enquadrada.

## Verificação e próximos passos

- 0.47.1: `./build.ps1 -GuiTests` passou; `./tools/package-windows.ps1` passou com CTest,
  simulação relocada e PATH mínimo, instalação/desinstalação e preservação de arquivos extras.
  Captura do pacote revisada em `build/cmake-release/package-test-*/package.bmp`.
  Artefatos e hashes em `build/dist/`; sem assinatura digital nem validação em VM limpa.
  A integração inclui todas as branches locais e remotas existentes em 19/11/2026.

- 0.45.1: bancada em largura total; seleção de peças próximas ajustada e validada no slot 4;
  terminais visuais das portas espelhadas corrigidos. Capturas individuais dos 59 tipos em
  1×, 3× e 7× estão em `build/component-review/full/`; revisão e comandos em
  [REVISAO_COMPONENTES.md](REVISAO_COMPONENTES.md). O contorno do LED RGB, do sensor de luz
  ambiente e do PIR foi refinado sem alterar o modelo elétrico.
  `./build.ps1 -GuiTests -OutputName final-review` passou após o polimento.

- 0.45.0: `./build.ps1 -GuiTests` passou; CMake/CTest verificaram novamente o fluxo de interface
  e os 235 passos do autoteste gráfico após o ajuste final de autorrepetição. Capturas e limites
  estão em [REVISAO_INTERFACE.md](REVISAO_INTERFACE.md). Branch separada, sem tag/release.
- `./build.ps1 -GuiTests` passou com os testes de simulação, instrumentos e interface.
- **Teste instável em aberto (17/11/2026):** `tests/test_chip_library_flow.c` falha de vez
  em quando em `assert(app_editor_save(inside))` (vistas as linhas 106 e 183): cerca de 2 falhas
  em 10 execuções completas de `build.ps1`, nenhuma em mais de 50 execuções do teste isolado.
  Causa não identificada. Já descartado: colisão de nome na lixeira (corrigida na 0.42.1,
  errno 17). Hipótese não confirmada: outro processo (antivírus/indexador) segurando
  `build/test-blocks/block-N.elec` durante o `MoveFileExA` de `workbench_save_project`.
  Próximo passo: registrar `GetLastError()` **antes** de qualquer outra chamada num log em
  arquivo (stderr derruba o `build.ps1` no PowerShell 5.1) e repetir o build completo.
  Se a falha se repetir, rodar o build de novo contorna.
- Próximos passos: preservar estado em mudanças de topologia e frequência, medir
  desempenho em circuitos maiores e avançar nas pendências do roadmap da 1.0.

## Bancada EEPROM em desenvolvimento — 20/11/2026

Branch `feature/eeprom-workbench-usability`, versão 0.48.0, formato 36. EEPROM aceita gravação pelo barramento; detalhes e limites em [MODELO_EEPROM.md](MODELO_EEPROM.md). Protoboard larga de 60 colunas, seleção com Ctrl+clique e edição conjunta, terminais com alvos de clique independentes da escala. Fixture pública: `tests/fixtures/eeprom-programmer.elec`. Testes humanos com mouse e teclado no `build/build.exe` e capturas em `build/human-review/captures`. A versão 0.48.0 ainda não foi publicada.

## Notas na bancada - 20/11/2026

Na mesma branch, versão 0.49.0 e formato 37: componente Nota em Básicos, edição por clique duplo, cores independentes de fundo e texto, histórico e persistência. Slot 1 pessoal anotado por etapa, com cópia anterior em saves/backup. ./build.ps1 -GuiTests passou; captura revisada em build/notes-circuit.bmp. Montagem física não validada: contatos e fios continuam idealizados.


### Correcao 0.49.1

Notas agora sao aceitas como elementos sem efeito eletrico no gerador de netlist. Regressao em test_netlist falhou antes da correcao e passou depois (DC e sessao continua, circuito identico com/sem nota). test_eeprom_programmer_flow passou com nota, incluindo gravacao e persistencia. Slot 1 pessoal, 76 pecas, simulado por 10 ms sem erro. build/build.exe recompilado; suite completa nao repetida nesta correcao localizada.


### Post-its e terminais - 0.50.0

Notas com fita, sombra e canto levantado; alca no canto inferior direito altera a escala proporcionalmente, ancora o canto oposto e registra uma unica acao para desfazer. Teste test_app_ui_flow falha com app_input anterior e passa com a implementacao, incluindo clique duplo na alca; autoteste --self-test-history passou. Capturas de zoom com roda real no build/build.exe em build/human-review/captures/notes-{normal,zoom-out,zoom-in}.png; revisao do papel em build/postits-paper.bmp. Terminais usam raio de 3 unidades da bancada, independente da escala do componente; picking mantido. Sem mudanca de formato (37). Suite completa nao repetida neste ajuste.


### Papel e catalogo - 0.51.0

Papel usa malha sombreada com borda inferior arqueada, fita fora da area de texto e a mesma representacao nas miniaturas. Nota e juncao nao usam mais fallback de LED. Catalogo tem 9 categorias; biblioteca de chips conserva indice 2 (Salvos), Fontes=6, Placas=7, Bancada=8. Cada tipo aparece uma unica vez, verificado em test_app_ui_flow, que tambem cria Nota pela aba Bancada e verifica texto, cores e redimensionamento. Passaram test_app_layout, test_app_ui_flow, test_app_commands e --self-test-history. Capturas de todas as abas e do papel em build/catalog-review/. Suite completa nao repetida; formato permanece 37.


0.51.1: papel simplificado conforme referencia visual: plano na maior parte, curva localizada no canto direito e sombra discreta. Apenas renderizacao; build recompilado e captura build/catalog-review/paper-simple.bmp revisada. Sem repetir suite de simulacao.


## Publicacao 0.51.1 - 20/11/2026

Trabalho de EEPROM, notas e catalogo integrado na main; tag v0.51.1 publicada como pre-release com instalador, ZIP portatil, fontes completas e hashes. ./build.ps1 -GuiTests e ./tools/package-windows.ps1 passaram, incluindo 9 testes CTest, PATH minimo, pacote movido e instalacao/desinstalacao. Corrigida omissao do codigo do aplicativo no ZIP de fontes: bsdtar com verificacao de erro e inclusao obrigatoria de Bancada-source.zip. Logs em build/release-0.51.1-*.log. Continua sem assinatura digital e sem validacao em VM limpa. Branch docs/audit-consolidation ja era ancestral da main e permanece referenciada pelo outro worktree, que nao foi alterado.
