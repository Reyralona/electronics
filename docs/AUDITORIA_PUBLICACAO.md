# Auditoria de referências locais nas publicações

Consulta: 17/11/2026. Escopo: notas públicas dos seis releases, mensagens das tags locais,
árvores versionadas e testes do repositório.

## Fatos verificados

- As mensagens das tags locais e as notas dos releases v0.36.0, v0.37.0, v0.37.1,
  v0.38.0, v0.39.0 e v0.41.0 não continham caminhos de perfil pessoal.
- Cinco notas mencionavam uma pasta de dados local padrão do Windows. As cinco foram
  atualizadas no GitHub para dizer apenas "diretório de dados do aplicativo"; a leitura
  após a edição confirmou a remoção.
- A nota de v0.41.0 continha uma medida feita sobre um save pessoal, indisponível para
  reprodução no repositório. A frase foi removida do release público e do changelog
  corrente. A tag histórica não foi reescrita.
- A busca nas árvores das tags por caminhos de perfil pessoal não retornou ocorrências.
- Os testes atuais usam circuitos e dados em `tests/` ou geram arquivos em `build/`.
  Testes antigos vinculados a um save pessoal e seus utilitários de reparo foram
  removidos. A fixture do Cockcroft-Walton está em `tests/fixtures/`.
- Scripts de compilação e testes deixaram de exigir uma instalação de ferramentas em
  um diretório fixo. As ferramentas necessárias devem estar no `PATH`.

## Decisões

- Notas de release futuras são rejeitadas se contiverem caminhos de perfil pessoal,
  caminhos `file://` ou caminhos de rede locais.
- Não reescrever tags históricas: a auditoria não encontrou dados pessoais nelas, e
  mover pontos de retorno publicados exigiria substituir os artefatos correspondentes.
- Arquivos de teste gerados em `build/` continuam descartáveis e não são fonte de dados.

## Verificação

`git grep` nas tags, leitura das seis notas via GitHub CLI, busca por caminhos absolutos
no checkout, parser de PowerShell para os scripts alterados e `git diff --check`.
`./build.ps1 -GuiTests -OutputName audit-build` passou com `gcc.exe` e `bsdtar.exe` no `PATH`.
