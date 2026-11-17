# Continuidade

Estado para retomar o desenvolvimento em 17/11/2026. O histórico detalhado permanece no Git;
este arquivo registra apenas fatos e pendências que continuam relevantes.

## Produto e versão

- Versão em preparação: 0.42.0, Windows x64, pré-release experimental.
- O comportamento atual está em [GUIA.md](GUIA.md); visão e critérios da 1.0 estão em
  [VISAO.md](VISAO.md), [META_1_0.md](META_1_0.md) e [ROADMAP_1_0.md](ROADMAP_1_0.md).
- Branch de trabalho: `codex/fix-ngspice-runtime-packages`. Integrar e apagar a branch
  depois da verificação e da publicação, conforme [VERSIONAMENTO.md](VERSIONAMENTO.md).

## Simulação

- A sessão contínua mantém o estado de capacitores e lógica entre avanços. Mudanças de
  topologia ou frequência reiniciam a sessão; esta limitação ainda precisa de solução.
- `.save none`, restaurado após `delete all`, limita o histórico do ngspice à amostra
  atual. Gráficos solicitados com `T` usam análise separada e histórico completo.
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
- `build.ps1` descobre ferramentas no `PATH`, copia as DLLs do build de desenvolvimento
  e testa a interface com `PATH` mínimo. O pacote de distribuição inclui o runtime.
- O executável local de desenvolvimento é `build/build.exe`; arquivos em `build/` são
  descartáveis e não devem aparecer como dependência de fonte ou fixture.

## Verificação e próximos passos

- `./build.ps1 -GuiTests` passou com os testes de simulação, instrumentos e interface.
- Antes da release, executar o empacotamento completo, testes do ZIP e instalador,
  verificar hashes e notas, integrar a `main`, criar a tag anotada e publicar o pré-release.
- Depois da release: preservar estado em mudanças de topologia e frequência, medir
  desempenho em circuitos maiores e avançar nas pendências do roadmap da 1.0.
