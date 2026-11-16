# Contexto para continuidade

Instruções para agentes (Codex, Claude) que trabalham neste repositório. Documentos de projeto ficam em `docs/`.

- Leia `docs/VISAO.md` para a visão, os limites do MVP e o fluxo de trabalho; consulte-o sob demanda para evitar repetir contexto.
- Estado e decisões atuais: `README.md`, `docs/GUIA.md` e `docs/DECISOES.md`. Não presuma funcionalidades além das documentadas.
- Meta ampliada e critérios de simulação/depuração: `docs/META_1_0.md`. Não marcar 1.0 sem satisfazer os critérios; usar testes e capturas revisadas para guiar o trabalho.
- Siga `docs/VERSIONAMENTO.md`: trabalho em branches, versões registradas e tags para pontos de retorno. Branches integradas são apagadas; nenhuma branch passa de 7 dias sem integrar a `main`.
- Revisão de qualidade e direção de arquitetura: `docs/QUALIDADE_CODIGO.md`. Siga suas convenções no código novo ou tocado.
- Antes de implementação ampla, registre uma comparação técnica revisável com fontes oficiais e datas de consulta. Separe fatos, hipóteses, recomendações e decisões pendentes.
- Prefira incrementos pequenos e verificáveis; mantenha explícitos os limites do modelo elétrico. Registre decisões e comandos reais de execução e teste conforme surgirem.
- Atualize esta nota apenas com contexto durável e conciso. Não inclua segredos, dados de autenticação nem dados pessoais: o repositório é público.

- Leia `docs/QUALIDADE_CODIGO.md` antes de alterar arquitetura; use `docs/CONTINUIDADE.md` para o ponto de retomada. Refatoração e funcionalidade em commits separados. Código novo: uma instrução por linha, funções curtas, limites nomeados, causas de falha visíveis e modelo sem SDL/OpenGL.

- Qualidade de uso e instrumentos: siga `docs/QUALIDADE_DE_USO.md`. Interior de chip deve ser isolado;
  consultar não altera a bancada. Nomes, funções de pinos e ações principais precisam ser claros na UI.

- Testes de interface como fluxo humano: descreva a sequência de ações de uma pessoa (criar, entrar, renomear,
  salvar, reabrir, excluir) e o resultado visível após cada passo, incluindo casos opostos; confirme que o teste
  falha antes da correção. Exemplo: `tests/test_chip_library_flow.c`.

- Visão consolidada (histórico desde 14/11, revisão de código, pendências da 1.0 em ordem): `docs/ROADMAP_1_0.md`.
