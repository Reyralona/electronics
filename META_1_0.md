# Meta 1.0 — critérios ainda pendentes

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
