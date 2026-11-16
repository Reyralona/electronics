# Versionamento e recuperação

> **Regras de versões, branches e tags** do projeto. `VERSION` e `CHANGELOG.md` ficam na raiz do repositório.

Usamos `major.medium.minor` (por exemplo, `0.2.0`): major para mudanças grandes ou incompatíveis; medium para funcionalidades; minor para correções e ajustes compatíveis. Antes de 1.0, o projeto ainda está em desenvolvimento e seus formatos e interfaces podem mudar. `VERSION` identifica a versão do código; `CHANGELOG.md` resume cada versão.

`main` guarda incrementos integrados e verificados. Cada trabalho começa em uma branch `feature/nome` ou `fix/nome`, com commits pequenos. Depois de verificar, integramos a branch a `main` e criamos uma tag anotada `vMAJOR.MEDIUM.MINOR` para um ponto de retorno. Enviar branch, main e tags ao remoto preserva cópias compartilhadas. Não reescrever histórico publicado sem motivo e comunicação claros.

A partir de 0.36.0, cada versão distribuída também tem uma GitHub Release com instalador Windows,
ZIP portátil, fontes correspondentes e hashes. Depois de enviar a tag, executar
`./tools/publish-release.ps1 -Tag vMAJOR.MEDIUM.MINOR` com os artefatos testados em `build/dist`.
Por enquanto são pré-releases experimentais, sem assinatura digital. O fluxo de validação e publicação
está em [DISTRIBUICAO_WINDOWS.md](DISTRIBUICAO_WINDOWS.md); o envio de tags ainda não publica sozinho.

## Vida curta das branches

- Uma branch existe só enquanto seu trabalho não foi integrado. Logo após integrá-la a `main` e criar a tag, apague-a localmente e no remoto (`git branch -d nome`; `git push origin --delete nome`). A tag preserva o ponto de retorno.
- Integre a `main` com frequência. Uma branch com mais de **7 dias** sem integração deve ser integrada, atualizada a partir da `main` ou apagada, com a decisão registrada no commit ou no `CHANGELOG.md`.
- Cada agente ou pessoa mantém no máximo uma branch ativa de trabalho, com prefixo identificável quando houver vários agentes (por exemplo, `claude/nome`), e não altera a branch ou a pasta de trabalho de outro agente.
- Revisão periódica, antes de começar trabalho novo: `git fetch --prune`, `git branch --merged main` (candidatas a apagar) e `git branch --no-merged main` (trabalho pendente). Não apague branch com commits não integrados sem confirmação do responsável.
- Executáveis de teste temporários em `build/` são descartáveis. O executável a usar é sempre `build/build.exe`, gerado por `./build.ps1`. Não acumule cópias com nomes de etapa (`-OutputName` só quando o build.exe estiver aberto, e apague a cópia depois).

Para consultar versões: `git tag --list` e `git log --oneline --decorate --graph --all`. Para examinar uma versão sem alterar main: `git switch --detach v0.1.0`; retorne com `git switch main`. Para desenvolver a partir de uma versão: `git switch -c fix/nome v0.1.0`. Para desfazer um commit publicado mantendo o histórico, use `git revert <commit>` após verificar suas consequências. Antes de trocar de versão, guarde ou faça commit das alterações locais.

## Proteção no GitHub

O repositório é público e tem dois rulesets (configurados em 16/11/2026 com `gh api .../rulesets`):

- **Escrita apenas pelo dono:** em `main`, `feature/**` e `fix/**`, só administradores do repositório podem enviar, apagar ou reescrever. Outras pessoas contribuem por fork e pull request.
- **Histórico da main protegido:** ninguém, nem o dono, pode dar force-push ou apagar a `main`. Para desfazer algo publicado, use `git revert`.

Branches de agentes (`claude/**`, `codex/**`) não são protegidas e podem ser apagadas depois de integradas. Consultar: `gh api repos/Reyralona/electronics/rulesets`.
