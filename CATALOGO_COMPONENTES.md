# Catalogo solicitado e criterios de aceite

Pedido do responsavel em 15/11/2026. Este e um registro de escopo, nao uma lista de recursos
concluidos. Estado inicial: 22 tipos na v0.27.0. Preservar nomes/IDs, pinagem e arquivos anteriores.

## Ordem de trabalho

1. Continuidade eletrica: conservar memoria ao operar controles e ao modificar conexoes; validar
   explicitamente DFF com Q=1, D=0 e clock ja alto. Restaurar apenas ic=1 cria borda falsa no runtime.
2. Capturas ampliadas de cada componente existente, exceto encapsulamentos de CI repetidos.
   Comparar com referencias Tinkercad e fisicas, registrar defeitos, alterar e recapturar.
3. Acrescentar familias em incrementos com modelo, controle, desenho, pinagem, save/undo e testes.
   Nenhum item abaixo deve aparecer como funcional apenas por possuir um desenho.

## Passivos, semicondutores e alimentacao

| Pedido | Modelo/aceite requerido | Estado inicial |
|---|---|---|
| Diodo | Juncao nao linear, anodo/catodo claros, faixa no catodo, curva I/V e polarizacao inversa | Pendente |
| Transistor NPN | B/C/E claros, corte, regiao ativa e saturacao, modelo SPICE identificado | Pendente |
| Transistor PNP | Mesmos criterios, polaridades corretas | Pendente |
| Rele | Bobina R/L, contatos independentes, acionamento/release e histerese identificados | Pendente |
| Bateria 9 V | Corpo retangular e conector de pressao, polaridade, resistencia interna identificada | Pendente |
| Pilha 1.5 V | Corpo cilindrico, ressalto positivo e negativo plano | Pendente |
| Moeda 3 V | Disco com espessura/borda e identificacao da face positiva | Pendente |
| DIP configuravel | Banco de contatos independentes; quantidade, geometria e estados salvos | Pendente |
| DPST configuravel | Dois polos acionados juntos; distinguir de banco DIP SPST | Pendente |

Baterias nao devem prometer descarga por capacidade se o modelo implementado for fonte constante
com resistencia interna. Antes de desenhar, capturar referencias proximas de cada formato.

## Sensores e ambiente simulado

| Pedido | Estimulo humano/ambiente e resposta requerida | Estado inicial |
|---|---|---|
| Fotoresistor | Luz controlavel e resistencia dependente da luz, curva identificada | Pendente |
| Fotodiodo | Luz controlavel, fotocorrente e juncao; nao substituir por resistor | Pendente |
| Sensor de luz ambiente | Luz controlavel e resposta do dispositivo escolhido, alimentacao quando aplicavel | Pendente |
| Sensor flexivel | Dobra/angulo controlavel e variacao resistiva identificada | Pendente |
| Sensor de forca | Forca controlavel e curva resistiva aproximada explicita | Pendente |
| Sensor infravermelho | Identificar receptor/reflexao/modulo e sua resposta; estimulo IR controlavel | Pendente |
| Sensor de distancia | Distancia controlavel; se ultrassonico, respeitar trigger/echo e tempo de voo | Pendente |
| PIR | Movimento controlavel; saida digital alimentada, retencao/tempo identificados | Pendente |
| Umidade do solo | Umidade controlavel; identificar sonda resistiva ou modulo de saida analogica | Pendente |
| Inclinacao | Angulo controlavel; contato e orientacao coerentes | Pendente |
| Temperatura | Temperatura em graus C; modelo e faixa identificados | Pendente |
| Gas | Concentracao/estimulo controlavel; resposta, aquecimento e limites identificados | Pendente |

Controles devem ficar visiveis no inspector, com nome, unidade/faixa e valor atual. Alterar um
slider publica revisao, recalcula sem reiniciar memoria, aceita undo e e salvo no projeto/slot.
O slider representa o estimulo externo, nao uma sobreposicao arbitraria da saida eletrica.
Curvas genericas devem ser rotuladas como aproximacoes; nao inventar calibracao industrial.

## Saidas e atuadores

| Pedido | Modelo/aceite requerido | Estado inicial |
|---|---|---|
| LED RGB | Tres juncoes/correntes, pino comum identificado, mistura por corrente de cada cor | Pendente |
| Lampada | Potencia e aquecimento/brilho; explicitar modelo resistivo/termico | Pendente |
| Motor de vibracao | Corrente, inercia e vibracao dependentes do acionamento | Pendente |
| Motor CC | R/L, forca contraeletromotriz, corrente, velocidade e sentido | Pendente |
| Motor CC com codificador | Modelo de motor e sinais A/B coerentes com posicao/sentido | Pendente |
| Micro servo | Alimentacao, largura de pulso, posicao e dinamica identificadas | Pendente |
| Piezo | Modelo eletrico e resposta em frequencia; som opcional coerente com sinal | Pendente |
| Sete segmentos | Segmentos/ponto individuais, corrente e anodo/catodo comum identificados | Pendente |
| LCD 16x2 I2C | Alimentacao, endereco, barramento e protocolo; nao fingir texto recebido | Pendente |

### Displays no exterior de chips personalizados

Sete segmentos e LCDs internos podem ser exportados para a face do encapsulamento, inclusive
mais de um. A imagem externa consulta o mesmo estado eletrico dos dispositivos internos; nao
cria uma segunda simulacao. Salvar lista/layout das faces, tamanhos e orientacoes. Renomear ou
redimensionar o chip nao deve apagar displays. O editor interno permanece isolado; aplicar
alteracoes a instancia externa continua explicito e desfazivel. Testar contador interno com
um e varios displays externos antes de marcar este criterio como concluido.

## Instrumentos

| Pedido | Aceite requerido | Estado inicial |
|---|---|---|
| Osciloscopio | Pontas reais, canais/referencia, tempo/tensao, escala e historico limitados/configuraveis | Pendente |
| Multimetro | Pontas reais, tensao/corrente/resistencia, impedancia e modo corretos | Pendente |
| Gerador de funcao | Frequencia, amplitude, offset, forma de onda e impedancia de saida | Pendente |

## Circuitos integrados do Tinkercad

Pedido: TODOS os circuitos integrados disponiveis no Tinkercad. A cobertura completa ainda nao
foi verificada numa fonte oficial enumeravel. Nao substituir esse pedido por uma lista presumida
de quatro portas. Ja presentes: 74HC00/08/32/86, alem dos blocos logicos didaticos.
Inventariar nome exato, fabricante/modelo de referencia, pinagem, alimentacao, verdade/estado,
limites e teste de cada CI. Catalogo do produto pode mudar: registrar data e evidencia consultada.

## Revisao visual e evidencias

Guardar close-ups antes/depois por ID de componente e tema. Avaliar silhueta, proporcao corpo/
terminais, volume 2.5D, material, orientacao, polaridade e legibilidade. LEDs devem permanecer em
pe, resistores/pot/indutor devem parecer objetos fisicos e os tres formatos de bateria devem ser
distintos. Nao reutilizar imagens do Tinkercad como assets da Bancada.
Referencias iniciais oficiais: https://www.tinkercad.com/circuits e guia de Circuits em
https://assets.ctfassets.net/jl5ii4oqrdmc/4sMFqe3rDlbUymJt0I4yh/85a4487f7fe274e74c19870ae4679fc1/tinkercad-guides_circuits-Printable.pdf
(consulta em 15/11/2026). O guia possui screenshots, mas nao e um inventario completo dos CIs.
