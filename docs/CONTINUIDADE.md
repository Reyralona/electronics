# Continuidade

Estado para retomar o desenvolvimento em 19/11/2026. O histórico detalhado permanece no Git;
este arquivo registra apenas fatos e pendências que continuam relevantes.

## Produto e versão

- Versão integrada na `main`: 0.47.1, incluindo os recursos da 0.47.0, a validação do runtime ngspice
  no build e a auditoria consolidada. Pacotes da pré-release validados em 19/11/2026.
  Formato de projeto 35. Pré-release anterior: 0.46.0, Windows x64.
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
