# Composição de chips — decisão técnica

Consulta e decisão: 16/11/2026.

Fatos: `Part.group` identifica o encapsulamento visível e o solver já recebe as peças
elementares. O editor e a persistência assumiam um único nível. O manual oficial do
[ngspice 45, seção 2.5](https://ngspice.sourceforge.io/docs/ngspice-45-manual.pdf)
descreve subcircuitos compostos e expansão das instâncias antes da simulação.

Alternativas:

| Opção | Custo e consequência |
|---|---|
| Árvores de Workbench independentes | Exige reconstruir índices elétricos e resultados do solver, além de adaptar toda a geometria. |
| Peças elétricas planas com camadas de encapsulamento | Conserva solver e índices; exige preservar os grupos, portas e nomes anteriores por peça. |
| Desagrupar automaticamente | Perde os chips internos; não atende ao pedido. |

Decisão: manter as peças elétricas planas. Cada grupo registra seu pai; cada peça
possui uma lista alocada de suas camadas internas (grupo, portas e nomes).
Encapsular guarda uma camada; entrar/desagrupar retira somente a camada externa.
Copiar, histórico e arquivo preservam as camadas. Os editores formam uma pilha de
contextos isolados. Instâncias continuam independentes, sem atualização implícita.

Hipótese a verificar: congelar contatos entre encapsulamentos em fios internos antes
de agrupar conserva a conectividade sem expor a geometria oculta no exterior.

Limites: permanecem os limites globais documentados de peças, fios e grupos; não há
um novo limite de profundidade. Composição finita não cria referência circular a
modelos de biblioteca. Não há decisão pendente que impeça esse incremento.
