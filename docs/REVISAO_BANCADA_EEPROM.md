# Revisão da bancada EEPROM — 20/11/2026

Versão em desenvolvimento: 0.48.0, branch `feature/eeprom-workbench-usability`.

## Evidências

- Antes da correção, Ctrl+clique não formava a seleção desejada e Ctrl+A no campo numérico não substituía o texto. Testes de seleção conjunta e cliques reais confirmaram a correção.
- Uma peça reduzida podia iniciar um fio ao clicar no corpo, ou escolher o terminal vizinho. O teste falhou antes da prioridade do corpo e da escolha pelo terminal mais próximo; passou depois.
- Redimensionar resistor e DIP switch encaixados conserva as pontas nos furos. Encaixe repetido sem mudança conserva a revisão. Testes `test_leads` e `test_workbench_revision`.
- Arquivo antigo de chip com camadas, formato 35, carrega e salva no formato 36; máscaras antigas são estendidas com zeros. Fixture `legacy-chip-layers-35.elec`.
- EEPROM: pulso, setup/hold, alimentação, leitura, escrita, endereço diferente, sobrescrita, polling, persistência e sessão ngspice real cobertos por testes.
- Mouse e teclado reais no `build/build.exe`, com pasta isolada, gravaram 5A no endereço 2; endereço 3 permaneceu FF. O arquivo salvo foi conferido e instalado no slot pessoal somente após backup com hash.
- Capturas em `build/human-review/captures`: seleção antes/depois, modo de escrita, leitura dos endereços 2/3 e temas claro/escuro. Os 729 pixels mais luminosos dos LEDs de dados apresentaram RGB iguais entre temas.

## Comandos

- `./build.ps1 -GuiTests`: compilação e bateria completa aprovadas, incluindo autoteste gráfico com PATH mínimo.
- `cmake --build build/cmake-release --target bancada-eeprom test_eeprom test_logic_chips test_eeprom_programmer_flow -j 4`
- `ctest --test-dir build/cmake-release -R '^(eeprom|eeprom_programmer_flow)$' --output-on-failure`: 2/2 aprovados.

O instalador e a relocação do pacote 0.48.0 não foram validados nesta revisão. Não houve publicação de release.
Os limites elétricos permanecem em [MODELO_EEPROM.md](MODELO_EEPROM.md).