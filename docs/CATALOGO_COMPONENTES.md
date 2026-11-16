# Catálogo solicitado e critérios de aceite

> **Escopo e estado em 16/11/2026 (0.36.0).** As tabelas distinguem modelos presentes de critérios ainda não atendidos. O histórico dos incrementos está no fim.

Pedido do responsável em 15/11/2026. Há 58 tipos no registro atual, conferidos por
`build/build.exe --list-components`. Ter uma peça no catálogo não encerra todos os critérios.
Preservar nomes/IDs, pinagem e leitura dos arquivos anteriores.

## Ordem de trabalho

1. Continuidade eletrica: conservar memoria ao operar controles e ao modificar conexoes; validar
   explicitamente DFF com Q=1, D=0 e clock ja alto. Restaurar apenas ic=1 cria borda falsa no runtime.
2. Capturas ampliadas de cada componente existente, exceto encapsulamentos de CI repetidos.
   Comparar com referencias Tinkercad e fisicas, registrar defeitos, alterar e recapturar.
3. Acrescentar familias em incrementos com modelo, controle, desenho, pinagem, save/undo e testes.
   Nenhum item abaixo deve aparecer como funcional apenas por possuir um desenho.

## Passivos, semicondutores e alimentacao

| Pedido | Modelo/aceite requerido | Estado atual |
|---|---|---|
| Diodo | Juncao nao linear, anodo/catodo claros, faixa no catodo, curva I/V e polarizacao inversa | Implementado: silicio generico; nao linear |
| Transistor NPN | B/C/E claros, corte, regiao ativa e saturacao, modelo SPICE identificado | Implementado: NPN generico SPICE |
| Transistor PNP | Mesmos criterios, polaridades corretas | Implementado: PNP generico SPICE |
| Rele | Bobina R/L, contatos independentes, acionamento/release e histerese identificados | Parcial: bobina R/L e contatos NA/NF; acionamento aproximado, sem histerese temporal |
| Bateria 9 V | Corpo retangular e conector de pressao, polaridade, resistencia interna identificada | Implementado: 9 V / 1 ohm didatico |
| Pilha 1.5 V | Corpo cilindrico, ressalto positivo e negativo plano | Implementado: AA / 0.2 ohm |
| Moeda 3 V | Disco com espessura/borda e identificacao da face positiva | Implementado: 3 V / 15 ohm didatico |
| DIP configuravel | Banco de contatos independentes; quantidade, geometria e estados salvos | Parcial: quatro contatos independentes, estados salvos; quantidade fixa |
| DPST configuravel | Dois polos acionados juntos; distinguir de banco DIP SPST | Implementado: dois polos acionados juntos; geometria fixa |

Baterias nao devem prometer descarga por capacidade se o modelo implementado for fonte constante
com resistencia interna. Antes de desenhar, capturar referencias proximas de cada formato.

## Sensores e ambiente simulado

| Pedido | Estimulo humano/ambiente e resposta requerida | Estado atual |
|---|---|---|
| Fotoresistor | Luz controlavel e resistencia dependente da luz, curva identificada | Implementado: curva generica + lux |
| Fotodiodo | Luz controlavel, fotocorrente e juncao; nao substituir por resistor | Implementado: juncao + fotocorrente generica |
| Sensor de luz ambiente | Luz controlavel e resposta do dispositivo escolhido, alimentacao quando aplicavel | Implementado como fototransistor generico C/E |
| Sensor flexivel | Dobra/angulo controlavel e variacao resistiva identificada | Implementado: curva generica + angulo |
| Sensor de forca | Forca controlavel e curva resistiva aproximada explicita | Implementado: FSR generico + forca |
| Sensor infravermelho | Identificar receptor/reflexao/modulo e sua resposta; estimulo IR controlavel | Implementado: módulo de obstáculo com estímulo controlável e resposta aproximada |
| Sensor de distancia | Distancia controlavel; se ultrassonico, respeitar trigger/echo e tempo de voo | Parcial: IR analógico; ultrassônico com trigger/echo pendente |
| PIR | Movimento controlavel; saida digital alimentada, retencao/tempo identificados | Parcial: saída alimentada por estímulo; sem retenção temporal |
| Umidade do solo | Umidade controlavel; identificar sonda resistiva ou modulo de saida analogica | Implementado: sonda/divisor resistivo generico |
| Inclinacao | Angulo controlavel; contato e orientacao coerentes | Implementado: contato generico + angulo |
| Temperatura | Temperatura em graus C; modelo e faixa identificados | Implementado: transferencia nominal TMP36, alimentado |
| Gas | Concentracao/estimulo controlavel; resposta, aquecimento e limites identificados | Parcial: transferência genérica por estímulo; sem aquecimento ou calibração física |

Controles devem ficar visiveis no inspector, com nome, unidade/faixa e valor atual. Alterar um
slider publica revisao, recalcula sem reiniciar memoria, aceita undo e e salvo no projeto/slot.
O slider representa o estimulo externo, nao uma sobreposicao arbitraria da saida eletrica.
Curvas genericas devem ser rotuladas como aproximacoes; nao inventar calibracao industrial.

## Saidas e atuadores

| Pedido | Modelo/aceite requerido | Estado atual |
|---|---|---|
| LED RGB | Tres juncoes/correntes, pino comum identificado, mistura por corrente de cada cor | Implementado: três junções e brilho por corrente; queima por elemento pendente |
| Lampada | Potencia e aquecimento/brilho; explicitar modelo resistivo/termico | Implementado 0.31.0: resistencia constante 250 ohm (5 V / 0,1 W), brilho pela corrente; sem modelo termico |
| Motor de vibracao | Corrente, inercia e vibracao dependentes do acionamento | Implementado: modelo eletromecânico aproximado |
| Motor CC | R/L, forca contraeletromotriz, corrente, velocidade e sentido | Implementado: R/L, força contraeletromotriz e inércia aproximadas |
| Motor CC com codificador | Modelo de motor e sinais A/B coerentes com posicao/sentido | Implementado: motor CC e quadratura A/B por ângulo calculado |
| Micro servo | Alimentacao, largura de pulso, posicao e dinamica identificadas | Parcial: posição por nível médio; não decodifica largura de pulso |
| Piezo | Modelo eletrico e resposta em frequencia; som opcional coerente com sinal | Parcial: capacitor de 20 nF; sem som ou modelo mecânico |
| Sete segmentos | Segmentos/ponto individuais, corrente e anodo/catodo comum identificados | Implementado: oito junções com correntes próprias; queima por elemento pendente |
| LCD 16x2 I2C | Alimentacao, endereco, barramento e protocolo; nao fingir texto recebido | Feito 0.34.0 (texto por conectividade I2C; protocolo nao simulado) |

### Displays no exterior de chips personalizados

Estado atual: LED RGB e sete segmentos aparecem no corpo do chip. LCD externo e edição do
layout das faces ainda não atendem ao escopo abaixo.

Sete segmentos e LCDs internos podem ser exportados para a face do encapsulamento, inclusive
mais de um. A imagem externa consulta o mesmo estado eletrico dos dispositivos internos; nao
cria uma segunda simulacao. Salvar lista/layout das faces, tamanhos e orientacoes. Renomear ou
redimensionar o chip nao deve apagar displays. O editor interno permanece isolado; aplicar
alteracoes a instancia externa continua explicito e desfazivel. Testar contador interno com
um e varios displays externos antes de marcar este criterio como concluido.

## Instrumentos

| Pedido | Aceite requerido | Estado atual |
|---|---|---|
| Osciloscopio | Pontas reais, canais/referencia, tempo/tensao, escala e historico limitados/configuraveis | Parcial: dois terminais e entrada de 1 MΩ; pontas independentes e vários canais pendentes |
| Multimetro | Pontas reais, tensao/corrente/resistencia, impedancia e modo corretos | Parcial: V/A/Ω por terminais; entrada de 10 MΩ e teste de 1 mA; pontas independentes pendentes |
| Gerador de funcao | Frequencia, amplitude, offset, forma de onda e impedancia de saida | Parcial: seno/quadrada/triangular, frequência ajustável e 50 Ω; amplitude/offset fixos (0–5 V) |

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


## Histórico dos incrementos

As contagens e pendências abaixo descrevem cada versão. Para o estado atual, use as tabelas acima.

### Incremento analogico 0.28.0
14 novos tipos; 36 tipos totais. Sensores possuem sliders de ambiente e Shift+scroll,
com estado salvo e desfazivel. Baterias sao fontes constantes com Rserie, sem capacidade.
Tambem foi criada a revisao reproduzivel de cada tipo em dois temas. Instrumentos, atuadores,
displays externos, DIP/DPST, quatro familias restantes de sensores e inventario integral de CIs
continuam pendentes; este incremento nao conclui todo o pedido nem a continuidade topologica.

### Bloqueio de infraestrutura (antes da 0.32.0)
LED RGB, sete segmentos, DIP, LCD e rele com varios contatos precisam de varias correntes/estados
por peca. Hoje SimAutoResult guarda uma corrente por componente e Part tem um unico `on`.
Antes desses itens: introduzir elementos internos por peca (correntes e estados indexados),
persistidos em formato novo, com protecao de queima por elemento. Nao improvisar com varias pecas.

### Estado 0.32.0 (15/11/2026)
Implementados nesta versao (modelos em DECISOES.md, teste em tests/test_devices.c): LED RGB,
display 7 segmentos (visivel no exterior de blocos), chave DIP 4 vias, DPST, rele 5 V (NA/NF),
motor CC, motor de vibracao, motor CC com encoder, micro servo (aprox. por nivel medio), piezo
passivo, gerador de funcao, multimetro (V/A/Ohm), osciloscopio, sensores PIR, gas, infravermelho
de obstaculo e distancia (IR analogico). 54 tipos no total.
Pendentes: LCD 16x2 I2C (sem microcontrolador/mestre I2C), sensor ultrassonico com trigger/echo,
DIP de tamanho configuravel (hoje 4 vias), inventario de CIs Tinkercad alem de 74HC00/08/32/86.

0.34.0: Arduino Uno, ESP32 DevKit, ATtiny85 (programaveis no editor lateral) e LCD 16x2 I2C. 58 tipos.
