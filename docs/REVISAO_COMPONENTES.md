# Revisão dos desenhos dos componentes

Data: 19/11/2026. Branch `feature/interface-usability`. Revisão visual local, sem alteração do modelo elétrico.

## Evidência

- 59 tipos capturados em 1×, 3× e 7×: `build/component-review/contact-sheet-{1,3,7}x.png`.
- Imagens individuais: `build/component-review/<id>-<zoom>x.png`.
- Capturas individuais completas, sem recorte: `build/component-review/full/<id>-<zoom>x.png`.
- Portas normais e espelhadas: `build/mirror-gates-review.bmp`.
- Captura de um tipo: `build/build.exe --capture-component=and@7 --capture-output=build/and.bmp`.
- Captura comparativa das portas: `build/build.exe --capture-mirror-gates --capture-output=build/mirror-gates.bmp`.
- Recriação das 177 imagens individuais completas: `python tools/capture-components.py --executable build/build.exe` (requer Pillow).

Os arquivos em `build/` são descartáveis. Os comandos acima recriam as imagens depois de compilar.

## Constatações

As imagens individuais em 1×, 3× e 7× foram conferidas por família:

| Família | Componentes revistos | Resultado |
|---|---|---|
| Alimentação e controles | `source`, `switch`, `clock`, `button`, `potentiometer`, `battery-9v`, `battery-aa`, `battery-coin`, `function-generator` | Corpo, controles e polaridade distinguíveis. |
| Passivos e sensores discretos | `resistor`, `capacitor`, `inductor`, `diode`, `photodiode`, `ldr`, `flex`, `force`, `tilt`, `soil`, `temperature`, `ambient-light`, `lamp`, `npn`, `pnp` | Contorno do sensor de luz ambiente reforçado. |
| Lógica | `and`, `nand`, `or`, `xor`, `not`, `nor`, `xnor`, `dff`, `74hc00`, `74hc08`, `74hc32`, `74hc86` | Espelho das portas corrigido; inscrições 74HC têm limite em zoom alto. |
| Indicadores e chaves | `led`, `rgb-led`, `seven-segment`, `dip-switch`, `dpst` | Contorno do LED RGB desligado reforçado. |
| Motores e saídas | `relay`, `dc-motor`, `vibration-motor`, `encoder-motor`, `servo`, `piezo` | Silhuetas e terminais distinguíveis. |
| Instrumentos | `multimeter`, `oscilloscope` | Leitura e controles distinguíveis. |
| Módulos sensores | `pir`, `gas`, `ir-obstacle`, `distance` | Contorno do domo PIR reforçado. |
| Placas e conexão | `breadboard`, `arduino-uno`, `esp32`, `attiny85`, `lcd-16x2-i2c`, `junction` | Revisados nas imagens completas para evitar o recorte da folha de contato. |

- AND, NAND, OR e XOR tinham os trechos desenhados dos terminais espelhados duas vezes. Os terminais do modelo já estavam do lado correto. A captura comparativa confirma uma entrada dupla de um lado e saída única do outro nas quatro portas.
- A leitura do buffer frontal depois da troca de quadros podia produzir BMPs pretos. A captura agora lê o buffer traseiro antes da troca e rejeita um quadro totalmente preto.
- Em 1×, os contornos das peças pequenas e dos terminais permanecem distinguíveis. Em 3× e 7×, a linguagem de cores e materiais é consistente entre fonte, passivos, portas e instrumentos.
- A lente desligada do LED RGB, o sensor de luz ambiente e o domo do PIR tinham pouco contraste sobre a bancada clara. Seus contornos foram reforçados e revistos em tema claro e escuro.
- A inscrição dos chips 74HC fica suave em zoom muito alto porque o texto rasterizado é ampliado junto com o desenho; em 1× e 3× continua legível. Uma mudança no renderizador de fontes precisa ser tratada em incremento próprio para não alterar o texto do restante da interface.
- As placas grandes, como Arduino e ESP32, exigem a imagem individual completa para revisão; a folha de contato recorta as laterais em zoom alto. As imagens completas também foram geradas.

## Verificação

- Capturas geradas para todos os 59 tipos nas três escalas, sem imagens pretas.
- Teste de interface com LEDs a 20 unidades e passivos próximos passou.
- `./build.ps1 -GuiTests -OutputName capture-review` passou, incluindo o autoteste gráfico.
- Depois do polimento, `./build.ps1 -GuiTests -OutputName final-review` passou, incluindo o autoteste gráfico e os testes de simulação.
- A captura comparativa foi repetida com o executável final; a imagem de 1280×720 tem variação de cor nos três canais, e não um quadro preto.
