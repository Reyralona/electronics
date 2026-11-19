# Usabilidade da interface — 18/11/2026

Base: main 02fe084 (0.44.1). Implementação em `feature/interface-usability`.
Escopo autorizado: as cinco prioridades da revisão Impeccable, preservando o visual e o modelo elétrico.

## Comparação técnica antes da implementação

Fontes oficiais consultadas em 18/11/2026:

- [SDL_SetWindowMinimumSize](https://wiki.libsdl.org/SDL3/SDL_SetWindowMinimumSize): limita a área cliente; pode falhar, exige verificar retorno.
- [SDL_SetWindowAspectRatio](https://wiki.libsdl.org/SDL3/SDL_SetWindowAspectRatio): disponível desde SDL 3.2; é uma solicitação, não uma garantia do gerenciador de janelas.
- [SDL_StartTextInput](https://wiki.libsdl.org/SDL3/SDL_StartTextInput): entrada Unicode explícita, pareada com SDL_StopTextInput.
- [UI Automation providers](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-providersoverview): controles personalizados precisam expor provedores para acesso por clientes assistivos.

Fatos locais: layout compartilhado em C; coordenadas lógicas 1280×720; persistência atômica já existente; comandos compartilhados por teclado e mouse.

| Alternativa | Benefício | Custo/limite | Direção |
|---|---|---|---|
| Reutilizar controles e comandos | Preserva comportamento e dependências | Exige foco explícito e geometria coerente | Escolhida |
| Trocar toolkit | Poderia fornecer controles e acessibilidade nativos | Reescrita ampla, distribuição e regressões | Fora deste incremento |
| Reflow de toda a bancada | Aproveita qualquer proporção | Muda todas as coordenadas e testes | Não necessário para as cinco prioridades |
| Escalar a interface inteira | Texto e alvos crescem juntos | Tamanho disponível depende do monitor | Escolhida com proporção preservada e limite visível |
| Nome de projeto em formato novo | Identidade livre | Migração e novos campos persistentes | Resumo de peças/fios sem alterar formato é suficiente neste incremento |

Hipótese a verificar: ações visíveis e busca por nome reduzem a dependência de atalhos. Não houve teste com pessoas.
Recomendação: manter a identidade visual, proteger arquivos e tornar os comandos existentes acessíveis por foco.
Decisão: implementar confirmação de sobrescrita com alvo fixo, resumo dos slots, barra de ações, busca, estado vazio de chips, foco nos controles e configuração de medição próxima ao gráfico.
Leitor de tela: investigação concluída quanto à necessidade de provedores; não declarar suporte sem uma implementação e validação próprias.

## Aceite

- Salvar em vazio funciona; salvar em ocupado exige decisão; cancelar preserva arquivo; confirmar grava somente o slot apresentado.
- Botões de histórico, passo, chip e gráficos executam os comandos existentes; ações indisponíveis não alteram a bancada.
- Tab/Shift+Tab percorrem controles; Enter/Espaço ativam; diálogos prendem o foco; Esc cancela sem confirmar.
- Busca encontra peças por nome, trata texto sem resultado e volta ao catálogo ao limpar.
- Texto, alvos e ponteiro permanecem alinhados ao redimensionar e mudar escala; tamanho indisponível tem causa visível.
- Ritmo fica em Simulação; explicação de gráfico aparece inteira; medida/terminal podem ser escolhidos junto ao gráfico.
- Testes de fluxo incluem casos opostos e reprodução anterior à correção; capturas claro/escuro são revisadas depois do build.

Implementação e evidências: [REVISAO_INTERFACE.md](REVISAO_INTERFACE.md).
