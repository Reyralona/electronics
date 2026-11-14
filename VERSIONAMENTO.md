# Versionamento e recuperação

Usamos `major.medium.minor` (por exemplo, `0.2.0`): major para mudanças grandes ou incompatíveis; medium para funcionalidades; minor para correções e ajustes compatíveis. Antes de 1.0, o projeto ainda está em desenvolvimento e seus formatos e interfaces podem mudar. `VERSION` identifica a versão do código; `CHANGELOG.md` resume cada versão.

`main` guarda incrementos integrados e verificados. Cada trabalho começa em uma branch `feature/nome` ou `fix/nome`, com commits pequenos. Depois de verificar, integramos a branch a `main` e criamos uma tag anotada `vMAJOR.MEDIUM.MINOR` para um ponto de retorno. Enviar branch, main e tags ao remoto preserva cópias compartilhadas. Não reescrever histórico publicado sem motivo e comunicação claros.

Para consultar versões: `git tag --list` e `git log --oneline --decorate --graph --all`. Para examinar uma versão sem alterar main: `git switch --detach v0.1.0`; retorne com `git switch main`. Para desenvolver a partir de uma versão: `git switch -c fix/nome v0.1.0`. Para desfazer um commit publicado mantendo o histórico, use `git revert <commit>` após verificar suas consequências. Antes de trocar de versão, guarde ou faça commit das alterações locais.
