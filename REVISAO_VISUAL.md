# Revisao visual de componentes

Rodada inicial: 15/11/2026, base v0.27.0. Capturas reais do aplicativo, nao mockups.
Comando: ./tools/capture-components.ps1 -Destination build/component-review/before
Foram geradas 36 imagens: 18 tipos nao-DIP em dois temas, zoom 7x (breadboard 1.8x).
Galeria: [antes](build/component-review/before/index.html). Resumo visual em
[contact sheet](build/component-review/before/contact-sheet.png).
Referencias: guia oficial Tinkercad Circuits, paginas 6/7 em build/visual-references, e
https://www.tinkercad.com/circuits (consultados em 15/11/2026). Arte da Bancada permanece propria.

## Achados nas capturas antes de alterar codigo

| Peca/familia | Achado concreto | Correcao a verificar |
|---|---|---|
| Todas as pecas comuns | Bolinhas eletricas crescem com zoom e parecem argolas maiores que os terminais | Manter marcadores em tamanho de tela e metal proporcional ao corpo |
| Fonte | Caixa e sinais quase planos; traco de 1 px fica fino no close-up | Faces/material e espessura de gravacao proporcionais |
| Interruptor | Desenho de simbolo esquematico, sem alavanca fisica | Caixa, trilho/atuador e contatos legiveis |
| LED | Corpo vertical correto, mas base termina como outra semiesfera, sugerindo formato estranho | Flange eliptica e lente com degradacao/volume |
| Breadboard | Proporcoes legiveis, buracos pouco rebaixados e sem marcacoes | Rebaixos, fenda central e marcacoes coerentes sem mudar contatos |
| Resistor | Corpo retangular; faixas fixas nem sempre correspondem ao valor | Corpo axial arredondado e codigo nominal de cores |
| Capacitor | Faixa sugere eletrolitico polarizado, mas modelo e nao polarizado | Aparencia de capacitor nao polarizado coerente com modelo |
| Indutor | Parece outro resistor com listras, bobinas sem curvatura | Ferrite e espiras de cobre com volume |
| Potenciometro | Disco e eixo ja distinguiveis, faltam terminais/material | Lugs, eixo e faces metalicas |
| Botao | Corpo plano, atuador distinguivel; faltam fixacao e volume | Base, cantos e cabeca com altura |
| Clock | Simbolo e corpo planos, tracos finos no zoom | Corpo de instrumento/bloco identificado, tracos coerentes |
| AND/NAND/OR/XOR/NOT/NOR/XNOR/DFF | Blocos didaticos, nao CIs fisicos; simbolos muito finos, rotulos ampliados pixelizam | Tracos proporcionais e texto de anotacao em tamanho de tela |

Nao substituir silenciosamente a fonte ajustavel por bateria de 9 V; os tres formatos de bateria
solicitados sao tipos novos, com capturas de referencia proprias antes de modelar.
Cada componente novo tambem deve entrar no capturador, em ambos os temas. Revisar silhueta,
orientacao, polaridade, terminais e estados ativo/inativo antes de marcar como pronto.
