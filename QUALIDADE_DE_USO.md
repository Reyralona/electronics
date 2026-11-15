# Qualidade de uso e instrumentos

Pedido explicito do responsavel em 15/11/2026. Requisitos de produto e aceite para a meta 1.0.
Este documento complementa META_1_0.md, prompt.md e QUALIDADE_CODIGO.md. Nao descreve recursos
como concluidos: a secao de estado identifica o que ainda precisa ser implementado.

## Principio de interacao

As operacoes principais devem ser descobertas na propria interface, com botoes, rotulos,
feedback de foco/selecao e ajuda curta. Atalhos aceleram uma acao que tambem deve estar visivel.
Evitar exigir conhecimento de detalhes de implementacao para usar a bancada.

## Chips e blocos reutilizaveis

- Permitir renomear o chip na interface e conservar o nome no projeto e na biblioteca.
- Separar o nome exibido do identificador persistente e do caminho do arquivo.
- Deixar clara a funcao de cada terminal: entradas, saidas, alimentacao e terminais bidirecionais.
- Exibir terminais circulares claros, coerentes com as outras pecas, com feedback de encaixe.
- Rotular os pinos com nomes uteis, nao apenas P1/P2 sem contexto; mostrar a correspondencia interna.
- Permitir corrigir a classificacao/nome das portas no editor de bloco. A classificacao visual
  nao deve introduzir uma aproximacao eletrica nem forcar direcao em um terminal analogico.

## Interior isolado

- Abrir o interior em um ambiente separado da montagem principal, com nome e caminho de contexto
  evidentes, por exemplo `Bancada > Meu contador`.
- Apenas consultar o interior nao altera posicoes, fios, parametros, selecao, historico ou estado
  eletrico da bancada externa. Uma simulacao do interior pertence ao contexto isolado.
- Disponibilizar salvar, renomear, testar e voltar para a bancada nesse ambiente.
- Diferenciar salvar um modelo na biblioteca de aplicar alteracoes a uma instancia da bancada.
  Aplicacao ao circuito externo deve ser uma acao explicita e desfazivel.
- Indicar alteracoes nao salvas e oferecer salvar/descartar/cancelar ao sair. Nunca descartar
  edicoes silenciosamente nem atualizar outras instancias sem uma acao explicita.
- Resultados assincronos devem carregar a identidade do documento/contexto e a revisao correta;
  um resultado do editor nao pode aparecer na bancada ou vice-versa.

## Identificacao eletrica dos pinos

- Disponibilizar funcao/polaridade em cada componente, com rotulos e ajuda ao apontar.
- Distinguir V+/V-, VCC/GND, anodo/catodo, entrada, saida, entrada de clock e contato passivo.
- Componentes sem polaridade devem ser identificados como tal. GND, negativo de uma fonte,
  contato de breadboard e neutro de rede nao sao conceitos intercambiaveis.
- Distinguir a funcao do terminal da tensao calculada em relacao a uma referencia. O sinal da
  tensao medida nao muda o nome ou a polaridade construtiva do terminal.
- Usar forma/rotulo alem de cor; deixar a informacao legivel com zoom e selecao.

## Instrumentos com pontas posicionaveis

Pedido obrigatorio: multimetro com pontas que o usuario realmente conecta aos pontos de medida;
medidor de clock/pulso. As pontas precisam aparecer e ser salvas junto com a configuracao do instrumento.

- Tensao: duas pontas e referencia explicitamente escolhidas, unidade, sinal e modo DC/AC claros.
- Corrente: conexao em serie e modelo do instrumento explicitos. Nao estimar corrente a partir
  da tensao ou conectividade, nem tratar uma ligacao paralela como amperimetro valido.
- Resistencia/continuidade: definir modelo e condicoes de validade; comunicar quando fontes ativas
  ou o restante do circuito tornam a leitura inadequada. Nao mostrar uma resistencia inventada.
- Clock/pulso: frequencia, periodo, duty cycle e niveis/limiares claros; resultados derivados
  de amostras simuladas. Comunicar ausencia de bordas ou resolucao insuficiente.
- Separar tempo simulado, taxa de amostragem e velocidade de reproducao/redesenho.
- Registrar impedancia/carga e demais aproximacoes do instrumento; evitar que uma sonda ideal
  seja apresentada como um multimetro fisico com parametros nao modelados.

Instrumentos adicionais sugeridos para avaliacao: osciloscopio de varios canais, sonda logica,
contagem de bordas e medidas de largura de pulso. Sao propostas; definir modelos e casos de
validacao antes de inclui-los como entregues.

## Criterios de verificacao

- Renomear, salvar/reabrir, copiar e instanciar preservam nomes e mapeamentos das portas.
- Abrir/consultar/fechar um chip deixa a montagem externa identica; editar e cancelar tambem.
- Aplicar alteracoes muda somente o alvo escolhido e pode ser desfeito.
- Capturas revisadas demonstram pinos/rotulos, contexto isolado e pontas dos instrumentos.
- Circuitos de referencia verificam tensao diferencial, corrente em serie e medidas de pulso;
  casos invalidos produzem causa visivel. Testes devem cobrir separacao entre contextos.

## Estado atual (0.20.0)

Entregues: slots de montagem/configuracoes, miniaturas, selecao por area/clipboard e packing
basico com instancias independentes. A 0.20.0 acrescenta funcao/nome dos pinos ao registro,
rotulos de selecao, ajuda ao apontar e terminais de blocos com circulos iguais aos demais.
Portas de blocos mostram a funcao do terminal interno; classificacao personalizada e semantica
especifica do circuito ainda precisam de suporte no editor.

Pendentes: renomeacao, editor interno isolado, portas configuraveis, identificacao completa dos
pinos e instrumentos com pontas. Ctrl+Shift+G atualmente desempacota na propria bancada e altera
a montagem; essa operacao nao atende ao requisito de consultar o interior isoladamente.


## Incremento do editor isolado (0.21.0)
Ver interior/Enter abre copia independente, com cabecalho EDITOR ISOLADO. Renomear/F4 aceita
acentos. Salvar modelo conserva o rascunho na biblioteca, sem modificar a bancada; Aplicar ao
chip altera somente a instancia original, com undo. Voltar/Esc e fechar pedem decisao quando ha
alteracoes nao salvas. Cancelar continua no editor; descartar preserva a montagem externa.
Os testes verificam isolamento de modelo, historico, preferencias e resultados assincronos.
Nomes persistem no formato 17, com leitura dos anteriores. Limite provisoriamente explicito de
127 bytes UTF-8; nomes maiores precisam de armazenamento dinamico antes da 1.0.
Ainda pendentes: configurar semantica/portas no editor, instrumentos com pontas e demais gates
1.0. Desempacotar permanece operacao separada que altera a bancada.


## Escala, terminais flexiveis e alinhamento (pedido de 15/11/2026)
Requisitos obrigatorios: redimensionar componentes; corrigir proporcao padrao entre resistor e
breadboard para permitir montagem plausivel; dobrar terminais como condutores fisicos, mantendo
a identidade eletrica de cada pino. Nao confundir deformacao de terminal com criacao de fio.
A interface deve apresentar modo explicito para editar terminais, com controles/atalhos distintos
e feedback antes de arrastar. Ajuste visual nao altera resistencia, capacitancia ou outros
parametros eletricos. Geometria ajustada deve participar de hit testing, encaixe, copia, undo,
packing, editor isolado e persistencia, inclusive migracao de arquivos antigos.
Grid configuravel: espacamento, visibilidade e encaixe opcionais; permitir alinhamento consistente
e encaixe nos furos da breadboard. Mostrar configuracao e permitir suspender encaixe durante
arraste com controle claro. Preferencias devem ser salvas e independentes do zoom.
Verificar com montagem resistor/LED/breadboard, terminais dobrados ligados aos furos corretos,
zoom/rotacao, salvar/reabrir, desfazer/refazer e ausencia de confusao com ligacao de fios.
Ainda nao implementado: requer comparacao tecnica antes de mudar arquitetura/geometria.


Pedido adicional de 15/11/2026: duplo clique em trecho de fio ou terminal flexivel adiciona
joelho no ponto indicado, preservando ordem do caminho; duplo clique em joelho existente remove.
Priorizar joelho existente para evitar adicionar e apagar no mesmo gesto. Clique simples segue
selecionando/arrastando; feedback deve explicar a acao. Incluir undo, persistencia, zoom e curvas
nos testes. Remocao por duplo clique em fios ja existe; insercao por trecho ainda pendente.


Grid 0.22.0: controles de espacamento, encaixe e breadboard em F1; Alt suspende; preferencias
persistidas. Verificados posicionamento, arraste, suspensao e salvar/carregar por eventos GUI.
Colagem/preview ainda precisam refletir encaixe; redimensionamento e dobra continuam pendentes.


0.23.0: insercao por duplo clique em trecho de fio implementada, com projecao no caminho
renderizado, ordem preservada e undo/persistencia. Joelho existente tem prioridade quando os
pontos de controle estao visiveis. Terminais flexiveis e seu modo dedicado ainda pendentes.


0.24.0: controle Tamanho no inspetor, 1%-10000%, com undo/persistencia e geometria dos pinos
compartilhada. Resistor reduzido pode ocupar furos vizinhos. Parametros eletricos sao preservados.
Pecas novas mantem tamanho anterior; presets proporcionais e calibracao fisica ainda precisam
ser definidos. Terminais dobraveis e modo dedicado continuam pendentes.
