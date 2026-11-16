# Guia de uso da Bancada

Este guia descreve o programa como ele está na versão 0.34.1. Para instalar, veja o [README](../README.md).

## A tela

- **Topo:** **Projetos** (slots de salvamento), **Exemplos**, **Configurações** e o botão **Simulando/Pausado** da simulação.
- **Esquerda:** o catálogo de peças, em seis abas: **Básicos**, **Digital**, **Chips** (seus chips), **Sensores**, **Saídas** e **Medição**. `Tab` alterna entre as abas, e a roda do mouse rola a lista.
- **Centro:** a bancada.
- **Direita:** o painel da peça selecionada, com valor, tamanho, leitura de corrente e controles próprios (cursor do potenciômetro, ambiente dos sensores, código da placa).
- **Rodapé:** mensagens, dicas da peça selecionada e o tempo simulado.

## Idioma

Abra **Configurações** e clique na bandeira do Brasil ou dos Estados Unidos para escolher
português ou inglês. A escolha fica salva nas preferências do programa. Projetos não mudam o idioma.
Na primeira execução, o programa usa português se o idioma do sistema for português; caso contrário, inglês.
Nomes de chips digitados e código das placas mantêm o conteúdo original. Mensagens de status já exibidas
mudam de idioma quando uma nova ação gera outra mensagem.

## Montar

### Peças

- Clique numa peça do catálogo e depois na bancada para colocá-la.
- Arraste para mover. `R` gira 90°.
- `Delete` apaga a peça e os fios ligados a ela.
- Com uma peça selecionada, escolher outro tipo no catálogo **troca a peça no lugar**, mantendo os fios nos terminais equivalentes.
- **Tamanho:** clique em **Tamanho** no painel (1% a 10000%) ou use `Ctrl+roda`. É só visual: não muda o valor elétrico.
- Passe o mouse sobre um terminal para ver o nome e a função dele: IN/OUT, V+/GND, A/K (anodo/catodo) etc.

### Fios

- Clique num terminal (bolinha branca) e depois em outro para ligar.
- Cliques em pontos vazios no caminho criam **dobras** (até 16 por fio).
- Dois cliques num trecho do fio inserem uma dobra. Arraste a dobra para ajustar e dê dois cliques nela para removê-la.
- Clique no fio para selecioná-lo. `Delete` apaga. A cor dos próximos fios, ou do fio selecionado, é escolhida em **Configurações**. A cor é só visual.

### Breadboard e contato direto

- A breadboard tem duas metades de 30 colunas de 5 contatos ligados entre si, mais quatro trilhos de alimentação independentes.
- Solte uma peça perto dos furos e os terminais encaixam. Terminais encaixados entram na conexão, sem fio.
- Um **terminal encostado em outro terminal** também conecta: ao arrastar, um terminal a até 10 px de outro encaixa nele.
- **Perninhas flexíveis:** segure `Alt` e arraste a bolinha de um terminal para mover só a perna da peça. `Alt` + dois cliques na perna cria uma dobra.
- `X` liga a **transparência**: as peças ficam translúcidas e os terminais escondidos atrás delas podem ser clicados.

### Valores

- Selecione a peça e pressione `E`, ou clique em **Valor** no painel. Aceita ponto ou vírgula e notação científica (`2.2e-6` = 2,2 µF).
- `+` e `-` dobram ou reduzem o valor à metade.
- **Potenciômetro:** `Shift+roda` move o cursor.
- **Sensores:** o controle de ambiente no painel (luz, temperatura, força, distância…) muda o comportamento elétrico. `Shift+roda` também ajusta.
- **Ligar/desligar:** dois cliques em fontes, interruptores, clocks, DPST e gerador. O **botão** fica fechado enquanto você o segura com o mouse. Na chave DIP e no relé, dois cliques sobre a alavanca. No multímetro e no gerador, dois cliques trocam o modo.

### Seleção, cópia e histórico

- Arraste numa área vazia (ou `Shift`+arraste) para selecionar várias peças.
- `Ctrl+A` seleciona tudo. `Ctrl+C`, `Ctrl+X` e `Ctrl+V` copiam, recortam e colam perto do mouse.
- `Ctrl+Z` e `Ctrl+Y` desfazem e refazem (24 passos por sessão). Desfazer muda a montagem, mas não volta o tempo da simulação.
- `Esc` cancela o fio em andamento e limpa a seleção.

### Vista

- A roda do mouse dá zoom ao redor do cursor. Arraste com o botão do meio para mover a vista.
- `Home` volta à vista inicial.

## Simular

A simulação é **automática e contínua**: enquanto ela está executando, cada mudança no circuito é recalculada pelo ngspice num processo separado, e a tela continua respondendo.

- `Espaço` (ou o botão no topo) pausa e continua.
- `Ctrl+Espaço` **reinicia o estado elétrico**: descarrega capacitores, zera flip-flops e reinicia as placas.
- `P` força um novo cálculo.
- Com a simulação pausada, `N` avança um passo.

O que aparece na tela:

- **LEDs e lâmpadas** brilham conforme a corrente calculada. Cinza significa que não há resultado válido.
- **Corrente nos fios** (ligue em **Configurações**): bolinhas amarelas andam no sentido convencional da corrente, e a velocidade indica a faixa de intensidade.
- **Painel da peça:** corrente, tensão e leitura dos instrumentos.
- **Queima:** passar de cerca de 30 mA num LED ou 0,25 W num resistor marca a peça como queimada. É um aviso didático e reversível, não um modelo térmico.

### Gráficos (sondagem)

- Selecione uma peça ou um fio e pressione `T` para calcular um gráfico ao longo do tempo: tensão num terminal ou corrente na peça ou no fio.
- Em **Configurações**, escolha tensão ou corrente, o terminal, o passo e a duração (padrão 10 µs e 5 ms).
- `Espaço` reproduz a curva. `N` e `B` avançam e voltam amostra a amostra, `G` esconde o gráfico, e clicar no gráfico escolhe uma amostra.
- O gráfico é uma análise separada, feita a partir do estado inicial. Ele não altera a simulação contínua.

## Chips (blocos)

1. Selecione as peças e pressione `Ctrl+G`. Elas viram um **chip** compacto, com terminais P1, P2…, e o chip vai para a aba **Blocos**.
2. Na aba **Blocos**, clique no chip e depois na bancada para colocar uma cópia independente.
3. Selecione um chip e pressione `Enter` (ou **Ver interior**) para abrir o **editor isolado**. Ele tem bancada, simulação e histórico próprios, e nada do que você fizer lá altera a bancada externa até você aplicar.
   - `F4` ou **Renomear**: muda o nome (aceita acentos).
   - `S` ou **Salvar modelo**: atualiza o chip na biblioteca. As cópias de um mesmo chip atualizam a mesma entrada; não aparece uma entrada nova.
   - `Ctrl+Enter` ou **Aplicar ao chip**: troca só esta cópia na bancada (`Ctrl+Z` desfaz).
   - `Esc` ou **Voltar**: sai, e pergunta se quer salvar quando houver mudanças.
4. `Ctrl+Shift+G` desfaz o chip na bancada e devolve as peças soltas.
5. O **×** no cartão da aba Blocos remove o chip da biblioteca (pede confirmação). O arquivo vai para `blocks/lixeira`, e as cópias na bancada continuam.

Um chip mostra só um terminal por nó elétrico que fica livre ou que se liga ao exterior. Por exemplo, três AND formando uma AND de 4 entradas viram um chip de 5 terminais. Displays de 7 segmentos e LEDs RGB dentro de um chip aparecem no corpo do chip.

## Microcontroladores

- Na aba **Digital** estão **Arduino Uno**, **ESP32** e **ATtiny85**. O ATtiny precisa de VCC e GND externos.
- Dois cliques na placa (ou **Editar código** no painel) abrem o editor à direita, com números de linha, indentação automática e sugestões (`Ctrl+Espaço`).
- `Ctrl+Enter` ou **Executar** envia o programa e reinicia a placa. Erros de compilação aparecem marcados na linha.
- A linguagem é um subconjunto do Arduino: `int`, `float`, `bool`, vetores, funções, `if`/`for`/`while`, `pinMode`, `digitalWrite`/`digitalRead`, `analogRead`/`analogWrite` (PWM como tensão média), `delay`, `millis`, `map`, `random`, `Serial.print`/`println` e `LiquidCrystal_I2C lcd(0x27,16,2)` com `init`, `backlight`, `clear`, `setCursor` e `print`.
- O **LCD 16x2 I2C** mostra o texto quando tem VCC e GND e quando SDA e SCL estão ligados aos pinos I2C da placa (Uno: A4/A5; ESP32: 21/22). O barramento não é simulado bit a bit.
- Um pino em `INPUT` sem nada ligado fica indefinido num Arduino real; na Bancada ele tende a ler 1. Use um resistor de pull-down ou `INPUT_PULLUP`, como faria no circuito real.

## Projetos, exemplos e configurações

- **Projetos** (ou `F3`): 9 slots. Escolha um e use **Salvar**/**Carregar**, ou `Ctrl+S`/`Ctrl+L`. Carregar um arquivo inválido não apaga a montagem aberta. Os slots guardam a montagem e as configurações, não o instante da simulação.
- **Exemplos** (ou `F2` para o próximo): 18 circuitos prontos, com título e explicação. `Ctrl+Z` volta à montagem anterior.
- **Configurações:** tema claro/escuro, tamanho do texto, grade (espaçamento, mostrar, encaixar), encaixe na breadboard, cor dos fios, curvas e alças dos fios, nomes dos terminais, corrente nos fios e parâmetros do gráfico. As preferências são salvas automaticamente em `saves/preferences.txt`.

## Todos os atalhos

| Tecla | Ação |
|---|---|
| `Espaço` | Pausar / continuar (no gráfico: reproduzir) |
| `Ctrl+Espaço` | Reiniciar o estado elétrico (fora do editor de código) |
| `P` | Recalcular agora |
| `N` / `B` | Passo à frente / atrás (pausado ou no gráfico) |
| `T` | Gráfico da peça ou do fio selecionado |
| `G` | Mostrar/esconder o gráfico |
| `R` | Girar a peça |
| `E` | Editar o valor |
| `+` / `-` | Dobrar / reduzir à metade o valor |
| `Delete` | Apagar a seleção |
| `Ctrl+Z` / `Ctrl+Y` | Desfazer / refazer |
| `Ctrl+A`, `Ctrl+C`, `Ctrl+X`, `Ctrl+V` | Selecionar tudo, copiar, recortar, colar |
| `Ctrl+S` / `Ctrl+L` | Salvar / carregar o slot escolhido |
| `Ctrl+G` / `Ctrl+Shift+G` | Criar chip / desfazer chip |
| `Enter` | Abrir o chip selecionado |
| `F4` | Renomear o chip |
| `X` | Transparência das peças |
| `Tab` | Próxima aba do catálogo |
| `Home` | Vista inicial |
| `Esc` | Cancelar / limpar a seleção / fechar diálogo |
| `F1` / `F2` / `F3` | Configurações / próximo exemplo / projetos |
| `Ctrl+roda` | Tamanho da peça (no chip: largura; com `Shift`: altura) |
| `Shift+roda` | Cursor do potenciômetro / ambiente do sensor |
| `Alt`+arrastar | Mover só a perna (terminal) ou suspender o encaixe na grade |

No editor de chip: `S` salva o modelo, `Ctrl+Enter` aplica e `Esc` volta.

## Limites do modelo elétrico

- As peças usam parâmetros genéricos: o LED tem 2 Ω em série, e transistores, sensores e baterias seguem curvas típicas. Detalhes de cada modelo em [DECISOES.md](DECISOES.md).
- As **portas lógicas** são blocos didáticos sem pinos de alimentação: 0 e 5 V, entrada baixa abaixo de 1,5 V e alta acima de 3,5 V. Uma entrada desligada lê 0.
- Os **CIs 74HC** simulam quatro portas alimentadas entre 4,5 e 5,5 V, com saídas de 50 Ω. Sem alimentação, as saídas ficam desligadas. Pinagem do 74HC00: 1A=1, 1B=2, 1Y=3, 2A=4, 2B=5, 2Y=6, GND=7, 3Y=8, 3A=9, 3B=10, 4Y=11, 4A=12, 4B=13, VCC=14 (segundo a [folha de dados da TI](https://www.ti.com/lit/ds/symlink/sn74hc00.pdf)).
- O **tempo simulado** corre bem mais devagar que o real: cerca de 1 ms simulado por segundo na tela. Um RC de 1 kΩ × 1 µF leva de 1 a 3 s para descarregar visivelmente.
- Adicionar ou remover peças ou ligações, ou mudar a frequência de um clock, reinicia a simulação contínua. O rodapé avisa quando isso acontece.
- Capacidade: 64 peças, 128 fios, 9 slots e 8 chips. Chips não podem conter outros chips.
