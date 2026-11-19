# Mapa e revisão da interface — 0.45.0

Revisão em 18/11/2026, com a skill Impeccable. Trabalho autorizado em branch separada da main.
Método: revisão de código e capturas no mesmo contexto, sem subagentes por escolha do responsável.
Não é pesquisa com pessoas nem certificação de acessibilidade. O detector web não se aplica ao desenho SDL/OpenGL.

## Mapa para manutenção

| Superfície | Implementação |
|---|---|
| Bancada e desenho de peças | `src/app_render.c`, `src/workbench_view.c` |
| Geometria, viewport proporcional e transformação do ponteiro | `src/app_layout.c/h` |
| Barra de ações, foco e disponibilidade de controles | `src/app_controls.c/h`, `src/app_input.c` |
| Catálogo e busca | `src/app_palette.c/h`, `src/app_render.c` |
| Inspetores de peça e fio | `src/app_render.c`, `src/app_input.c` |
| Biblioteca e criação de chip | `src/app_packing.c` |
| Interior isolado e confirmação de saída | `src/app_editor.c` |
| Projetos, resumo e confirmação de substituição | `src/app_editing.c` |
| Exemplos e comandos da bancada | `src/app_commands.c`, `src/workbench_examples.c` |
| Configurações e preferências | `src/app_settings.c`, `src/app_preferences.c` |
| Gráficos | `src/app_commands.c`, `src/sim_state.c`, `src/app_render.c` |
| Código e monitor serial | `src/app_code_editor.c`, `src/code_text.c` |
| Textos e idiomas | `src/text/*.def`, `src/ui_text.c`, `src/app_text_utf8.c` |
| Eventos e capturas de desenvolvimento | `src/dev_capture.c`, `src/dev_scenarios.c` |

## Resultado das cinco prioridades

| Prioridade inicial | Entrega | Limite explícito |
|---|---|---|
| P1: proteger e identificar projetos | Resumo de peças/fios; Substituir exige confirmação; Cancelar preserva arquivo; alvo não muda com outra seleção | Não cria nomes livres nem altera formato 34 |
| P1: ações visíveis | Desfazer, Refazer, Um passo, Criar chip, Ao vivo e Curva completa | Mesmos comandos e limites elétricos anteriores |
| P1: teclado e escala | Tab/Shift+Tab, foco visível, Enter/Espaço, diálogos com foco restrito; escala integral e viewport proporcional | Montagem ainda usa mouse; leitor de tela requer integração própria |
| P2: configuração e medição | Simulação separada; explicação completa; escolha de grandeza/terminal junto ao gráfico ao vivo | Fio mede corrente; curva completa usa Configurações |
| P2: encontrar peças e criar chip | Busca por nome/identificador, maiúsculas e acentos portugueses; biblioteca vazia explica a ação Criar chip | Busca cobre catálogo padrão; nomes da biblioteca continuam na aba Chips |

Também entregues: Confirmar/Cancelar por mouse na edição de valores, erro junto ao campo,
idiomas por nome e F2 abrindo a mesma escolha de exemplos que o botão. Ctrl+F2 conserva o avanço direto.
Não foi alterado o comportamento de aplicar o rascunho ao fechar o editor de código.

## Verificação por fluxo humano

- Projetos: abrir, escolher, salvar, acrescentar peça, tentar substituir, cancelar, confirmar e reabrir.
  O arquivo conserva a versão anterior ao cancelar; confirmar atinge somente o slot apresentado.
- Catálogo: buscar Arduino a partir de Básicos, escolher e colocar a placa; limpar volta à categoria.
  Busca sem resultado não coloca uma peça oculta. Busca sem acentos encontra Junção.
- Valores: selecionar resistor, editar texto inválido, confirmar, corrigir, confirmar e desfazer.
  O valor inválido fica no campo, com causa visível; desfazer restaura o valor anterior.
- Chips: criar pela barra com seleção válida; sem seleção, o botão não altera a montagem.
  A suíte existente cobre entrar, renomear, salvar, reabrir e excluir preservando as instâncias.
- Teclado: Tab abre caminho para Projetos sem trocar categoria; Shift+Tab percorre ao contrário;
  Enter/Espaço ativam. Na confirmação de substituição, a primeira ação no percurso é Cancelar.
  Autorrepetição de Enter/Espaço não ativa o botão; o teste falhou antes dessa proteção.
- Gráfico: abrir ao vivo, selecionar outra peça, mudar o terminal do ponto originalmente medido e fechar.
  No gráfico do fio, o controle desabilitado de grandeza não muda a medição para tensão.
- Escala: clicar em controles numa janela 1000×900, mudar escala e repetir clique.
  A transformação inversa mantém o alvo alinhado ao desenho.

Reproduções anteriores à correção:

- `test_app_commands`: o arquivo já continha duas peças quando deveria continuar contendo uma, antes da confirmação.
- `test_app_ui_flow` compilado com `app_input.c` da main: a busca não recebia o texto ARDUINO.
- Durante a revisão, a captura mostrou a miniatura do Uno cobrindo o nome e a busca.
  A escala da miniatura passou a usar os limites do corpo já disponíveis no registro de peças.

Os eventos de teste usam `SDL_zero`, scancode e soltura da tecla. A inicialização por `{0}` do union
deixou campos sem valor definido nesta ferramenta: a instrumentação observou `repeat=127` e falhas intermitentes.
Depois da inicialização explícita, o fluxo passou em 30 execuções consecutivas sem instrumentação.

## Capturas revisadas

Capturas locais em `build/ui-review/after/`, sem dados pessoais. Não são dependência de teste nem fonte versionada.
Foram examinados configurações claras/escuras, escala de 133% (1707×960), confirmação de substituição,
busca, biblioteca vazia, foco, edição de valor, editor de código, interior de chip, exemplos e gráfico ao vivo.
As capturas de catálogo foram refeitas após corrigir a miniatura.

Exemplo para reproduzir depois de `./build.ps1 -GuiTests`:

```powershell
./build/build.exe --capture-search --capture-output=build/search.bmp
./build/build.exe --capture-overwrite --capture-output=build/overwrite.bmp
./build/build.exe --capture-ui-large --capture-output=build/scale.bmp
./build/build.exe --capture-focus --capture-output=build/focus.bmp
./build/build.exe --capture-code --capture-output=build/code.bmp
```

Em tema claro, o destaque tem contraste calculado de 5,87:1 sobre a superfície; em escuro, 7,13:1.
Texto secundário claro: 4,61:1. São cálculos dos tokens usados, não uma certificação de cada pixel ou componente.

## Comandos de validação executados

- `./build.ps1 -GuiTests`: passou com a suíte completa, ngspice, os 18 exemplos e autoteste gráfico com PATH mínimo.
- `cmake -S . -B build/ui-cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug`: configurou o caminho CMake sem novas dependências.
- `cmake --build build/ui-cmake --target bancada test_app_ui_flow --parallel 4`: passou com os avisos tratados como erros.
- `ctest --test-dir build/ui-cmake -R '^app_ui_flow$' --repeat until-fail:30 --output-on-failure`:
  30 execuções passaram após a inicialização explícita dos eventos.
- Após a proteção final contra ativação repetida, `ctest --test-dir build/ui-cmake -R '^(app_ui_flow|gui)$' --output-on-failure`:
  os dois testes passaram. A linha de compilação da aplicação de `build.ps1` foi executada novamente para atualizar `build/build.exe`.

O registro local da suíte completa está em `build/interface-validation.log`.

## Limites preservados

- Modelos elétricos e regras de reinício da simulação permanecem os mesmos.
- Escala solicitada precisa caber no monitor; o programa informa quando não cabe.
- Integração com leitor de tela e validação em vários monitores/DPI não foram realizadas.
- Não há novo toolkit, dependência, pacote, tag ou release neste trabalho.
