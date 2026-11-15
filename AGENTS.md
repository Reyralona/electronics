# Contexto para continuidade

- Leia `prompt.md` para a visão, os limites do MVP e o fluxo de trabalho; consulte-o sob demanda para evitar repetir contexto.
- Estado e decisões atuais: consulte `README.md` e `DECISOES.md`. Não presuma funcionalidades além das documentadas.
- Meta ampliada e critérios de simulação/depuração: `META_1_0.md`. Não marcar 1.0 sem satisfazer os critérios; usar testes e capturas revisadas para guiar o trabalho.
- Siga `VERSIONAMENTO.md`: trabalho em branches, versões registradas e tags para pontos de retorno. Branches integradas são apagadas; nenhuma branch passa de 7 dias sem integrar a `main`.
- Revisão de qualidade e direção de arquitetura: `QUALIDADE_CODIGO.md`. Siga suas convenções no código novo ou tocado.
- Antes de implementação ampla, registre uma comparação técnica revisável com fontes oficiais e datas de consulta. Separe fatos, hipóteses, recomendações e decisões pendentes.
- Prefira incrementos pequenos e verificáveis; mantenha explícitos os limites do modelo elétrico. Registre decisões e comandos reais de execução e teste conforme surgirem.
- Atualize esta nota apenas com contexto durável e conciso. Não inclua segredos ou dados de autenticação.

- Leia `QUALIDADE_CODIGO.md` antes de alterar arquitetura; use `CONTINUIDADE.md` para o ponto de retomada. Refatoracao e funcionalidade em commits separados. Codigo novo: uma instrucao por linha, funcoes curtas, limites nomeados, causas de falha visiveis e modelo sem SDL/OpenGL.

- Qualidade de uso e instrumentos: siga `QUALIDADE_DE_USO.md`. Interior de chip deve ser isolado;
  consultar nao altera a bancada. Nomes, funcoes de pinos e acoes principais precisam ser claros na UI.
