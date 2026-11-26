# Fonte AC: comparacao tecnica (26/11/2026)

## Fatos

- O [manual oficial do ngspice](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf), consultado em 26/11/2026, define `SIN(V0 VA FREQ ...)`: `VA` e a amplitude de pico e `FREQ` esta em Hz. Uma fonte independente pode usar essa forma em analise transitoria.
- O gerador de funcao atual da Bancada oferece seno, quadrada e triangular, com frequencia regulavel, saida de 0 a 5 V e 50 ohms. Ele ja fornece uma referencia de circuito quando nao ha fonte DC.

## Hipoteses

- "Velocidade" significa frequencia, em Hz; "voltagem" significa amplitude de pico da senoide, em V. A forma de onda desejada e bipolar, centrada em 0 V.

## Recomendacao

Adicionar uma fonte AC separada na pagina de alimentacao. Usar `SIN(0 amplitude frequencia)` em transitorio, pinos de saida e retorno e resistencia serie pequena como na fonte de bancada. Expor amplitude e frequencia no inspetor, salvar ambas no formato do projeto e ajustar o passo da simulacao a frequencia.

## Decisoes pendentes

- Faixa de frequencia e de amplitude pode ser ampliada depois de validar estabilidade do solver e legibilidade da interface nos extremos.
