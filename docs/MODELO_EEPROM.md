# EEPROM AT28C16 e bancada de gravação

Implementação 0.48.0; revisão em 20/11/2026. O formato do projeto passa a 36.

## O que é simulado

A memória tem 2048 endereços de oito bits. Conteúdo inicial ausente significa FF em cada endereço.
Os pinos de dados compartilham entrada e saída; CE/OE/WE controlam a direção e a seleção.
A última descida de CE/WE retém o endereço e a primeira subida retém o dado, com OE alto.
O modelo verifica preparação de endereço/OE, preparação/retenção do dado e pulso mínimo de 100 ns.
Aguarda 5 ms após energizar; uma escrita válida termina em 1 ms. Durante esse intervalo, IO7
mostra o complemento do bit programado. Não se deve interpretar os outros sete bits como leitura válida.

O núcleo é C sem SDL/OpenGL, ligado ao d_cosim do ngspice 45.2. Uma instância agregada atende as
EEPROMs do circuito por eventos do solver, sem amostragem por quadro e sem reconstruir o circuito
após cada byte. A DLL pertence ao projeto; não é uma nova dependência baixada.
O processo auxiliar devolve imagens alteradas ao worker; a UI incorpora os bytes quando recebe
o resultado correspondente. Salvar aguarda o worker para não escrever uma imagem anterior.

## Limites explícitos

- Modelo funcional na faixa nominal 4,5–5,5 V; não é um modelo transistor a transistor.
- Não representa desgaste/retentividade ao longo de anos, dispersão térmica ou identificação/apagamento a 12 V.
- Pulsos manuais longos são aceitos para a bancada didática; não validam o limite máximo de pulso da peça real.
- A interrupção de uma gravação pela alimentação conserva o byte anterior; silício real não garante esse resultado.
- A simulação pausada congela o tempo do ciclo interno. A conclusão depende de avançar o tempo simulado.
- Salvar preserva os bytes; fechar um projeto sem salvar continua descartando alterações não salvas.
- Mudanças de topologia continuam sujeitas ao reinício da sessão já documentado no guia.

Esses limites não podem ser apresentados como equivalência elétrica completa com um CI real.

## Circuito de teste na protoboard larga

Arquivo público: `tests/fixtures/eeprom-programmer.elec`. Ele começa apagado, com FF em toda a memória.
Os três blocos da esquerda escolhem A0–A3, A4–A7 e A8–A10; o quarto contato do último bloco não é usado.
Os dois blocos da direita escolhem os oito bits do dado. Cada bloco vai do bit menor ao maior,
da esquerda para a direita. Os LEDs D7–D0 mostram o barramento do bit maior para o menor.

1. Escolha o endereço e o dado pelas pequenas chaves (clique duplo em cada alavanca).
2. Ligue ESCREVER: amarelo aceso; o 74HC245 fornece os dados e a EEPROM libera suas saídas.
3. Pressione e solte GRAVAR. A subida de WE ao soltar inicia o ciclo interno.
4. Desligue ESCREVER: verde LEITURA aceso; o 74HC245 libera o barramento e a EEPROM o controla.
5. Confira os LEDs, mude o endereço e volte. Salve para conservar o conteúdo ao reabrir.

O LED aceso vale 1. D0 a D7 pesam 1, 2, 4, 8, 16, 32, 64 e 128; some os pesos acesos.
5A em hexadecimal é 90 em decimal: D6 + D4 + D3 + D1 = 64 + 16 + 8 + 2.
Isso permite diferenciar o dado colocado nas chaves do byte efetivamente lido depois da gravação.

## Fontes oficiais e evidências

- [Microchip, AT28C16 doc0540](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0540.pdf), consultado em 19/11/2026.
- [ngspice, manual d_cosim](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf), consultado em 19/11/2026;
  a ABI foi conferida contra cosim.h/cmtypes.h do runtime 45.2 instalado, em 20/11/2026.
- `./build.ps1 -GuiTests`: inclui `test_eeprom`, `test_logic_chips` e `test_eeprom_programmer_flow`.
  O fluxo de pessoa cobre abrir, escolher endereço/dado, pressionar/soltar, ler outro endereço,
  desligar/religar, salvar, reabrir e excluir o arquivo temporário, sem usar saves pessoais.
- Cliques reais via Win32 no executável normal, em pasta isolada: gravação de 5A no endereço 2,
  leitura de FF no 3 e salvamento de `000: FF FF 5A FF`. Capturas em `build/human-review/captures/`.
- Comparação das capturas: os 729 pixels mais luminosos dos LEDs de dados têm os mesmos RGB nos dois temas.
