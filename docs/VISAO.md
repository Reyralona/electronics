# Visão do produto

> **Documento original de visão** (14/11/2026): ideia, público, princípios e escopo do MVP. Serve de referência para decisões; o estado atual está no [README](../README.md).

Documento de análise — simulador de elétrica e eletrônica para Windows

Status: visão inicial para descoberta e planejamento
Público: Codex e desenvolvedor responsável
Plataforma-alvo: Windows x64
Idioma inicial: português do Brasil, salvo decisão posterior

1. Resumo da ideia

Criar um aplicativo desktop nativo para Windows, escrito principalmente em C, que permita aprender eletricidade, eletrônica digital e circuitos por meio da montagem e simulação interativa. A pessoa posiciona componentes numa área de trabalho, conecta terminais com fios, executa a simulação e observa estados, sinais e resultados.

A experiência deve combinar a acessibilidade de um simulador didático como o Tinkercad Circuits com a exploração de lógica construída a partir de componentes básicos vista em projetos como Digital Logic de Sebastian Lague. A proposta não é copiar produtos ou projetos: são referências de usabilidade e de progressão conceitual.

O visual deve ser 2.5D: componentes legíveis, com volume e perspectiva suficientes para parecerem objetos manipuláveis, sem exigir uma cena 3D completa. A prioridade é tornar a montagem e o aprendizado claros, responsivos e agradáveis.

2. Problema e público

Problema: ferramentas de simulação podem ser poderosas, mas intimidantes; experiências muito simplificadas, por outro lado, não mostram como circuitos mais complexos emergem de componentes elementares.

Público inicial: iniciantes curiosos por eletricidade e eletrônica, estudantes e pessoas que querem experimentar circuitos sem hardware físico.

Promessa do produto: aprender construindo, testar hipóteses e avançar de circuitos simples até sistemas digitais compostos, com explicações curtas e contexto visual.

3. Princípios de produto

Aprender fazendo: explicação breve, ação imediata, feedback visível.

Causalidade legível: mostrar o que está alimentado, conectado, ligado/desligado ou inválido, sem esconder os limites do modelo.

Começar simples, crescer sem trocar de ferramenta: o mesmo espaço de trabalho deve servir a experiências introdutórias e a montagens digitais maiores.

Erros são parte do aprendizado: identificar erros comuns e oferecer pistas úteis, sem corrigir silenciosamente a montagem.

Interação consistente: selecionar, posicionar, girar, conectar, inspecionar, desfazer e refazer devem ser previsíveis.

Desempenho com limites claros: medir antes de otimizar; definir metas de escala após protótipo e perfil de hardware.

4. Experiência central

Escolher um tutorial, abrir um circuito de exemplo ou começar num espaço em branco.

Encontrar componentes numa biblioteca pesquisável, agrupada por função e nível.

Arrastar componentes para a placa ou área de montagem; posicionar e girar.

Conectar terminais com fios, com indicação clara de encaixe e continuidade.

Iniciar, pausar ou avançar a simulação; interagir com botões, chaves e entradas.

Observar indicadores e estados dos componentes, além de explicações contextuais.

Salvar, duplicar, exportar ou continuar um projeto.

O modo de simulação deve deixar claro quando o circuito está pausado e quando os estados estão sendo atualizados. A interface deve oferecer desfazer/refazer, zoom, pan, seleção múltipla e exclusão segura.

5. Escopo conceitual

Núcleo inicial (MVP proposto)

Tela principal de montagem 2.5D e controles de simulação.

Grade, zoom/pan, seleção, posicionamento, rotação e inspeção de componentes.

Breadboard representada visualmente e conexões que respeitem seus trilhos e grupos de contatos.

Fios entre terminais com encaixe, edição e remoção.

Fonte simples de baixa tensão, fios, resistor, LED/lâmpada, interruptor e botão.

Componentes digitais básicos: entradas/saídas, NOT, AND, OR e XOR; indicadores lógicos.

Simulação determinística adequada aos componentes suportados, com estados explicáveis.

Biblioteca pequena de circuitos de exemplo e tutoriais guiados.

Salvamento e carregamento local de projetos, com formato versionado.

Mensagens de circuito aberto, curto ou conexão inválida apenas quando o modelo elétrico realmente puder identificá-los.

Progressão posterior

Portas compostas e circuitos combinacionais.

Latches SR, flip-flops, registradores, contadores e memória construída por composição.

CI/componentes digitais prontos, para comparar abstração com implementação por portas.

Relés, motores, sensores e mais componentes analógicos.

Instrumentos como multímetro, osciloscópio e sondas lógicas.

Microcontroladores e programação, se o modelo de produto e a arquitetura justificarem.

Compartilhamento, importação/exportação e conteúdo criado pela comunidade.

Esta lista é uma direção, não um compromisso de entregar tudo na primeira versão. Componentes e recursos só entram quando houver modelo de simulação definido, tutorial ou caso de uso claro e critérios de validação.

6. Aprendizagem e tutoriais

Cada componente deve ter uma ficha didática curta e pesquisável:

O que é: uma explicação simples, sem pressupor conhecimento prévio.

O que faz: comportamento observável na simulação.

Terminais: função de cada pino e polaridade quando aplicável.

Como experimentar: uma atividade curta, com resultado esperado.

Erros comuns: sintomas, causa provável e dica de investigação.

Limites do simulador: simplificações relevantes do modelo.

Tutoriais devem ser objetivos, divididos em passos curtos e orientados a uma tarefa. Preferir destacar o próximo componente ou terminal, permitir repetir etapas e explicar por que cada ligação importa. Não interromper a exploração livre com pop-ups desnecessários.

7. Direção visual e interação

Perspectiva 2.5D com leitura imediata de componentes e pinos.

Sombras, profundidade e animações discretas; evitar efeitos que dificultem selecionar terminais.

Estado de seleção, foco, encaixe, conexão e erro distinguível por forma/contorno além de cor.

Fios roteáveis e organizados, sem sobrepor terminais de forma ambígua.

Animação de corrente/sinal apenas se representar corretamente o modelo; velocidade visual não deve sugerir medições físicas inexistentes.

Interface para mouse e teclado, com atalhos visíveis e dicas de acessibilidade.

O modo 2D/2.5D deve manter rótulos e terminais legíveis em diferentes níveis de zoom.

8. Arquitetura a investigar antes de codificar em escala

A intenção é produzir um executável nativo para Windows x64 e usar C como linguagem principal. Isso não determina, por si só, uma API gráfica, biblioteca de interface, formato de projeto ou método de simulação. O Codex deve verificar o ambiente e comparar opções atuais antes de escolher.

Investigar separadamente:

Aplicação/renderização: janela, entrada, UI, desenho 2.5D, texto e acessibilidade.

Modelo do circuito: componentes, pinos, fios, conectividade da breadboard e edição.

Simulação: eventos/estados, propagação digital, elementos analógicos suportados e resolução de conflitos.

Apresentação: conversão de estado simulado em fios/indicadores visuais.

Conteúdo: definições de componentes, tutoriais, exemplos e traduções.

Persistência: formato de projeto versionado, validação e migração.

Manter o modelo de circuito independente da interface e do renderer, sempre que isso não criar complexidade desnecessária. Definir limites do simulador como parte do produto: uma simulação didática não deve alegar precisão de SPICE ou equivalência com hardware real sem validação apropriada.

Antes de implementar uma arquitetura grande, produzir uma decisão técnica curta que compare alternativas, dependências, licença, facilidade de distribuição Windows, compilação, depuração e adequação a C. Marcar claramente fatos verificados, hipóteses e decisões pendentes.

9. Riscos e perguntas em aberto

Simulação analógica realista pode aumentar muito o escopo; decidir se a primeira versão é digital/discreta com alguns elementos elétricos didáticos.

C nativo favorece controle e distribuição enxuta, mas pode tornar interface, tooling e iteração mais custosos; validar isso com protótipo pequeno.

2.5D pode complicar precisão de seleção e roteamento; testar legibilidade com breadboard e fios cedo.

“Tudo do Tinkercad” não é um escopo implementável. Manter paridade como referência futura, não requisito inicial.

Definir plataforma mínima e orçamento de desempenho após conhecer a biblioteca/renderizador e o hardware-alvo.

Definir se o app será totalmente offline, como será distribuído e se conteúdo adicional será incluído no pacote.

Definir política de licenças e assets próprios; não reutilizar código, arte, textos ou componentes proprietários das referências.

10. Etapas sugeridas

Etapa A — descoberta e decisões

Inspecionar repositório e ambiente. Se ainda não houver repositório, propor estrutura mínima e explicar as escolhas. Comparar opções técnicas para renderização/UI, modelo de projeto e simulação. Apresentar riscos e um plano enxuto; não começar uma implementação ampla sem essa decisão revisável.

Etapa B — protótipo vertical

Uma janela funcional com área 2.5D, uma placa simples, fonte, interruptor e LED, fios editáveis, estado de simulação observável, tutorial curto e salvamento/carregamento. O objetivo é provar o ciclo completo, não encher a biblioteca de componentes.

Etapa C — MVP didático

Adicionar componentes essenciais, biblioteca pequena de exemplos, tutoriais, mensagens de diagnóstico, desfazer/refazer e polimento de interação. Definir métricas de desempenho e escala com perfis reproduzíveis.

Etapa D — lógica composta

Adicionar portas e ferramentas para construir, salvar e compartilhar abstrações próprias; em seguida desenvolver experiências guiadas de latch, flip-flop, registrador e memória.

Etapa E — expansão física e conteúdo

Avaliar instrumentos, analógicos, relés, motores, sensores e microcontroladores conforme demanda, validação e arquitetura.

11. Critérios de sucesso iniciais

Uma pessoa iniciante consegue montar e executar o primeiro circuito seguindo o tutorial, sem conhecimento externo.

O usuário consegue descobrir por que um LED não acende, com indicações úteis e sem o simulador afirmar uma causa não observada.

O mesmo circuito pode ser salvo, fechado e reaberto preservando componentes, posições, ligações e estado de edição esperado.

A construção de uma porta lógica a partir de componentes básicos e o uso de uma porta pronta podem ser comparados didaticamente.

A interface permanece legível e responsiva numa escala de circuito definida pelo protótipo.

Os resultados da simulação são reproduzíveis para a mesma montagem e sequência de entradas.

12. Prompt para iniciar o trabalho no Codex

Copie o bloco abaixo para o Codex junto com este documento e o repositório do projeto (se já existir).

Prompt

Quero desenvolver o aplicativo descrito neste documento. Leia o documento inteiro antes de propor alterações.

Atue como analista e engenheiro de software sênior. Trabalhe em etapas pequenas, mantenha o escopo sob controle e exponha incertezas em vez de convertê-las em requisitos. O objetivo de agora é chegar a um primeiro incremento tecnicamente sólido, não implementar toda a visão de uma vez.

Regras de trabalho

Inspecione os arquivos existentes e siga as convenções e instruções do repositório. Não substitua arquitetura ou arquivos sem necessidade demonstrada.

Separe fatos observados no projeto, hipóteses, recomendações e decisões ainda pendentes.

Antes de escolher bibliotecas, APIs ou ferramentas que possam ter mudado, consulte documentação oficial e verifique versões, suporte a Windows x64, licença, distribuição e compatibilidade com C. Cite links e datas de consulta no documento técnico.

Não afirme que C é automaticamente mais rápido ou mais otimizado que qualquer alternativa. Identifique gargalos prováveis e proponha medições para validar desempenho.

Não tente reproduzir “tudo do Tinkercad” no MVP. Use a visão do documento para priorizar um protótipo vertical pequeno e demonstrável.

Mantenha simulação, modelo de circuito, interface e renderização desacoplados quando isso for útil e proporcional ao projeto.

Explicite as simplificações elétricas. Não apresente aproximações didáticas como precisão de SPICE ou comportamento garantido de hardware real.

Evite dependências e abstrações desnecessárias. Registre escolhas arquiteturais relevantes em notas curtas e revisáveis.

Não adicione funcionalidades fora do incremento acordado. Ao final, resuma arquivos alterados, comportamento entregue, comandos para compilar/executar e limitações conhecidas.

Não invente que compilou ou verificou algo: só reporte verificações realmente executadas e seus resultados.

Sua primeira resposta

Primeiro, analise o documento e o repositório sem fazer alterações. Entregue:

resumo da visão e do usuário-alvo;

requisitos funcionais e não funcionais que estão claros;

ambiguidades e riscos, com recomendação para cada um;

comparação curta de alternativas técnicas atuais para aplicação nativa Windows, renderização 2.5D, UI e simulação em C;

arquitetura mínima proposta, separando modelo, simulação, aplicação e apresentação;

plano de protótipo vertical em incrementos pequenos, com critérios de aceite verificáveis;

itens que precisam de uma decisão minha antes de iniciar implementação ampla.

Se não houver repositório, trate isso como fase de descoberta e apresente a estrutura inicial recomendada sem gerar um projeto inteiro ainda. Não edite arquivos nesta primeira etapa.

Depois que eu aprovar o plano, implemente somente o primeiro incremento. Antes de editar, diga em poucas linhas o que vai mudar e quais critérios de aceite ele cobre. Faça a menor fatia funcional de ponta a ponta, sem criar scaffolding amplo. Ao concluir, relate exatamente o que mudou, como executar, o que foi verificado e quais limitações permanecem.