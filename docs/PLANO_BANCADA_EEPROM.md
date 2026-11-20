# Bancada, seleção e EEPROM

Pedido e consulta: 19/11/2026. Base: main 9f36b07, versão 0.47.1.

## Fatos e fontes

- A seleção atual limpa os itens anteriores num clique comum e não trata Ctrl+clique.
- O editor de valor modifica uma peça; a seleção por área já existe.
- O raio de conexão mistura unidades do componente, da bancada e da tela.
- A protoboard tem 420 contatos fixos e dois espaçamentos antigos; aumentar a escala
  da peça também muda a distância entre pinos, impedindo encaixe por translação apenas.
- A EEPROM atual é um decodificador combinacional de conteúdo fixo; não tem ciclo de escrita.
- [Microchip AT28C16, doc0540](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0540.pdf):
  2048 bytes, I/O bidirecional, endereço retido na última descida de CE/WE, dado na primeira
  subida; OE alto para escrever; ciclo interno até 1 ms e polling por IO7; proteção na alimentação.
- [Manual ngspice](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf), seções d_ram e d_cosim:
  d_ram não representa sozinho a EEPROM não volátil; d_cosim permite modelo em DLL e eventos
  digitais. A biblioteca digital do runtime 45.2 instalado contém ambos. O manual on-line é 47+;
  a compatibilidade deve ser confirmada no runtime fixado pelo projeto.

## Comparação e direção

| Alternativa | Vantagem | Problema |
|---|---|---|
| Amostrar EEPROM por quadro da UI | Pouco código | Perde pulsos e depende da velocidade da interface; rejeitada |
| Recriar netlist após cada gravação | Reusa leitura atual | Perde estado de outros CIs e capacitores; rejeitada |
| d_ram puro | Já instalado, eventos digitais | Não inclui ciclo EEPROM nem persistência; insuficiente |
| Modelo de EEPROM por eventos d_cosim | Tempo do solver, barramento e escrita modeláveis | Exige validar ABI, persistência e empacotamento |

Recomendação: manter a bancada e os estilos existentes; selecionar com Ctrl+clique,
editar seleções homogêneas com uma confirmação e um undo, usar alvos em pixels de tela,
alinhar terminais ao passo da protoboard sem impor a escala do corpo. Aumentar a protoboard
por propriedade preservando os identificadores dos contatos existentes.

Hipótese a verificar antes de decidir a implementação da EEPROM: d_cosim no ngspice 45.2
permite atender eventos e devolver memória ao processo principal sem recomeçar a simulação.
Não prometer equivalência analógica completa com silício: registrar temporizações implementadas
e funções ainda não verificadas. Não alterar outros CIs sem defeito concreto reproduzido.

## Aceite

- Teste humano: clicar, Ctrl+clicar peças separadas, alterar resistência, confirmar, desfazer;
  clique comum substitui e Ctrl+clique remove; seleção mista não altera valores incompatíveis.
- Capturas claras/escuras dos mesmos LEDs acesos, com cor física igual.
- Arrastar, redimensionar, conectar e encaixar com zoom alto/baixo e escala reduzida.
- Protoboard maior salva/reabre sem invalidar ligações de arquivos antigos.
- EEPROM: escrever, ler endereço correto e diferente, sobrescrever, inibir escrita,
  desligar/religar, salvar/reabrir e preservar demais estados durante gravação.
- Remontar o slot 1 na protoboard, preservar backup e testar pelo mouse/teclado reais
  do Windows em uma instância isolada, além dos testes de eventos SDL e de modelo.
