# Guia de uso da Bancada

Este guia descreve o programa como ele está na versão 0.45.0. Para instalar, veja o [README](../README.md).

## A tela

- **Topo:** **Projetos** (slots de salvamento), **Exemplos**, **Configurações** e o botão **Simulando/Pausado** da simulação.
- **Esquerda:** o catálogo de peças, em seis abas: **Básicos**, **Digital**, **Chips** (seus chips), **Sensores**, **Saídas** e **Medição**. `Ctrl+Tab` alterna entre as abas, e a roda do mouse rola a lista.
- **Busca:** clique em **Buscar peças** ou use `Ctrl+F`. A busca procura nomes e identificadores do catálogo padrão. Aceita maiúsculas e nomes sem acentos. **×** limpa a busca e volta à categoria escolhida.
- **Barra da bancada:** **Desfazer**, **Refazer**, **Um passo**, **Criar chip**, **Ao vivo** e **Curva completa**. Ações indisponíveis ficam desabilitadas.
- **Centro:** a bancada.
- **Direita:** o painel da peça selecionada, com valor, tamanho, leitura de corrente e controles próprios (cursor do potenciômetro, ambiente dos sensores, código da placa).
  O botão **–** no canto do painel o recolhe numa etiqueta pequena com o nome da peça; clique na etiqueta para abri-lo.
  O painel continua recolhido ao trocar de peça.
- **Rodapé:** mensagens, dicas da peça selecionada e o tempo simulado.

## Idioma

Abra **Configurações** e clique em **Português** ou **English** para escolher o idioma.
A escolha fica salva nas preferências do programa. Projetos não mudam o idioma.
Na primeira execução, o programa usa português se o idioma do sistema for português; caso contrário, inglês.
Nomes de chips digitados e código das placas mantêm o conteúdo original. Mensagens de status já exibidas
mudam de idioma quando uma nova ação gera outra mensagem.

## Montar

### Teclado e escala

- `Tab` e `Shift+Tab` percorrem os controles visíveis. O contorno mostra o foco; `Enter` ou `Espaço` ativa.
- Nos diálogos, o foco permanece nas ações daquele diálogo. `Esc` cancela ou fecha; não confirma substituição de arquivo.
- No editor de código, `Tab` indenta e `Ctrl+Tab` percorre os controles. Clique no texto para voltar a digitar.
- **Configurações → Escala da interface** amplia texto e alvos juntos (80% a 133%). A preferência é salva.
  Ela define o tamanho solicitado para a janela. Ao redimensionar manualmente, a ampliação acompanha a área disponível.
  A janela mínima tem área cliente de 1024×576. A escala solicitada precisa caber na área útil do monitor;
  caso contrário, o programa informa o motivo e conserva a preferência anterior.
- Redimensionar preserva a proporção de desenho e dos cliques, com margens quando necessário.
  Isso não oferece suporte automático a leitor de tela; essa integração continua pendente.

### Peças

- Clique numa peça do catálogo e depois na bancada para colocá-la.
- Arraste para mover. `R` gira 90°.
- `M` (ou **Espelhar**, no painel da peça) espelha a peça da esquerda para a direita: os terminais trocam de
  lado e os fios acompanham. Mostradores e textos continuam legíveis: o visor da fonte, o display de
  7 segmentos, o LCD e as marcas das baterias não ficam invertidos. Como na rotação, a breadboard e os
  instrumentos de bancada não espelham. Chips também espelham (os terminais passam para o outro lado).
- `Delete` apaga a peça e os fios ligados a ela.
- Com uma peça selecionada, escolher outro tipo no catálogo **troca a peça no lugar**, mantendo os fios nos terminais equivalentes.
- **Tamanho:** clique em **Tamanho** no painel (1% a 10000%) ou use `Ctrl+roda`. É só visual: não muda o valor elétrico.
- Passe o mouse sobre um terminal para ver o nome e a função dele: IN/OUT, V+/GND, A/K (anodo/catodo) etc.

### Fios

- Clique num terminal (bolinha branca) e depois em outro para ligar.
- Cliques em pontos vazios no caminho criam **dobras** (até 16 por fio).
- Com o encaixe na grade ligado, novas dobras seguem a grade. Ao criar ou arrastar uma dobra perto do eixo horizontal ou vertical do ponto anterior, ela se alinha exatamente para deixar o trecho reto.
- Segure `Shift` ao arrastar uma dobra para manter o trecho anterior horizontal ou vertical, mesmo longe do eixo de encaixe.
- A seleção por área inclui peças e dobras visíveis. Arraste uma peça ou dobra selecionada para mover o bloco junto; as dobras selecionadas ficam destacadas.
- Dois cliques num trecho do fio inserem uma dobra. Arraste a dobra para ajustar e dê dois cliques nela para removê-la.
- **Fio em fio (junção):** com um fio em andamento, clique em cima de outro fio para terminar nele; ali aparece
  um ponto de junção, e tudo que chega ao ponto fica no mesmo nó. `Alt`+clique num fio cria a junção e já
  começa um fio a partir dela. `Alt`+arraste uma junção para movê-la. Apagar o último fio de uma junção a remove.
- Clique no fio para selecioná-lo: o **painel do fio** abre no canto superior direito, com de onde ele sai e
  aonde chega, a corrente (com **Corrente nos fios** ligada) e as cores. `Delete` apaga.
- Fios e LEDs têm as mesmas 10 cores: vermelho, verde, azul, amarelo, branco, laranja, roxo, rosa, ciano e
  verde-limão. Os fios têm também o preto, que não existe em LED. Fios novos usam a última cor escolhida.
  A cor do fio é só visual; a do LED também muda a tensão direta (veja [DECISOES.md](DECISOES.md)).

### Breadboard e contato direto

- A breadboard tem duas metades de 30 colunas de 5 contatos ligados entre si, mais quatro trilhos de alimentação independentes.
- Solte uma peça perto dos furos e os terminais encaixam. Terminais encaixados entram na conexão, sem fio.
- Para posicionar uma peça livremente perto de outros terminais, segure `Alt` durante o arraste; isso suspende o encaixe na grade, na breadboard e entre terminais.
- Para alinhar o centro da peça à grade mesmo perto de furos ou terminais, segure `Ctrl` durante o arraste; a grade deve estar com o encaixe ligado.
- Um **terminal encostado em outro terminal** também conecta. Isso vale para todos os tipos,
  inclusive furos da breadboard, interruptores e portas de chips personalizados.
  Ao arrastar, um terminal a até 10 px de outro encaixa nele. Afastar as peças desfaz o contato.
- **Perninhas flexíveis:** segure `Alt` e arraste a bolinha de um terminal para mover só a perna da peça. `Alt` + dois cliques na perna cria uma dobra.
- `X` liga a **transparência**: as peças ficam translúcidas e os terminais escondidos atrás delas podem ser clicados.

### Valores

- Selecione a peça e pressione `E`, ou clique em **Valor** no painel. Aceita ponto ou vírgula e notação científica (`2.2e-6` = 2,2 µF).
- Use **Confirmar**/**Cancelar**, ou `Enter`/`Esc`. Entrada inválida permanece no campo, com a causa exibida junto dele.
- `+` e `-` dobram ou reduzem o valor à metade.
- **Potenciômetro:** `Shift+roda` move o cursor.
- **Sensores:** o controle de ambiente no painel (luz, temperatura, força, distância…) muda o comportamento elétrico. `Shift+roda` também ajusta.
- **Ligar/desligar:** dois cliques em fontes, interruptores, clocks e DPST.
  O **botão** fica fechado enquanto você o segura com o mouse.
  Na chave DIP, dê dois cliques na alavanca escolhida. O relé comuta pela corrente na bobina.
  No multímetro e no gerador, dois cliques trocam o modo.

### Seleção, cópia e histórico

- Arraste numa área vazia (ou `Shift`+arraste) para selecionar várias peças. Levando o mouse até a borda
  da bancada durante o arraste, a vista rola sozinha, e dá para alcançar peças fora da tela.
- `Ctrl+A` seleciona tudo. `Ctrl+C`, `Ctrl+X` e `Ctrl+V` copiam, recortam e colam perto do mouse.
- `Ctrl+Z` e `Ctrl+Y` desfazem e refazem (24 passos por sessão). Desfazer muda a montagem, mas não volta o tempo da simulação.
- `Esc` cancela o fio em andamento e limpa a seleção.

### Vista

- A roda do mouse dá zoom ao redor do cursor (de 12% a 400%). Arraste com o botão do meio para mover a vista.
- Abaixo de 100%, as bolinhas dos terminais encolhem junto com as peças. Abaixo de 60%, os nomes das portas
  lógicas e os números dos terminais dos chips somem para não cobrir o circuito; voltam ao aproximar.
- `Home` volta à vista inicial.

## Simular

A simulação é **automática e contínua**. O ngspice roda num processo separado, sem bloquear a interface.
Alterar controles e valores atualiza o circuito. Mudar conexões pode reiniciar o estado elétrico;
os limites estão no fim deste guia.

- `Espaço` (ou o botão no topo) pausa e continua.
- `Ctrl+Espaço` **reinicia o estado elétrico**: descarrega capacitores, zera flip-flops e reinicia as placas.
- `P` força um novo cálculo.
- Com a simulação pausada, `N` avança um passo.

O que aparece na tela:

- **LEDs e lâmpadas** brilham conforme a corrente calculada. O LED apagado fica escuro, bem diferente do aceso. Cinza significa que não há resultado válido.
- **Corrente nos fios** (ligue em **Configurações**): bolinhas amarelas andam no sentido convencional da corrente, e a velocidade indica a faixa de intensidade.
- Durante a sessão contínua, a barra inferior mostra o tempo simulado e a velocidade observada (`1,00x` significa 1 segundo simulado por segundo real). O valor depende do circuito e do computador.
- Em **Configurações → Ritmo da simulação**, use **−/+** para escolher 0,05×, 0,1×, 0,25×, 0,5× ou 1×.
  É a razão solicitada entre tempo simulado e tempo real na sessão contínua; não muda a frequência
  dos componentes nem o passo elétrico. A barra mostra o ritmo escolhido e o observado. Se o computador
  não acompanhar, o observado pode ficar abaixo do escolhido. A preferência é salva automaticamente.
- A sessão contínua escolhe o próprio passo máximo: 1 ms, ou 1/32 do período da senoide mais
  rápida do gerador de funções. Bordas de clock continuam calculadas com precisão. O passo das
  Configurações não afeta a velocidade da simulação ao vivo.
- Se um lote demorar mais de 10 s, a mensagem de erro aparece e o lote seguinte reinicia a sessão
  em t = 0. Clocks de kHz com muitas portas podem continuar bem abaixo de 1×.
- **Painel da peça:** corrente, tensão e leitura dos instrumentos.
- **Queima:** passar de cerca de 30 mA num LED ou 0,25 W num resistor marca a peça como queimada. É um aviso didático e reversível, não um modelo térmico.

### Gráficos (sondagem)

- Selecione uma peça ou um fio e pressione `T` para abrir o **gráfico ao vivo**: tensão num
  terminal ou corrente na peça ou no fio, enquanto a simulação continua rodando. O gráfico mostra
  os últimos segundos definidos pela **duração** das Configurações e se preenche a partir de t = 0.
- `Shift+T` calcula a **curva completa** de 0 até a duração, com o **passo** das Configurações
  (padrão 10 µs e 5 ms). A simulação contínua pausa enquanto essa curva é exibida.
- Na curva completa, `Espaço` reproduz, `N` e `B` avançam e voltam amostra a amostra, `G` esconde o
  gráfico e clicar no gráfico escolhe uma amostra.
- O **X** no canto fecha o gráfico. Fechar a curva completa retoma a simulação contínua.
- Mudar o número de peças ou de fios fecha o gráfico ao vivo; aperte `T` de novo.
- Um ponto sem leitura (terra, ou peça sem corrente medida) mostra um aviso em vez de uma linha.
- No gráfico ao vivo de uma peça, os controles acima do gráfico trocam a grandeza ou o terminal
  da peça que está sendo medida, mesmo se outra peça estiver selecionada. Trocar reinicia a série de amostras.
  No fio, a grandeza é corrente. **Configurações** dá acesso aos parâmetros da curva completa.

## Chips

**Terminais escolhidos:** na aba Digital, coloque uma **Junção de fios**, conecte-a ao nó desejado
e dê dois cliques no ponto para nomeá-lo. Junções nomeadas soltas no interior definem os terminais
do próximo chip: por exemplo J, K, CLOCK, Q e Q_bar. Um CLOCK pode ligar várias entradas internas.
Quando existem esses pontos nomeados, os demais nós livres não viram terminais automaticamente.
Ligações que já chegam de fora continuam expostas para preservar o circuito. Chips internos mantêm
seus próprios terminais. Para remover um terminal escolhido, apague o nome do ponto.

1. Selecione as peças e pressione `Ctrl+G`. Elas viram um **chip** compacto e vão para a aba **Chips**.
   A seleção também pode conter chips: eles ficam preservados dentro do novo chip, em vários níveis.
2. Na aba **Chips**, clique no chip e depois na bancada para colocar uma cópia independente.
   Selecione o chip e pressione `R` para girar 90°. As portas e os fios acompanham o corpo.
   A orientação fica salva no projeto e pode ser desfeita. Girar não muda a montagem interna.
3. Selecione um chip e pressione `Enter` (ou **Ver interior**) para abrir o **editor isolado**.
   Ele tem bancada, simulação e histórico próprios. Alterações no circuito só chegam à bancada ao aplicar.
   Selecione um chip interno e use novamente `Enter` ou **Ver interior** para entrar no próximo nível.
   O cabeçalho mostra o caminho. **Voltar** retorna um nível; **Aplicar ao chip** altera apenas o nível acima.
   - **Renomear:** este botão, dentro do chip, é o único lugar para mudar o nome. Digite e confirme com `Enter`.
     O nome muda na bancada e na entrada existente da biblioteca. Aceita acentos.
     Sair ou descartar o rascunho não apaga um nome confirmado. `Esc` cancela a digitação antes de confirmar.
   - `S` ou **Salvar modelo**: atualiza o chip na biblioteca. As cópias de um mesmo chip atualizam a mesma entrada; não aparece uma entrada nova.
   - `Ctrl+Enter` ou **Aplicar ao chip**: troca só esta cópia na bancada (`Ctrl+Z` desfaz).
   - `Esc` ou **Voltar**: sai, e pergunta se quer salvar quando houver mudanças.
4. **Nomear terminais:** dê dois cliques num terminal do chip e digite o nome (até 15 bytes; aceita acentos).
   `Enter` confirma, nome vazio remove e `Ctrl+Z` desfaz. O nome aparece dentro do corpo, junto ao terminal,
   e o nome do chip fica centralizado entre os nomes dos dois lados. Os nomes ficam salvos no projeto e no modelo da biblioteca.
5. **Reorganizar terminais:** segure `Alt` e arraste um terminal do chip para o outro lado ou para outra
   posição do mesmo lado. Os fios acompanham, e `Ctrl+Z` desfaz. Por exemplo, num 3AND feito com duas AND,
   leve a terceira entrada para a esquerda: ficam as 3 entradas à esquerda e a saída à direita.
   O arranjo fica salvo no projeto e no modelo da biblioteca. Até 64 terminais por chip.
6. `Ctrl+Shift+G` desagrupa somente o nível selecionado. Os chips internos continuam encapsulados.
7. O **×** no cartão da aba Chips remove o chip da biblioteca (pede confirmação). O arquivo vai para `blocks/lixeira`, e as cópias na bancada continuam.

Um chip mostra só um terminal por nó elétrico que fica livre ou que se liga ao exterior. Por exemplo, três AND formando uma AND de 4 entradas viram um chip de 5 terminais. Displays de 7 segmentos e LEDs RGB dentro de um chip aparecem no corpo do chip.

## Microcontroladores

- Na aba **Digital** estão **Arduino Uno**, **ESP32** e **ATtiny85**. O ATtiny precisa de VCC e GND externos.
- Dois cliques na placa (ou **Editar código** no painel) abrem o editor à direita, com números de linha, indentação automática e sugestões (`Ctrl+Espaço`).
- `Ctrl+Enter` ou **Executar** envia o programa e reinicia a placa. Erros de compilação aparecem marcados na linha.
- O editor aceita até 16.383 bytes após expandir tabulações em quatro espaços e retirar `CR`.
  Se o programa não couber, o editor avisa e mantém o original sem cortes. Pausar a simulação também pausa a placa.
- A linguagem é um subconjunto do Arduino: `int`, `float`, `bool`, vetores, funções, `if`/`for`/`while`, `pinMode`, `digitalWrite`/`digitalRead`, `analogRead`/`analogWrite` (PWM como tensão média), `delay`, `millis`, `map`, `random`, `Serial.print`/`println` e `LiquidCrystal_I2C lcd(0x27,16,2)` com `init`, `backlight`, `clear`, `setCursor` e `print`.
- O **LCD 16x2 I2C** mostra o texto quando tem VCC e GND e quando SDA e SCL estão ligados aos pinos I2C da placa (Uno: A4/A5; ESP32: 21/22). O barramento não é simulado bit a bit.
- Um pino em `INPUT` sem nada ligado fica indefinido num Arduino real; na Bancada ele tende a ler 1. Use um resistor de pull-down ou `INPUT_PULLUP`, como faria no circuito real.

## Projetos, exemplos e configurações

- **Projetos** (ou `F3`): slots que crescem conforme os projetos são salvos, em páginas de seis cartões com miniaturas e resumo de peças/fios. Ao carregar, a vista tenta enquadrar o circuito inteiro. Escolha um e use **Salvar**/**Carregar**, ou `Ctrl+S`/`Ctrl+L`. Em slot ocupado, **Substituir** pede confirmação; cancelar conserva o arquivo salvo. Carregar um arquivo inválido não apaga a montagem aberta. Os slots guardam a montagem e as configurações, não o instante da simulação.
- **Exemplos** (ou `F2`): 18 circuitos prontos, com título e explicação. `Ctrl+F2` avança diretamente para o próximo. `Ctrl+Z` volta à montagem anterior.
- **Configurações:** tema claro/escuro, escala da interface, grade (espaçamento, mostrar, encaixar), encaixe na breadboard, curvas e alças dos fios, nomes dos terminais, simulação e parâmetros do gráfico. A cor fica no painel do fio. As preferências são salvas automaticamente em `saves/preferences.txt`.

### Onde ficam os arquivos

| Forma de uso | Pasta dos dados |
|---|---|
| Instalador | `%APPDATA%/Bancada/Bancada` |
| ZIP portátil | Ao lado de `bancada.exe` |
| Build de desenvolvimento, sem marcador de distribuição | Diretório de onde o programa foi aberto |

Dentro dessa pasta, `saves/` guarda os projetos `slot-N.elec`, suas miniaturas `slot-N.bmp` e `preferences.txt`.
`blocks/` guarda os oito chips da biblioteca; os removidos vão para `blocks/lixeira`.
Desinstalar preserva os dados. Não há migração automática entre essas pastas.
Para mover projetos antigos, feche o programa e copie `saves/` e `blocks/` para o destino escolhido.

### Instrumentos

- **Multímetro:** conecte os terminais da peça ao circuito. No modo corrente, use uma ligação em série.
  A medição de tensão tem entrada de 10 MΩ. No modo Ω, a corrente de teste é de até 0,5 mA e a tensão
  de teste é limitada a cerca de 2 V. Desligue as fontes e descarregue capacitores antes de medir resistência;
  com uma fonte ativa, a interface indica leitura indisponível. A corrente de teste ainda pode alterar a
  carga de capacitores, inclusive depois de voltar ao modo V.
- **Osciloscópio:** mede tensão entre seus terminais, com entrada de 1 MΩ. O gráfico usa a sondagem por `T`.
- **Gerador de função:** oferece seno, onda quadrada e triangular. A frequência é ajustável;
  a saída usa níveis de 0 a 5 V e resistência de 50 Ω.

Ainda faltam pontas independentes posicionáveis, osciloscópio de vários canais e medidor dedicado de clock/pulso.

## Todos os atalhos

| Tecla | Ação |
|---|---|
| `Espaço` | Pausar / continuar (no gráfico: reproduzir) |
| `Ctrl+Espaço` | Reiniciar o estado elétrico (fora do editor de código) |
| `P` | Recalcular agora |
| `N` | Avançar com a simulação pausada; no gráfico, próxima amostra |
| `B` | Amostra anterior do gráfico, com reprodução pausada |
| `T` | Gráfico ao vivo da peça ou do fio selecionado |
| `Shift+T` | Curva completa desde t = 0 (pausa a simulação) |
| `G` | Mostrar/esconder o gráfico |
| `R` | Girar a peça |
| `M` | Espelhar a peça (troca os terminais de lado) |
| `E` | Editar o valor |
| `+` / `-` | Dobrar / reduzir à metade o valor |
| `Delete` | Apagar a seleção |
| `Ctrl+Z` / `Ctrl+Y` | Desfazer / refazer |
| `Ctrl+A`, `Ctrl+C`, `Ctrl+X`, `Ctrl+V` | Selecionar tudo, copiar, recortar, colar |
| `Ctrl+S` / `Ctrl+L` | Salvar / carregar o slot escolhido |
| `Ctrl+G` / `Ctrl+Shift+G` | Criar chip / desagrupar chip |
| `Enter` | Abrir o chip selecionado |
| `X` | Transparência das peças |
| `Home` | Vista inicial |
| `Esc` | Cancelar / limpar a seleção / fechar diálogo |
| `F1` / `F2` / `F3` | Configurações / exemplos / projetos |
| `Ctrl+F2` | Próximo exemplo diretamente |
| `Ctrl+F` | Buscar peças |
| `Tab` / `Shift+Tab` | Próximo controle / controle anterior |
| `Ctrl+Tab` | Próxima categoria; no editor de código, percorrer controles |
| `Ctrl+roda` | Tamanho da peça (no chip: largura; com `Shift`: altura) |
| `Shift+roda` | Cursor do potenciômetro / ambiente do sensor |
| `Alt`+arrastar | Mover só a perna (terminal), reorganizar terminal de chip ou suspender o encaixe na grade |

No editor de chip: `S` salva o modelo, `Ctrl+Enter` aplica e `Esc` volta.

## Limites do modelo elétrico

- As peças usam parâmetros genéricos: o LED tem 2 Ω em série, e transistores, sensores e baterias seguem curvas típicas. Detalhes de cada modelo em [DECISOES.md](DECISOES.md).
- As **portas lógicas** (AND, OR, NOT…) seguem a família 74HC com **alimentação comum**: não mostram pinos
  VCC/GND, mas são alimentadas pela **primeira fonte da bancada** (positivo = VCC, negativo = terra), como os
  pinos de alimentação ocultos dos programas de esquema. Funcionam de 2 a 6 V; fora disso ficam desligadas.
  Entrada até 30% do VCC lê 0 e a partir de 70% lê 1; entre os dois é indefinida. A saída alta fica no VCC
  da fonte, atrás de cerca de 50 Ω, e a corrente sai da própria fonte. Uma entrada desligada lê 0.
  Exemplo: com fonte de 2 V, entradas em 2 V são 1 e a saída dá 2 V. Um LED ligado direto numa saída
  com 5 V passa do limite: use um resistor de 220 a 330 Ω. Sem fonte, um gerador de clock (módulo de 5 V)
  alimenta as portas.
- Os **CIs 74HC** simulam quatro portas alimentadas pelos próprios pinos VCC e GND, de 2 a 6 V, com os mesmos
  limiares proporcionais e saídas de 50 Ω. Sem alimentação, as saídas ficam desligadas. Pinagem do 74HC00: 1A=1, 1B=2, 1Y=3, 2A=4, 2B=5, 2Y=6, GND=7, 3Y=8, 3A=9, 3B=10, 4Y=11, 4A=12, 4B=13, VCC=14 (segundo a [folha de dados da TI](https://www.ti.com/lit/ds/symlink/sn74hc00.pdf)).
- O **tempo simulado** corre bem mais devagar que o real: cerca de 1 ms simulado por segundo na tela. Um RC de 1 kΩ × 1 µF leva de 1 a 3 s para descarregar visivelmente.
- Adicionar ou remover peças ou ligações, ou mudar a frequência de um clock, reinicia a simulação contínua. O rodapé avisa quando isso acontece.
- Capacidade: 1024 peças elementares (inclusive as que estão dentro dos chips; junções contam como peças), 2048 fios, 1024 grupos, slots de projeto conforme o espaço disponível e 8 modelos na biblioteca. Chips podem conter outros chips; cada instância é independente.
