# Textos da interface

> **Guia de estilo e glossário** de todos os textos que o programa mostra, em pt-BR e en-US. Baseado nas ideias do ASD-STE100 (Simplified Technical English): uma palavra para cada ideia, frases curtas, sem jargão.

## Onde ficam

Todo texto visível fica em `src/text/*.def`, com os dois idiomas lado a lado:

```c
UI_TEXT(LIBRARY_FULL, "Biblioteca cheia. Remova um chip.", "Library full. Remove a chip.")
```

O código usa `tr(TXT_LIBRARY_FULL)`. Não escreva texto visível direto no código. As exceções são símbolos e unidades (`V`, `mA`, `Hz`), nomes de pinos (`GND`, `1A`), palavras-chave da linguagem do Arduino e textos que só aparecem em capturas ou autotestes de desenvolvimento.

Regras técnicas, verificadas por `tests/test_ui_text.c`:

- Os dois idiomas têm os mesmos especificadores (`%d`, `%s`, `%.3g`…), na mesma ordem.
- Sem espaço no início ou no fim, sem espaço duplo.
- Sem palavras da lista proibida abaixo.
- No máximo 20 palavras por frase.

## Regras de escrita

1. **Diga o que é ou o que faz. Nada mais.** Não explique o óbvio nem repita o que a tela já mostra.
   - Não: "O modelo foi salvo com sucesso na biblioteca de chips." Sim: "Chip salvo."
2. **Uma ideia por frase.** Frases curtas, de preferência com até 12 palavras.
3. **Rótulos e botões:** um verbo ou um substantivo, de 1 a 3 palavras ("Salvar", "Tamanho", "Abrir chip").
4. **Instruções no imperativo**, em voz ativa: "Selecione um chip." / "Select a chip."
5. **Erros e avisos:** o que aconteceu, depois o que fazer. "Arquivo inválido. A montagem não mudou." / "Invalid file. The circuit did not change."
6. **Confirmação só quando o resultado não está visível.** Mover uma peça não precisa de mensagem; salvar um arquivo precisa.
7. **Dicas de teclas:** `Tecla: ação`, separadas por ` · ` ("R: girar · E: valor").
8. **Linguagem neutra:** não fale de "o usuário" nem use adjetivos de gênero sobre quem usa. Prefira o imperativo ("Selecione") ou a forma impessoal ("Não foi possível salvar").
9. **Sem jargão de programação.** Use as palavras do glossário. Termos de eletrônica (tensão, corrente, terminal) são permitidos, porque são o assunto do programa.
10. **Português com acentos**, inglês americano.
11. **Números e unidades:** espaço entre número e unidade ("5 V", "20 mA"); unidades do SI.

## Glossário

Use sempre a mesma palavra para a mesma coisa.

| pt-BR | en-US | Significado |
|---|---|---|
| bancada | bench | Área onde se monta o circuito |
| circuito | circuit | O que está montado na bancada |
| peça | part | Qualquer item do catálogo colocado na bancada |
| catálogo | catalog | Lista de peças à esquerda |
| terminal | terminal | Ponto de ligação de uma peça |
| pino | pin | Terminal numerado de CI, chip ou placa |
| perna | leg | Fio próprio de uma peça (resistor, LED…) que pode ser dobrado |
| fio | wire | Ligação desenhada entre terminais |
| dobra | bend | Ponto de curva de um fio ou perna |
| breadboard | breadboard | Placa de furos |
| contato | contact | Furo da breadboard ou encosto entre terminais |
| chip | chip | Bloco criado com Ctrl+G |
| biblioteca | library | Lista de chips salvos (aba Chips) |
| CI | IC | Circuito integrado do catálogo (74HC00…) |
| placa | board | Microcontrolador (Arduino, ESP32, ATtiny) |
| código | code | Programa da placa |
| projeto | project | Arquivo com circuito e configurações |
| slot | slot | Uma das 9 posições de projeto |
| exemplo | example | Circuito pronto |
| simulação | simulation | Cálculo contínuo do circuito |
| cálculo | calculation | Uma execução do ngspice |
| gráfico | graph | Curva no tempo (tecla T) |
| tensão | voltage | |
| corrente | current | |
| potência | power | |
| alimentação | supply | VCC/GND de CI ou placa |
| terra | ground | |
| entrada / saída | input / output | |
| queimado | burned | Peça acima do limite didático |
| curto-circuito | short circuit | |
| valor | value | Grandeza principal da peça |
| tamanho | size | Escala visual |
| girar | rotate | |
| salvar / carregar | save / load | |
| aplicar | apply | |
| apagar | delete | Tirar da bancada |
| remover | remove | Tirar da biblioteca |
| renomear | rename | |
| desfazer / refazer | undo / redo | |
| pausar / continuar | pause / resume | |
| reiniciar | reset | Voltar ao estado elétrico inicial |
| executar | run | Rodar o código da placa |
| grade | grid | |
| encaixar | snap | |
| transparência | transparency | |
| configurações | settings | |
| idioma | language | |

## Palavras proibidas

Não use na interface (pt e en): `netlist`, `snapshot`, `runtime`, `worker`, `host`, `buffer`, `topologia`/`topology`, `transação`/`transaction`, `sucesso`/`successfully`, `por favor`/`please`, `componente`/`component` (use peça/part), `joelho`/`knee` (use dobra/bend), `bloco`/`block` (use chip), `inválida a operação`, `ocorreu um erro`/`an error occurred` (diga qual erro).
