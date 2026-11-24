# Guia de uso da Bancada

Este guia descreve o programa como ele está na versão 0.45.0. Para instalar, veja o [README](../README.md).

## A tela

- **Topo:** **Projetos** (slots de salvamento), **Exemplos**, **Configurações** e o botão **Simulando/Pausado** da simulação.
- **Esquerda:** o catálogo de peças, em doze abas: **Básicos** (passivos e chaves), **Portas** (portas soltas e CIs de portas), **Salvos** (seus chips), **Sensores**, **Saídas**, **Medição**, **Fontes**, **Placas** (microcontroladores), **Bancada** (protoboards, junções e notas), **Lógica** (flip-flops, contadores, registradores, decodificadores, barramento), **Memória** e **Lineares** (555, amp-op, comparador, regulador e drivers). `Ctrl+Tab` alterna entre as abas, e a roda do mouse rola a lista.
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
- **Configurações → Escala da interface** amplia texto e alvos juntos (80% a 133%), somada à escala do
  Windows. A preferência é salva. Numa janela normal, ela define o tamanho da janela. Maximizada ou em tela
  cheia, a janela não muda: uma escala menor deixa painéis e textos menores e dá mais espaço à bancada.
  A escala precisa caber na janela maximizada ou na área útil do monitor; caso contrário, o programa informa
  o motivo e conserva a preferência anterior.
- Janelas maiores aumentam a bancada e o catálogo mostra mais peças; o cabeçalho, o inspetor e os diálogos
  acompanham as bordas ou o centro. Só uma janela menor que o desenho na escala escolhida ganha margens.
  Isso não oferece suporte automático a leitor de tela; essa integração continua pendente.

### Peças

- Clique numa peça do catálogo e depois na bancada para colocá-la.
- Arraste para mover. `R` gira 90°; `Shift+R` (ou `Shift` + botão direito) gira 45°.
- `M` (ou **Espelhar**, no painel da peça) espelha a peça da esquerda para a direita: os terminais trocam de
  lado e os fios acompanham. Mostradores e textos continuam legíveis: o visor da fonte, o display de
  7 segmentos, o LCD e as marcas das baterias não ficam invertidos. Como na rotação, a breadboard e os
  instrumentos de bancada não espelham. Chips também espelham (os terminais passam para o outro lado).
- `Delete` apaga a peça e os fios ligados a ela.
- Com uma peça selecionada, escolher outro tipo no catálogo **troca a peça no lugar**, mantendo os fios nos terminais equivalentes.
- **Tamanho:** clique em **Tamanho** no painel (1% a 10000%) ou use `Ctrl+roda`. O valor elétrico não muda; terminais conectados preservam a posição ao redimensionar.
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
- O encaixe usa a posição dos terminais e funciona com a grade visual desligada. Ao redimensionar uma peça sobre a breadboard, as pontas próximas aos furos são realinhadas. A rotação de 45° não garante que todos os pinos de um componente rígido caibam na malha retangular; confira os contatos ou use fios.
- O marcador verde indica contato geométrico, não que o circuito esteja fechado nem que exista tensão entre os pinos. Em **Configurações**, desative **Mostrar pinos verdes** para ocultá-lo.
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
- Durante a sessão contínua, a barra inferior mostra o tempo simulado e a velocidade observada (`1x` significa 1 segundo simulado por segundo real). O valor depende do circuito e do computador.
- Em **Configurações → Ritmo da simulação**, use **−/+** para escolher de 0,001× (câmera lenta: 1 ms do
  circuito por segundo) a 1×: 0,001×, 0,002×, 0,005×, 0,01×, 0,02×, 0,05×, 0,1×, 0,25×, 0,5× ou 1×.
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

**Terminais escolhidos:** na aba Bancada, coloque uma **Junção de fios**, conecte-a ao nó desejado
e dê dois cliques no ponto para nomeá-lo. Junções nomeadas soltas no interior definem os terminais
do próximo chip: por exemplo J, K, CLOCK, Q e Q_bar. Um CLOCK pode ligar várias entradas internas.
Quando existem esses pontos nomeados, os demais nós livres não viram terminais automaticamente.
Ligações que já chegam de fora continuam expostas para preservar o circuito. Chips internos mantêm
seus próprios terminais. Para remover um terminal escolhido, apague o nome do ponto.

1. Selecione as peças e pressione `Ctrl+G`. Elas viram um **chip** compacto e vão para a aba **Chips**.
   A seleção também pode conter chips: eles ficam preservados dentro do novo chip, em vários níveis.
2. Na aba **Chips**, clique no chip e depois na bancada para colocar uma cópia independente.
   Selecione o chip e pressione `R` para girar 90° ou `Shift+R` para girar 45°. As portas e os fios acompanham o corpo.
   Clique em **Tamanho** no painel do chip para informar o percentual. `Ctrl+roda` reduz ou amplia largura e altura; `Ctrl+Shift+roda` altera só a altura e `Ctrl+Alt+roda` só a largura. O tamanho mínimo mantém o espaçamento físico entre as portas. Com encaixe na breadboard ligado, a largura ajustada segue passos compatíveis com os furos.
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

## CIs digitais de média escala

Na aba **Digital** estão o contador **74HC161**, o registrador **74HC173**, o transceptor **74HC245**, o
somador **74HC283** e a EEPROM **AT28C16**. Os nomes de pino seguem as folhas de dados; `~` marca pino ativo
em nível baixo (por exemplo `~LOAD`). Cada CI se alimenta pelos próprios pinos VCC e GND, de 2 a 6 V, com os
limiares e as saídas de 50 Ω dos outros CIs 74HC. Saídas de três estados (173, 245, EEPROM) soltam o pino
quando desabilitadas.

- **74HC161**: conta na subida do clock com ENP e ENT altos; `~LOAD` baixo carrega A–D na subida;
  `~CLR` baixo zera na hora. RCO sobe em 15 com ENT alto.
- **74HC173**: carrega 1D–4D na subida do clock com `~G1` e `~G2` baixos; CLR alto zera. Saídas ligadas
  com M e N baixos.
- **74HC245**: com `~OE` baixo, DIR alto leva A para B e DIR baixo leva B para A.
- **74HC283**: soma A1–A4, B1–B4 e C0; saída S1–S4 e C4.
- **AT28C16**: lê com `~CE` e `~OE` baixos e `~WE` alto. O conteúdo é texto hexadecimal guardado na peça,
  por exemplo `00: 1E 2F 3D` (o endereço antes de `:` é opcional). `;` ou `#` começam comentário.
  Endereços não gravados leem `FF`, como uma memória apagada. Com `~OE` alto e `~CE` baixo, um pulso
  baixo em `~WE` grava: o endereço é retido na descida e o dado na subida. CE também pode controlar
  o ciclo. Aguarde 5 ms simulados após ligar e 1 ms para concluir a escrita. Os bytes acompanham o
  salvamento do projeto. Durante a escrita, IO7 permite consultar a conclusão.
  Os pinos IO0–IO7 são bidirecionais: desligue o circuito que fornece os dados antes de ler.
  O modelo funcional de 5 V não representa desgaste, dispersão analógica, identificação nem apagamento
  especial a 12 V; veja [modelo e testes](MODELO_EEPROM.md).

## Microcontroladores

- Na aba **Placas** estão **Arduino Uno**, **ESP32** e **ATtiny85**. O ATtiny precisa de VCC e GND externos.
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
- **Configurações:** tema claro/escuro, escala da interface, grade (espaçamento, mostrar, encaixar), encaixe na breadboard, curvas e alças dos fios, nomes dos terminais, marcadores verdes de contato, simulação e parâmetros do gráfico. A cor fica no painel do fio. As preferências são salvas automaticamente em `saves/preferences.txt`.

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

## Fonte de bancada, proteção e fichas dos CIs

- **Fonte de bancada (CV/CC):** dois botões, como numa fonte de laboratório. Com `Shift`+roda sobre o botão
  **V**, a tensão muda em 0,1 V (0 a 30 V); sobre o botão **A**, o limite de corrente muda em 10 mA (0 a 3 A).
  O controle deslizante do inspetor também ajusta o limite. Abaixo do limite a fonte mantém a tensão (lâmpada
  **CV** verde). Se o circuito pedir mais, a tensão cai e a corrente fica no limite (lâmpada **CC** vermelha).
  Um curto na fonte de bancada não a queima: ela entra em CC. Baterias e clocks não têm limite e ainda queimam.
  Hábito seguro: comece com o limite baixo e aumente só o necessário.
- **Limites dos datasheets:** CIs, portas soltas, displays e placas seguem os máximos absolutos das folhas de
  dados. Acima do máximo a peça mostra um anel âmbar no pino, uma linha vermelha no inspetor e uma mensagem
  no rodapé. Acima de 1,8 vez o máximo de corrente, como num curto ou em duas saídas brigando no mesmo fio,
  ou acima da tensão máxima absoluta, a peça queima e para de funcionar até a ligação mudar. Valores:
  74HC até 25 mA por saída (35 mA em alguns), alimentação recomendada até 6 V e queima acima de 7 V;
  AT28C16 até 6,25 V; Arduino Uno e ATtiny85 com 40 mA por pino e pinos até VCC + 0,5 V; ESP32 não aceita
  5 V nos pinos (máximo 3,6 V); segmentos de display e cores do LED RGB com o limite do LED (30 mA).
- **Entrada solta:** uma entrada de CI que nada define (nem fonte, nem resistor, nem saída) recebe um aviso, com
  as chaves no estado atual. O caso clássico é um botão ligado ao VCC sem resistor de pull-down: solto, a entrada
  fica no ar. A simulação lê 0, mas um CI real lê ao acaso. Portas não usadas de um CI não geram aviso.
- **Ficha do CI:** selecione um CI e clique em **Ficha** no inspetor, ou pressione `I`. Ela mostra o chip
  sozinho e ampliado, com os pinos numerados e nomeados, o que cada pino faz, a faixa de alimentação e os
  máximos absolutos. Passe o mouse num pino, no desenho ou na lista, para ler a explicação completa. `Esc`
  fecha.

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
  limiares proporcionais e saídas de 50 Ω. Sem alimentação, as saídas ficam desligadas. O 74HC161, o 74HC173,
  o 74HC245, o 74HC283 e a AT28C16 seguem a mesma regra, com lógica interna digital e atraso de 1 ns por porta. Pinagem do 74HC00: 1A=1, 1B=2, 1Y=3, 2A=4, 2B=5, 2Y=6, GND=7, 3Y=8, 3A=9, 3B=10, 4Y=11, 4A=12, 4B=13, VCC=14 (segundo a [folha de dados da TI](https://www.ti.com/lit/ds/symlink/sn74hc00.pdf)).
- Os **CIs lineares** são modelos ideais dos datasheets da TI, sem banda, slew rate nem aquecimento:
  **NE555** (limiares em 2/3 e 1/3 de VCC ou no pino CONT; saída alta 1,7 V abaixo de VCC; astável com
  f = 1,44/((RA + 2RB)·C)); **LM358** (ganho 10⁵, saída de 0 V a V+ − 1,5 V); **LM393** (saída em coletor
  aberto, use pull-up); **LM7805** (5 V com pelo menos 7 V na entrada, até 2,1 A, não absorve corrente);
  **ULN2003A** (chaves Darlington com cerca de 1 V de saturação e diodos para COM); **L293D** (meias-pontes
  com quedas de 1,4 V e 1,2 V, desligadas com EN baixo). Nesses CIs só a alimentação é protegida; a corrente
  das saídas não é verificada.
- O **tempo simulado** corre bem mais devagar que o real: cerca de 1 ms simulado por segundo na tela. Um RC de 1 kΩ × 1 µF leva de 1 a 3 s para descarregar visivelmente.
- Adicionar ou remover peças ou ligações, ou mudar a frequência de um clock, reinicia a simulação contínua. O rodapé avisa quando isso acontece.
- Capacidade: 1024 peças elementares (inclusive as que estão dentro dos chips; junções contam como peças), 2048 fios, 1024 grupos, slots de projeto conforme o espaço disponível e 8 modelos na biblioteca. Chips podem conter outros chips; cada instância é independente.

### Seleção e protoboard — 0.48.0

Ctrl+clique adiciona ou remove somente a peça clicada. Se todas as peças selecionadas forem do mesmo tipo, Valor (ou E) altera o valor de todas: selecione resistores, pressione E, digite 200 e confirme para aplicar 200 ohms. Ctrl+A dentro do campo seleciona o texto. Ctrl+Z desfaz a alteração conjunta. O campo Tamanho e a cor dos LEDs também aceitam seleção do mesmo tipo.

O catálogo Bancada inclui Protoboard larga (60 colunas), com o mesmo passo de furos da placa original. Ao reduzir uma peça conectada, suas pontas conservam os contatos. As marcas e áreas clicáveis dos terminais mantêm um tamanho útil na tela. A cor física dos LEDs é igual nos temas claro e escuro.

### Notas flutuantes — 0.49.0

Em Bancada, escolha **Nota** e coloque na bancada. Clique duplo ou use **Texto** no painel para escrever até 127 bytes (acentos ocupam mais de um byte); Enter confirma e Esc cancela. O texto quebra automaticamente conforme o espaço do papel. No painel, **Fundo** e **Texto** têm paletas independentes. Arraste para mover, ajuste Tamanho para ampliar e use Delete para remover. Notas não têm pinos nem participam eletricamente do circuito. São salvas no projeto e acompanham copiar/colar e desfazer.

Na 0.50.0, selecione a nota e arraste a alça azul no canto inferior direito para ampliar ou reduzir proporcionalmente (mínimo de 50% pelo arraste). O canto superior esquerdo fica fixo. Também pode digitar Tamanho no painel. Ctrl+Z desfaz o redimensionamento. A fita, a sombra e o canto levantado são apenas visuais.


### Correcoes em desenvolvimento - 0.52.1

Firmware e circuito avancam pelo mesmo tempo simulado: mudar a velocidade ou o desempenho da maquina nao muda a relacao entre delay/micros/millis e componentes externos. O solver para no proximo delay/delayMicroseconds; as instrucoes entre esperas sao instantaneas, sem emulacao dos ciclos de um AVR/ESP32. PWM continua representado pela media, conforme o modelo atual.

ATtiny85 precisa de VCC medido de pelo menos 2,7 V. Perder alimentacao reinicia a VM; retornar repete setup. Uno e ESP32 conservam a alimentacao USB ideal. Excluir, substituir ou colar uma placa nao reaproveita a execucao de outra instancia. Undo/redo reiniciam os programas para aplicar o estado restaurado.

555 e LM358 debitam a corrente positiva entregue a carga da sua alimentacao. A fonte CV/CC existente tem ganho finito (no teste de limite 10 mA houve excesso de aproximadamente 0,20 mA); nao e um limitador matematico perfeito. Corrente de repouso completa, aquecimento e tolerancias do componente fisico continuam fora deste incremento.

Aplicar uma edicao no interior de um chip fecha o grafico ao vivo, pois os indices podem mudar. Apenas consultar ou cancelar a edicao conserva o grafico. Falha ao registrar historico impede a publicacao da edicao; o rascunho e a causa permanecem visiveis.
