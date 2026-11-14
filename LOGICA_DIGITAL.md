# Integração digital: comparação e prova de conceito

Consulta: 14/11/2026. Esta nota separa o que foi verificado das decisões ainda abertas.

## Fatos e fontes

- O [manual oficial ngspice 45, seções 8.3 e 8.4](https://ngspice.sourceforge.io/docs/ngspice-45-manual.pdf) descreve portas digitais por eventos, sinais digitais indeterminados, atrasos e pontes ADC/DAC entre domínios. A porta AND recebe duas ou mais entradas; outras portas e memórias têm modelos próprios.
- O [manual ngspice, capítulo 5](https://ngspice.sourceforge.io/docs/ngspice-45-manual.pdf) também descreve fontes comportamentais analógicas. Elas permitem equações sobre tensões e correntes, mas uma porta construída assim exigiria definir explicitamente transições, estados indefinidos e conflitos.
- A [documentação XSPICE](https://ngspice.sourceforge.io/xspice.html) lista portas e elementos sequenciais na biblioteca de modelos. Isso não comprova que todos estejam instalados na distribuição final do projeto.
- No runtime de desenvolvimento ngspice 45.2 instalado localmente, `./tools/test-xspice-gates.ps1` confirmou AND, NAND, OR e XOR para as quatro combinações binárias e uma entrada intermediária. A saída DAC alimentou um resistor e um LED genérico; o teste verificou também sua corrente. Os limiares, atrasos e níveis do arquivo são didáticos, não especificações de um CI comercial.

## Comparação e hipóteses

- **XSPICE com pontes explícitas:** reaproveita propagação digital e sinais indefinidos do motor; requer biblioteca de modelos, integração e validação do comportamento analógico das pontes e das saídas.
- **Fontes comportamentais analógicas:** integram-se ao netlist existente, mas exigem um modelo próprio para estados digitais, atrasos e conflitos. Um limiar simples perderia informação sobre entradas indefinidas.
- **Motor digital próprio:** pode oferecer controle didático de eventos e abstrações, mas exigiria sincronização comprovada com o motor analógico, incluindo realimentação e atrasos.

## Recomendação e pendências

Usar XSPICE como candidato inicial para portas mistas, com pontes explícitas e valores documentados, enquanto se avaliam requisitos de abstração digital e distribuição. Antes de apresentar CIs como componentes reais, faltam pinagem, alimentação, comportamento sem energia, impedância e conflitos de saída, testes de temporização, modelos específicos e integração com a bancada. As portas didáticas AND, NAND, OR, XOR e NOT foram integradas à bancada usando essas pontes fixas. Elas não modelam alimentação ou um CI comercial.

## Flip-flop D: comparação (14/11/2026)

**Fatos:** o [manual oficial ngspice 45, seção 8.4.12](https://ngspice.sourceforge.io/docs/ngspice-45-manual.pdf) descreve `d_dff` como elemento de um bit sensível à borda de subida, com condição inicial e entradas assíncronas opcionais. Uma prova local com `d_dff`, pontes ADC/DAC, clock e fonte PWL confirmou retenção após mudança de D e atualização na borda seguinte.

**Hipótese e recomendação:** usar uma abstração didática com D, clock e Q, condição inicial zero e set/reset não expostos; isso permite validar memória básica usando o mesmo motor das portas. Uma alternativa construída com portas pode ser adicionada depois para comparação, mas requer testes próprios de realimentação e inicialização.

**Pendente:** pinagem e alimentação de um CI comercial, saída complementar, set/reset, tempos de setup/hold e comportamento físico não são representados por essa abstração.
