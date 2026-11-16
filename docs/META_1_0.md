# Meta 1.0 — critérios ainda pendentes

> **Critérios para a versão 1.0.** As notas de incremento abaixo registram o estado de cada data; a lista consolidada do que falta está em [ROADMAP_1_0.md](ROADMAP_1_0.md).

Pedido de 14/11/2026: continuar até uma versão 1.0 com catálogo abrangente de componentes interoperáveis, inspirado no Tinkercad, sem declarar concluído um simulador que apenas verifica conectividade.

- Levantar catálogo verificável, modelos, pinagem e casos de validação de cada componente. Capacitores, indutores, portas lógicas, clocks e microcontroladores exigem modelos distintos. Não apresentar uma lista presumida como catálogo completo.
- Fios com joelhos editáveis e curvas, encaixe em terminais e breadboard, seleção e remoção de pontos.
- Aparência própria 2.5D: volume, sombras, terminais legíveis e feedback de interação.
- Simulação elétrica analógica/digital com tempo simulado, passo, pausa, frequência de clock e unidades explícitas. MHz é frequência, não a velocidade de redesenho da janela.
- Menu de configurações e desenvolvimento: visualização de corrente em fios, inspeção de tensões, correntes e estados, captura de imagem, exemplos reproduzíveis e diagnósticos.
- O efeito de fio aceso deve refletir corrente calculada, incluindo direção e escala; não confundir conectividade ou tensão com corrente. Não implementar como uma animação arbitrária.
- Salvar e carregar toda a montagem, incluindo joelhos, parâmetros e configuração; desfazer/refazer; validar formatos e preservar versões anteriores.
- Testes de circuitos de referência, transientes RC/RL, portas, clocks, conflitos e falhas de convergência. Capturar e revisar imagens da aplicação para orientar decisões visuais.

Pesquisa inicial: a [interface C compartilhada do ngspice](https://ngspice.sourceforge.io/shared.html) permite controlar simulação e receber dados; [XSPICE](https://ngspice.sourceforge.io/xspice.html) integra modelos digitais e analógicos. São candidatos, não dependências já integradas. Licença, distribuição Windows e modelos específicos precisam de validação antes da escolha final. Consulta em 14/11/2026.

Estado atual na branch experimental: quinze tipos de componentes, incluindo portas didáticas, clock, flip-flop D e CI NAND aproximado, análise DC pontual pelo ngspice e visualização opcional de correntes em fios quando o modelo permite medi-las sem ambiguidade. O runtime ainda não é distribuído com o aplicativo e a sondagem transitória mostra uma tensão, corrente de componente ou corrente de fio por vez, mas ainda não controla o estado completo do circuito. Não atende à meta 1.0. As tags 0.x representam incrementos, não conclusão do catálogo ou precisão elétrica.


Incremento 0.19.0 (15/11/2026): slots de montagem/configuracao, selecao por area/clipboard,
miniaturas nativas e blocos reutilizaveis com terminais foram acrescentados e testados.
Os blocos preservam componentes reais no solver; encapsulamento inicial em um nivel, sem encaixe
automatico de portas compactas em breadboards. O formato 16 conserva grupos, mas ainda usa enums
para tipos. Esses incrementos nao encerram o catalogo, os limites dinamicos, a instrumentacao
inspecionavel nem o backend de passo real exigidos pela 1.0.


Pedido de qualidade de uso em 15/11/2026: os criterios em [QUALIDADE_DE_USO.md](QUALIDADE_DE_USO.md)
integram esta meta. Exigem chips renomeaveis, portas/polaridade/funcoes legiveis, terminais circulares
claros, editor interno isolado que nao altera a bancada ao consultar e instrumentos com pontas
posicionaveis (multimetro e medidor de clock/pulso). Packing basico e sondagem atual nao comprovam
esses criterios. Capturas e circuitos de referencia devem verificar a experiencia e o modelo eletrico.


Tambem obrigatorios (15/11/2026): escala/proporcoes ajustaveis, terminais dobraveis em modo
explicito separado dos fios, e grid/encaixe configuraveis. Detalhes e verificacao em
QUALIDADE_DE_USO.md; nao considerar entregues sem persistencia e montagem realista na breadboard.


Incremento 0.27.0: perninhas flexiveis, contato direto e backend transiente persistente por contexto
implementados e testados para RC/DFF/botoes/fontes/valores. Ainda ha reinicio ao mudar topologia
ou frequencia, vetores crescentes no solver e catalogo/instrumentos/limites de capacidade pendentes.
Esta versao NAO satisfaz os criterios1.0. Ver README e CONTINUIDADE para comportamento atual.


Pedido ampliado de 15/11/2026: revisar close-ups de todos os componentes nao-CI, ampliar analogicos,
sensores com estimulos controlaveis, atuadores, baterias, instrumentos, reles, transistores e todos
os CIs do catalogo Tinkercad verificavel. Sete segmentos/LCD internos precisam poder aparecer no
exterior de chips personalizados. Lista integral e aceite em CATALOGO_COMPONENTES.md; itens ainda
nao implementados permanecem explicitamente pendentes.
