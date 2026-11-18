# Continuidade

Estado para retomar o desenvolvimento em 17/11/2026. O histórico detalhado permanece no Git;
este arquivo registra apenas fatos e pendências que continuam relevantes.

## Produto e versão

- Versão atual: 0.44.0 (sem release). Formato de projeto 34. Última pré-release publicada: 0.43.0, Windows x64.
- O comportamento atual está em [GUIA.md](GUIA.md); visão e critérios da 1.0 estão em
  [VISAO.md](VISAO.md), [META_1_0.md](META_1_0.md) e [ROADMAP_1_0.md](ROADMAP_1_0.md).
- Nenhuma branch de trabalho pendente; tudo integrado à `main`, conforme
  [VERSIONAMENTO.md](VERSIONAMENTO.md).

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

## Verificação e próximos passos

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
