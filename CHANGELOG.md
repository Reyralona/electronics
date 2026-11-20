# Histórico de versões

## 0.52.1

- Correcoes da auditoria Ponytail: 555 e LM358 debitam a corrente de carga da alimentacao, incluindo o efeito do limite CV/CC.
- Placas mantem identidade durante edicoes; remover/substituir nao transfere a VM para outra placa. ATtiny so executa com VCC medido de pelo menos 2,7 V e repete setup apos queda e retorno.
- Firmware segue o tempo simulado; delay e delayMicroseconds limitam o proximo lote do solver para preservar as transicoes. Instrucoes entre esperas continuam instantaneas no modelo, sem emulacao de ciclos de CPU.
- Falha do historico aborta ou restaura a edicao; rascunhos e mensagens de falha sobrevivem a Executar e Ctrl+Enter. Aplicar alteracoes em chip fecha grafico cujo indice pode mudar.
- Conectividade do LCD reutilizada ate mudar a bancada; testes de interface usam diretorios exclusivos por processo e execucao. CTest inclui logica, lineares, protecoes e placas.
- Relatorio, evidencias e retomada em docs/CORRECOES_AUDITORIA_0_52.md. Formato 38 preservado; branch ainda nao integrada/publicada.

## 0.52.0

- Inclui as correcoes da 0.51.2: oito falhas da auditoria, rascunhos e reinicio de placas, chips personalizados ao copiar/espelhar e otimizacao do simulador.

- Integra a coleção digital: 74HC02/10/11/14, 74HC273/373/374, 74HC163/193, 74HC138/139/151/157 e 74HC4511, com fichas e testes funcionais.
- Nomes dos CIs centralizados e ajustados ao encapsulamento, tamanho e rotação; corrige o texto do NE555 saindo do corpo. Preserva os códigos dos componentes existentes ao integrar as branches.

- Fonte de bancada CV/CC: botões V (0–30 V, `Shift`+roda, passos de 0,1 V) e A (limite 0–3 A, passos de 10 mA, também no inspetor). Acima do limite a tensão cai e a corrente fica fixa, com lâmpada CC. Curto na fonte de bancada não a queima mais; baterias e clocks continuam queimando.
- Proteção pelos máximos absolutos dos datasheets: saídas 74HC (portas soltas, CIs de portas e CIs médios) avisam acima de 25 mA e queimam a partir de 1,8× (curto ou disputa de barramento); alimentação acima de 6 V avisa e acima de 7 V queima (AT28C16: 6,25 V). Anel âmbar no pino, linha no inspetor e uma mensagem no rodapé quando o conjunto de avisos muda.
- Segmentos de display e cores do LED RGB queimam acima do limite do LED. Placas: 40 mA por pino (aviso, queima a 1,8×), pinos fora da alimentação queimam (ESP32 não aceita 5 V), ATtiny85 queima acima de 6 V.
- Aviso de entrada solta, calculado só a partir das ligações: portas soltas sempre, CIs de portas apenas nas portas usadas, CIs médios alimentados e pinos de placa ligados a botão sem pull-up/pull-down.
- Ficha de cada CI (botão Ficha no inspetor ou `I`): chip isolado e ampliado, pinos numerados e nomeados, o que cada pino faz, alimentação e máximos absolutos.
- Catálogo com abas Portas, Lógica, Memória e Lineares. Novos CIs: 74HC04, 74HC74, NE555, LM358, LM393, LM7805 (TO-220), ULN2003A e L293D, com modelos ideais dos datasheets e testes de bancada. Formato de arquivo 38.
- Desempenho: respostas do simulador lidas em blocos e leitura de vários valores numa só ida e volta; a verificação de proteção não acrescenta equações ao solver. Circuito do slot 2 (com resistores nos segmentos): cerca de 0,6× sem instrumentos e 0,34× com instrumentos.
- Pesquisa de datasheets com fontes oficiais e data de consulta em `docs/datasheets/`.

## 0.51.2

- Corrige os oito erros reproduzidos na auditoria: estouro inteiro, numeros cientificos, tipos float, assinaturas de funcoes, rascunhos na troca de projeto, reinicio das placas, conexoes da protoboard larga ao criar chips e persistencia de terminais de sonda acima de 13.
- Falha ao aplicar codigo conserva o rascunho aberto; trocar projetos ou exemplos resolve o rascunho anterior e reinicia o runtime. Undo da troca recupera o programa editado.
- Verificacao de curtos reutiliza conectividade enquanto a montagem nao muda. Portas compartilham a conversao do mesmo no e eliminam uma fonte redundante da saida, mantendo alimentacao, impedancia e limiares.
- Circuito de contador/adder/decoders com 235 pecas e 333 fios ganha teste de equivalencia e benchmark. Medicao local a 50 Hz: aproximadamente 2,4x mais rapido; ainda abaixo de tempo real nesse caso, sem reduzir precisao ou pular ciclos.
- Chips personalizados: copiar/colar ancora no encapsulamento visivel (nao numa peca interna), entao a copia cai no cursor mesmo girada ou espelhada; espelhar nao inverte mais nomes, numeros de terminais nem displays. Teste com duas copias do decoder do slot 2 (uma espelhada e girada) confere as 16 entradas.
- Cada chip logico tem carga minima de 100 MOhm na alimentacao, para que a fuga de uma fonte desligada nao alimente o chip flutuando; desligar a fonte volta a convergir e deixa as saidas em alta impedancia.
- Build normal usa -O2. Roadmap revisado com pendencias reais da 1.0 e evidencias das correcoes.


## 0.51.1

Esta distribuicao reune as mudancas desde a pre-release 0.47.1.

- EEPROM AT28C16 grava e le pelo barramento, com persistencia dos bytes no projeto, consulta de conclusao e circuito de referencia com LEDs de dados e modo.
- Ctrl+clique seleciona pecas individualmente; selecoes da mesma especie permitem editar valores em conjunto. LEDs acesos usam cores mais vivas e iguais nos dois temas.
- Protoboard larga de 60 colunas, melhor encaixe de pecas reduzidas e terminais padronizados que acompanham o zoom.
- Notas com texto e cores configuraveis, redimensionamento proporcional, fita e canto suavemente levantado. Notas nao alteram a simulacao.
- Catalogo reorganizado em nove categorias, com icones proprios para notas e juncoes, busca global e todos os componentes preservados.
- Empacotamento verifica que o ZIP de fontes inclui o codigo do aplicativo e interrompe a geracao se o arquivador falhar.
- Formato 37, com leitura de projetos anteriores. Modelos eletricos continuam didaticos; a distribuicao permanece experimental.

## 0.51.0 - 20/11/2026

- Papel curvado em toda a superfície, com luz e sombra; fita afastada da área de escrita. Nota e junção recebem miniaturas próprias, sem reutilizar LED.
- Catálogo organizado em nove categorias, sem repetir ou omitir componentes: Básicos, Digital, Salvos, Sensores, Saídas, Medição, Fontes, Placas e Bancada. Busca global preservada.

## 0.50.0 - 20/11/2026

- Post-its com fita, sombra e canto levantado; alça no canto inferior direito para redimensionar proporcionalmente, mantendo o canto oposto fixo e permitindo desfazer.
- Terminais com raio uniforme, independente da escala do componente, acompanhando o zoom e preenchendo o furo da protoboard. Área de clique preservada.

## 0.49.1 - 20/11/2026

- Corrige erro de cálculo ao simular circuitos com notas. Testes verificam que as notas preservam o circuito elétrico e o fluxo de gravação da EEPROM.

## 0.49.0 — 20/11/2026

- Notas flutuantes no catálogo Básicos: texto por clique duplo, cores de fundo e texto independentes, arraste, escala, cópia, desfazer e salvamento. Formato 37, leitura dos projetos anteriores preservada.
- Bancada pessoal de EEPROM anotada por etapas; ligações preservadas.

## 0.48.0

- EEPROM AT28C16 grava pelo barramento: endereço e dado retidos pelas bordas de CE/WE, proteção por OE,
  espera após ligar, ciclo de 1 ms e consulta de conclusão por IO7. Bytes escritos acompanham o projeto
  ao salvar/reabrir e sobrevivem à alimentação desligada; a gravação não reinicia os outros CIs.
- O modelo usa eventos do ngspice, independentemente da taxa de quadros; a DLL faz parte do build e dos pacotes.
- Ctrl+clique adiciona ou remove somente as peças clicadas. Seleções da mesma espécie permitem editar
  valor, tamanho e cor dos LEDs em conjunto; valores diferentes são indicados e a alteração tem um único undo.
- LEDs acesos ficam mais vivos, com a mesma cor física nos temas claro e escuro.
- Conectores têm área clicável em pixels, escolhem o pino mais próximo e preservam o clique no corpo da peça.
  O indicador do pino apontado deixa de crescer com o zoom.
- Peças menores podem encaixar no passo da protoboard; redimensionar uma peça já conectada preserva as pontas.
- Protoboard larga com 60 colunas/840 contatos no catálogo. O formato 36 preserva a leitura de montagens
  antigas, inclusive os chips aninhados que usavam máscaras de 420 pinos.
- Circuito de gravação/leitura na protoboard em `tests/fixtures/eeprom-programmer.elec`, com seletores de
  endereço e dados, isolamento por 74HC245, botão GRAVAR e LEDs de dados/modo.

## 0.47.1

- Esta distribuição também inclui as novidades da 0.47.0: até 1024 peças e 2048 fios, CIs 74HC161/173/245/283
  e EEPROM AT28C16, ritmos de simulação até 0,001×, escala real da interface, textos nítidos e ajustes nos fios.
- A EEPROM lê conteúdo hexadecimal salvo no projeto; edição na interface e gravação pelo pino WE seguem pendentes.
- O build interrompe antes de compilar quando o ngspice está ausente ou incompleto e indica como prepará-lo.
- A referência DC e o teste de sessão contínua verificam o solver com PATH mínimo, sem depender do MSYS2.
- Teste de regressão cobre instalação ausente, DLL principal ausente, modelo digital ausente e executável inválido.

## 0.47.0

- Limites maiores: 1024 peças e 2048 fios por montagem (eram 256 e 512).
- Cinco CIs novos na aba **Digital**: contador **74HC161**, registrador **74HC173**, transceptor **74HC245**,
  somador **74HC283** e EEPROM **AT28C16**. A lógica interna de cada um é digital; só os pinos passam pelo
  circuito analógico, por isso dezenas de CIs simulam perto do tempo real.
- A EEPROM lê seu conteúdo em hexadecimal (texto da peça). Endereços não gravados leem `FF`; gravar pelo pino WE
  não é simulado. Por enquanto, o conteúdo só vem do arquivo de projeto: ainda não há editor na interface.
- Um computador de 8 bits no estilo SAP-1 (25 CIs, 312 peças) foi montado e testado com esses CIs durante o
  desenvolvimento, mas não entrou nos exemplos: a montagem ainda precisa de um desenho mais legível.
- Formato de projeto 35, por causa dos tipos novos. Projetos anteriores continuam abrindo.
- Fios lado a lado na breadboard: as sombras são desenhadas antes de todos os fios e não cobrem mais os vizinhos;
  a sombra é uma faixa translúcida de contato, sem deslocamento sólido. Dobras têm junção arredondada, sem degrau.
- A ponta do fio no furo para no primeiro joelho; com uma dobra perto do furo, não sobra mais um toco para fora.
- Ritmos mais lentos da simulação: 0,001×, 0,002×, 0,005×, 0,01× e 0,02×, além dos anteriores. Em 0,001×,
  um clock de 1 kHz pisca uma vez por segundo. O passo elétrico e o estado do circuito não mudam ao trocar o ritmo.
- **Escala da interface** funciona dentro da janela maximizada ou em tela cheia: diminuir a escala reduz a
  interface e aumenta a bancada, sem tirar a janela do modo maximizado. A escala do Windows é considerada.
- Textos nítidos: as letras são desenhadas na resolução real da tela, em pixels inteiros, em vez de ampliadas
  de 15 px. A interface não é mais esticada nem distorcida em janelas de outros formatos.
- Bordas suavizadas (antisserrilhado 4×) em círculos, cantos e fios; sombras suaves nos painéis; a barra de
  ações sobre a bancada tem fundo próprio e se destaca da grade.

## 0.46.0

- Ao carregar um projeto válido, a bancada ajusta zoom e posição para enquadrar peças, terminais, chips e fios.
- Projetos passam a crescer além dos nove slots anteriores, com seis cartões por página e navegação por setas ou Page Up/Page Down.
- A galeria maior mostra miniaturas da bancada enquadrada. Cada imagem é salva ao lado do projeto e gerada para saves antigos quando o cartão aparece.
- O formato `.elec` e o modelo elétrico continuam na versão anterior.

## 0.45.1

- A bancada ocupa toda a largura da janela maximizada.
- Seleção de componentes próximos usa dimensões mais fiéis ao desenho e escolhe o corpo mais próximo em áreas sobrepostas; validado no slot 4.
- Portas lógicas espelhadas desenham os trechos dos terminais do mesmo lado que os pinos do circuito.
- Capturas leem o quadro antes da troca de buffers e falham se vier totalmente preto. É possível gerar imagens individuais dos 59 componentes em 1×, 3× e 7×.
- Contornos do LED RGB desligado, do sensor de luz ambiente e do domo PIR ficaram mais visíveis nos temas claro e escuro.
- Sem alteração do formato de projeto ou do modelo elétrico.

## 0.45.0

- Projetos mostram um resumo de peças/fios. Substituir um slot ocupado exige confirmação; cancelar preserva o arquivo.
- Barra com Desfazer, Refazer, Um passo, Criar chip, Ao vivo e Curva completa, usando os comandos existentes.
- Busca de peças por nome/identificador em todo o catálogo, sem distinguir maiúsculas ou acentos portugueses.
- Tab/Shift+Tab percorrem controles com foco visível; Enter/Espaço ativam. Ctrl+Tab mantém a troca de categorias.
- No editor de código, Tab continua indentando; Ctrl+Tab acessa os controles.
- Escala amplia a interface inteira; desenho e cliques preservam proporção em janelas de outros formatos.
- Configurações separam Simulação de aparência; explicação do gráfico aparece inteira. Medida/terminal ficam junto ao gráfico ao vivo.
- Biblioteca vazia explica como criar chips; edição de valores tem Confirmar/Cancelar e erros junto ao campo.
- F2 abre Exemplos como o botão; Ctrl+F2 mantém o avanço direto. Idiomas usam nomes, em vez de bandeiras.
- Sem alteração do formato de projeto (34), dependências ou modelos elétricos.

## 0.44.1

- Corrigido: funções recursivas com muitas variáveis locais param antes de exceder a memória do interpretador.
- Corrigido: `random` aceita limites fracionários sem fechar o aplicativo; os limites são truncados para inteiros.
- O editor recusa programas que não cabem, inclusive após expandir tabulações, sem cortar o original.
- Pausar a simulação também interrompe a execução dos programas das placas.
- Alterar uma chave DIP invalida a leitura anterior, inclusive com a simulação pausada.

## 0.44.0

- Fios e LEDs compartilham 10 cores: vermelho, verde, azul, amarelo, branco, laranja, roxo, rosa,
  ciano e verde-limão. Os fios têm também o preto. Projetos antigos mantêm as cores (formato 33).
- O LED apagado ficou bem mais escuro, para distinguir de longe quando acende.
- Selecionar um fio abre o painel do fio: de onde vem, aonde vai, a corrente e a cor. A cor saiu
  das Configurações.
- `M` ou o botão **Espelhar** espelha peças e chips, trocando os terminais de lado. Visores,
  displays, LCD e marcas das baterias continuam legíveis (formato 34).
- Corrigido: clicar no campo **Valor** voltou a abrir a edição; os botões **Girar**,
  **Espelhar** e o play do topo usam o mesmo caminho direto.

## 0.43.0

- A simulação contínua escolhe o próprio passo máximo (1 ms, ou 1/32 do período de uma senoide).
  O passo das Configurações deixa de limitar a velocidade. Na máquina de referência, um
  decodificador de ~30 portas passou de ~0,08× para mais de 1× de capacidade, e um contador de
  4 bits com decodificador, de ~0,025× para ~0,6×.
- Circuitos lentos recebem lotes menores, e a tela continua atualizando.
- `T` abre um gráfico ao vivo sem pausar a simulação. `Shift+T` mantém a curva completa desde
  t = 0. A janela do gráfico tem um botão X para fechar.
- Corrigido: depois de um lote acima de 10 s, todos os lotes seguintes falhavam com "resultado
  incompleto". Agora a sessão reinicia no lote seguinte.

## 0.42.1

- `build.ps1`, a preparação do ngspice e o empacotamento encontram o MSYS2 mesmo fora do
  `PATH` (em `MSYS2_ROOT` ou na pasta padrão `C:\msys64`) e explicam quando ele falta.
  Mensagens com acento aparecem corretamente no Windows PowerShell 5.1.
- Excluir chips do mesmo lugar da biblioteca duas vezes no mesmo segundo não falha mais.

## 0.42.0

- A simulação contínua mantém apenas a amostra mais recente no ngspice. Capacitores,
  circuitos digitais e instrumentos preservam as leituras; o gráfico solicitado com `T`
  continua calculando a curva completa. O teste RC de 10 s deixou de perder ritmo por
  crescimento do histórico na máquina de referência.
- Configurações oferecem ritmo solicitado de 0,05× a 1× sem alterar frequências ou
  valores dos componentes. O rodapé mostra o ritmo observado e o escolhido.
- O multímetro limita a tensão produzida no modo resistência e avisa quando a medição
  é feita com uma fonte ativa. O osciloscópio e o multímetro foram conferidos em um
  multiplicador Cockcroft–Walton incluído nos testes do repositório.
- Corrigidos o cálculo do gráfico para o gerador de funções, o agendamento de gráficos
  durante a simulação contínua e a abertura do executável de desenvolvimento sem o
  compilador no `PATH`.
- A distribuição e os testes usam apenas fontes e fixtures versionadas. Scripts de
  desenvolvimento descobrem as ferramentas pelo `PATH`; as notas de release rejeitam
  caminhos locais.

## 0.41.0

- Simulação contínua só mede corrente individual dos fios quando a visualização de corrente está ligada.

- Junções nomeadas na aba Digital permitem escolher os terminais do chip, inclusive CLOCK compartilhado.

- Chips podem conter outros chips em vários níveis, preservando nomes, terminais e encapsulamentos.
- O editor permite entrar e voltar por nível. Aplicar afeta o contexto imediatamente acima, com desfazer.
- Copiar, salvar/reabrir e desagrupar preservam a composição. Formato 32, com leitura dos anteriores.

## 0.40.0

- Portas lógicas realistas (família 74HC): alimentadas pela primeira fonte da bancada, de 2 a 6 V, com
  entrada baixa até 30% e alta a partir de 70% do VCC. A saída alta segue a fonte, atrás de ~50 Ω, e puxa
  corrente da fonte. Com 2 V, a porta funciona em 2 V; fora de 2–6 V, fica desligada.
  Antes, as portas inventavam 5 V e, com entradas em 2 V, davam 2,5 V ideais que queimavam um LED direto.
- CIs 74HC quádruplos com a mesma faixa de 2 a 6 V e limiares proporcionais ao VCC.
- Limites de 64 para 256 peças e de 128 para 512 fios.
- Junções de fio: terminar um fio em outro fio (ou `Alt`+clique num fio para puxar uma derivação) cria um ponto
  de junção; `Alt`+arrastar move o ponto e apagar o último fio o remove.
- Dois cliques num terminal de chip nomeiam o terminal; o nome aparece dentro do corpo, junto ao terminal.
  Formato de arquivo 31; arquivos antigos continuam abrindo.

## 0.39.0

- Zoom mínimo de 35% para 12%: circuitos grandes cabem inteiros na tela.
- Arrastar a seleção até a borda da bancada rola a vista, para alcançar peças fora da tela.
- Com pouco zoom, as bolinhas dos terminais encolhem com as peças, e os nomes das portas e os números
  dos terminais dos chips somem abaixo de 60%.
- `Alt`+arrastar um terminal de chip o leva para o outro lado ou para outra posição (3AND: 3 entradas à
  esquerda, saída à direita). O arranjo é salvo no formato de arquivo 30; arquivos antigos continuam abrindo.
- A dica da bancada mostra `Ctrl+A`. O chip não tem mais a marca escura na borda; o pino 1 segue indicado pelo ponto claro no canto.

## 0.38.0

- Painel da peça mais compacto (320 px de largura, linhas e cores menores), alinhado à mesma borda direita.
- Botão **–** recolhe o painel numa etiqueta com o nome da peça; clicar na etiqueta o reabre.
  Recolhido, o painel não aceita cliques em controles ocultos. O estado vale ao trocar de peça e no editor do chip.
- Projetos, Exemplos e Configurações abrem um de cada vez: abrir um fecha o outro, pelo topo ou por F1/F3.
- Autoteste: minimizar, clicar onde ficavam as cores (sem efeito), reabrir pela etiqueta e escolher uma cor.

## 0.37.1

- Renomear um chip no editor atualiza o nome na bancada e na entrada existente da biblioteca
  ao confirmar com Enter. Não é preciso usar Aplicar ou Salvar modelo para confirmar o nome.
- Descartar alterações do circuito não descarta um nome já confirmado. Renomear preserva o circuito
  salvo na biblioteca; alterações do rascunho continuam dependendo de Salvar modelo ou Aplicar.
- Renomeação disponível somente no botão Renomear dentro do chip. Removidos o botão externo e F4.
- Regressões para confirmar “4and”, sair, salvar/reabrir o projeto e manter a entrada da biblioteca;
  nomes inválidos e falhas de gravação preservam o nome anterior.

## 0.37.0

- `R` gira chips personalizados em passos de 90°. Corpo, portas, clique e fios acompanham a orientação;
  o circuito interno permanece independente. Desfazer/refazer e salvar/reabrir conservam a rotação.
- Contato direto entre terminais de qualquer tipo de peça, incluindo breadboards e portas de chips
  diferentes. O encaixe usa os terminais visíveis. Separar as peças desfaz o contato, sem criar fios ocultos.
- Chips também encaixam ao arrastar e ao colocar uma cópia da biblioteca. Terminais internos ocultos
  continuam isolados do exterior; cada porta exposta liga ao nó real do circuito interno.
- Formato de projeto 29. Arquivos anteriores continuam legíveis, com chips na orientação original.
- Regressões para todos os terminais dos 58 tipos, chips em quatro orientações, persistência,
  circuito com dois interruptores em série e fluxo gráfico de encaixar/separar/desfazer/refazer.

## 0.36.0

- Build incremental com CMake/Ninja e empacotamento ZIP com CPack; `Empacotar.cmd` e
  `tools/package-windows.ps1` geram bundle, instalador NSIS por usuário, fontes e hashes SHA256.
- Dependências fixadas por versão/hash, com licenças e fontes correspondentes num arquivo separado.
  Os pacotes não exigem MSYS2, GCC ou ngspice instalados na máquina de destino.
- ZIP salva ao lado do executável; instalação usa `%APPDATA%/Bancada/Bancada` e preserva os dados
  ao desinstalar. Builds de desenvolvimento mantêm o comportamento anterior.
- Carregamento do modelo digital no bundle independente de caminhos absolutos e nomes curtos 8.3.
- Build otimizado revelou um buffer pequeno de expressão de porta; ampliado com limites nomeados.
  Testes de integração corrigidos para evitar contatos involuntários e considerar a fuga de entrada.
- Artefatos experimentais sem assinatura digital. Validação local com PATH mínimo e caminhos com
  espaços; ainda é necessária validação numa VM Windows limpa antes de anunciar suporte amplo.

## 0.35.0

- Interface em português (Brasil) e inglês (EUA), com seleção por bandeiras em Configurações
  e preferência salva. Na primeira execução, o idioma acompanha o sistema (inglês como alternativa).
- Catálogo de 622 textos nos dois idiomas: interface, peças, exemplos, simulação e editor de código.
  Testes verificam estilo, codificação e compatibilidade dos formatos de mensagem.
- Nomes longos de peças usam duas linhas no catálogo. Símbolos elétricos permanecem independentes
  do idioma; nomes de chips digitados e código das placas preservam o conteúdo original.
- Guia de textos em `docs/TEXTOS.md` e documentação de uso atualizada.
- Verificação: `./build.ps1 -GuiTests` passou; capturas em pt-BR/en-US revisadas.
  Limite conhecido: números em português ainda são exibidos com ponto decimal.

## 0.34.1

- Correção: entrada de porta lógica ou de CI 74HC atrás de interruptor aberto lia nível ALTO na
  simulação ao vivo (AND de 4 entradas acendia com tudo desligado). Entradas têm agora 100 MΩ de
  resistência de entrada e uma entrada aberta lê BAIXO.
- Correção: renomear/salvar um chip no editor isolado não cria mais uma segunda entrada na aba
  Blocos; o chip e suas cópias atualizam a própria entrada (formato de arquivo 28, lê os antigos).
- Licença GPL-3.0 (`LICENSE`).
- Documentação para o repositório público: README reescrito (o que é, instalação passo a passo,
  primeiros passos, limites), guia de uso atual em `docs/GUIA.md` e documentos de projeto movidos
  para `docs/` com cabeçalho explicando o propósito de cada um.

## 0.34.0

- Microcontroladores programáveis: Arduino Uno, ESP32 DevKit e ATtiny85, com linguagem estilo
  Arduino (setup/loop, pinMode, digitalWrite/Read, analogRead/Write, delay, millis, Serial...).
- Editor de código lateral (duplo clique na placa ou "Editar código" no inspector): fonte
  monoespaçada, números de linha, auto-indentação, sugestões ao digitar (Ctrl+Espaço), erro de
  compilação marcado na linha, monitor serial; Ctrl+Enter/Executar envia o programa (Ctrl+Z desfaz).
- LCD 16x2 I2C: mostra o texto de `lcd.print` quando SDA/SCL estão ligados aos pinos I2C da placa.
- X liga/desliga transparência das peças: terminais atrás de outra peça ficam acessíveis.
- Com uma peça selecionada, escolher outro tipo no catálogo troca a peça no lugar (fios mantidos
  por terminal; Ctrl+Z desfaz). Esc agora também limpa a seleção.
- Blocos personalizados expõem só um terminal por nó livre ou ligado ao exterior: três AND formando
  um AND de 4 entradas viram um bloco de 5 terminais (antes 9), inclusive via breadboard.
- Executável único `build/build.exe`.

## 0.33.0

- Terminal sobre terminal conecta: encaixe de 10 px ao arrastar ou posicionar peças.
- Rótulos de terminais sem sobreposição; por padrão só no terminal sob o mouse (opção para todos).
- Corrente nos fios como bolinhas amarelas em movimento, no lugar das setas.
- Remover blocos da biblioteca (botão × com confirmação; arquivo vai para blocks/lixeira).
- Relé com terminais B+/B−; autoteste GUI com 191 fases.

## 0.32.0

- Infraestrutura de várias medições por peça (até 8) e estado por peça (bits/modo), formato 26.
- 17 dispositivos: LED RGB, 7 segmentos, DIP 4 vias, DPST, relé, motores CC/vibração/encoder,
  micro servo, piezo, gerador de função, multímetro, osciloscópio, sensores PIR, gás, IR e distância.
- Displays dentro de blocos aparecem no corpo do bloco; biblioteca em seis abas; 18 exemplos.
- test_devices verifica cada dispositivo no ngspice; suíte completa e autoteste GUI passaram.

## 0.31.0

- Lâmpada indicadora (resistência constante 250 Ω, brilho e halo pela corrente); formato 25 (lê 1–24).
- Removido o protótipo legado de circuito fixo (preservado nas tags v0.1.0–v0.16.0).
- ROADMAP_1_0.md: histórico desde 14/11, revisão de código e pendências da 1.0 em ordem de dependência.

## 0.30.0

- Diálogo de Exemplos com 15 circuitos (título, categoria e descrição); F2 avança para o próximo.
- Correção: exemplo 74HC00 abortava na sessão contínua ("Timestep too small"); CIs quad usam
  acionamento contínuo entre limiares e chaves com histerese.
- test_examples_live roda todos os exemplos no ngspice; autoteste GUI com 189 fases.

## 0.29.0

- Interface revisada: cabeçalho com botões reais, biblioteca com abas e barra de rolagem, inspector
  com Valor/Tamanho clicáveis, rodapé com dicas por contexto e textos acentuados.
- Configurações em um único diálogo organizado, com toggles, campos e "Restaurar padrões".
- Preferências salvas automaticamente em saves/preferences.txt; tema/texto não mudam ao carregar slot.
- Removidas teclas S/L legadas (bancada.elec oculto) e dicas obsoletas.
- Arte: capacitor cerâmico, fonte de bancada com visor, interruptor e fototransistor redesenhados.
- Geometria compartilhada (app_layout) entre desenho, entrada e autoteste; suíte e 187 fases GUI passaram.

## 0.28.0

- Catalogo analogico: diodo, NPN/PNP, LDR, flex, FSR, inclinacao, fotodiodo, luz ambiente, TMP36,
  umidade do solo e baterias 9 V, AA e moeda 3 V (36 tipos). Sliders de ambiente salvos e desfaziveis.
- Revisao de arte reproduzivel por close-ups em dois temas (tools/capture-components.ps1).
- Inspector tecnico e dicas no rodape; terminologia "terminais".
- Brilho em tempo real: circuitos com fonte/bateria usam a sessao continua; potenciometro sem piscar.
- Correcao: desligar fonte/chave com capacitor carregado travava o solver (razao de contato 1e18);
  agora 1 mOhm/10 GOhm e o LED apaga gradualmente. Regressao com a fiacao exata do slot relatado.
- build.ps1 nao falha quando spice-live-host.exe esta em uso e atualizado.
- Formato 24 (le 1-23). Suite completa e fases GUI passaram. Pendentes em CATALOGO_COMPONENTES.md.


## 0.27.0

- Backend continuo ngspice em host nativo isolado por documento/contexto.
- Carga do capacitor e memoria do DFF conservadas ao operar fontes, botoes, interruptores e valores.
- Espaco pausa/continua; Ctrl+Espaco reinicia; slots/exemplos carregados com estado eletrico novo.
- Tempo simulado e limite de reconexao/frequencia visiveis; curvaT manual identificada como analise separada.
- Feedback de salvar/editar nao e sobrescrito por cada amostra automatica.
- Perninhas com os mesmos cantos arredondados e hit testing dos fios; sondagem dos14 pinos dos novos CIs.
- Suite completa,183 fases GUI, RC/DFF/botoes/protecao, isolamento de contextos, worker, play, pause e reset
  e execucao nativa comPATH minimo passaram. Capturas de descarga e perninhas revisadas.
- Novas conexoes/frequencia ainda reiniciam estado; memoria de vetores e configuracao ngspice
  externa precisam evoluir antes da1.0. Arquivos continuam no formato 23.


## 0.26.0

- Perninhas com pontas e joelhos editaveis: Alt+arraste e Alt+duplo clique, sem mover o corpo.
- Contato direto entre terminais de pecas comuns, com encaixe de pontas e feedback eletrico.
- Geometria dinamica por pino, clone profundo, undo/redo, copy/paste, slots e formato 23.
- Testes ngspice fecham circuito sem fios; separacao de uma ponta interrompe corrente.
- Suite completa e180fasesGUI passaram; captura --capture-leads revisada.
- Continuidade eletrica permanece em implementacao; prova isolada preservou RC e DFF via resume.


## 0.25.1

- Corpo do LED conserva projecao vertical ao girar; terminais e contatos acompanham a rotacao.
- Compilacao estrita, 171 fases GUI e captura comparativa de LEDs em 0/90 graus revisadas.


## 0.25.0

- Nome Bancada, icone de janela, fonte Segoe UI suavizada e tema escuro salvo.
- Texto ajustavel entre 12 e 20 px em F1, padrao 15; ainda nao escala paineis inteiros.
- Ctrl+scroll redimensiona pecas; Alt+clique facilita selecionar o corpo.
- Biblioteca rolavel: NOR, XNOR, potenciometro, botao momentaneo, 74HC08/32/86.
- Potenciometro com cursor Shift+scroll; botao abre ao soltar ou perder foco.
- LED vertical e brilho gradual conforme corrente medida em DC/transiente; potenciometro e botao com volume.
- Sem sombra deslocada de fio no tema escuro; cores dos fios preservadas.
- Chips personalizados: largura Ctrl+scroll, altura Ctrl+Shift+scroll; limites de pinos preservados, undo e slots.
- Movimentos puramente visuais sem breadboard conservam analise automatica concluida e posicao da curva.
- Formato 22, leitura 1-21; testes eletricos ngspice, suite completa e 171 fases GUI passaram.
- Pendente: continuidade de carga/estado digital em mudancas eletricas, terminais dobraveis,
  escala completa da GUI e instrumentos posicionaveis. Nao atende ainda a meta 1.0.


## 0.20.0

- Registro inicial centraliza metadados, IDs, parametros primarios e funcoes dos pinos.
- Rotulos/ajuda ao apontar distinguem entrada, saida, alimentacao, terra e terminais sem polaridade.
- Terminais de blocos maiores e espacados; acao existente rotulada como desempacotar na bancada.
- Requisitos de chips renomeaveis, editor isolado e instrumentos registrados em QUALIDADE_DE_USO.md.
- Build estrito, teste do registro e 106 fases GUI passaram; capturas revisadas.
- Formato segue 16; registro completo, editor isolado e instrumentos ainda pendentes.

## 0.19.0

- Ctrl+G cria blocos compactos com terminais expostos; Ctrl+Shift+G restaura o interior.
- Aba Blocos salva oito templates locais e coloca instancias independentes; clipboard/historico suportam grupos.
- Componentes e modelos eletricos internos preservados; contatos externos de breadboard materializados em fios.
- Formato 16 grava grupos/membros/portas e preserva leitura dos formatos 1-15.
- Testes DC/RC com ngspice, independencia/isolamento de instancias, codec estrito e falhas atomicas passaram.
- Build estrito e 100 fases GUI passaram; captura de packing revisada.
- Sem nesting ou encaixe automatico de blocos em breadboards; limites existentes continuam explicitos.

## 0.18.0

- Nove save states de montagem/configuracoes em F3; Ctrl+S salva e Ctrl+L carrega o slot escolhido.
- Biblioteca em abas com miniaturas dos componentes, desenhadas com o renderer nativo.
- Selecao por area/Shift+arraste, Ctrl+A/C/X/V, mover conjunto e exclusao com historico.
- Fios internos, cores e joelhos preservados na copia; falhas de colagem nao alteram a montagem.
- Testes de selecao/clipboard/slots e 88 fases GUI passaram; capturas revisadas.
- Packing reutilizavel com portas continua pendente e prioritario.

## 0.17.5

- Revisoes monotonas substituem comparacoes integrais da montagem no ciclo de simulacao.
- APIs de edicao atualizam a revisao; carregar, exemplos e undo/redo preservam sua continuidade.
- Um unico tipo de resultado valido substitui flags independentes DC/transiente.
- SimState passa de 290.576 para 183.040 bytes; sem alteracao dos modelos eletricos.
- Testes de mutacao, revisoes, transicoes e resultado pendente apos undo; build estrito e GUI passaram.
- Dez capturas finais identicas a 0.17.4. Registro de componentes e backend de passo real ainda pendentes.

## 0.17.4

- Funcoes de entrada/desenho divididas por operacao e aparencia, todas abaixo de 100 linhas.
- Invalidacao, reproducao e LEDs em SimState sem SDL/OpenGL; testes com tempo controlado.
- Correcao: sonda com indice de pino negativo passa a usar a selecao automatica valida.
- Build estrito e autoteste GUI passaram; capturas de montagem e transientes preservadas.
- Validade ainda usa flags; revisoes, registro de componentes e backend de passo real permanecem pendentes.

## 0.17.3

- Ciclo de vida da janela separado da entrada e do desenho; renderizacao recebe estado somente para leitura.
- Comandos de teclado extraidos e eventos divididos por tipo de interacao.
- Teste sem janela cobre edicao, historico, exclusao de fio e navegacao de amostras.
- Build estrito, autoteste GUI e captura NAND identica ? referencia; sem mudanca do modelo eletrico.
- Divisao das funcoes grandes de clique/desenho e consolidacao de SimState ainda pendentes.

## 0.17.2

- AppState e SimState organizam os dados da aplicacao e os resultados da simulacao.
- Cenarios de captura e 79 fases de autoteste extraidos da janela; atualizacao de simulacao em modulo proprio.
- Teste sem janela verifica invalidacao DC/transitoria e LEDs por amostra; modelo eletrico preservado.
- Eventos sinteticos inicializados explicitamente; autoteste e captura NAND verificados sem regressao visual.
- Estilo de formatacao registrado em .clang-format; entrada e desenho ainda precisam ser separados.


## 0.17.1

- Preparacao do ngspice baixa explicitamente gcc-libs e libwinpthread e verifica as DLLs antes de publicar o runtime.
- Falhas de processo mostram codigo Windows/saida; 0xC0000135 identifica DLL ausente.
- Teste com instalacao nova e PATH minimo confirma DC, transientes e exemplos sem MSYS2 no PATH.
- Executavel de uso: `build/bancada.exe`. Trabalho do outro worktree preservado.


## 0.17.0

- Integracao revisada do trabalho do Claude: calculo automatico, executar/pausar, giro por mouse e grade com zoom/pan.
- Portas didaticas, clock, flip-flop D e 74HC00 aproximado; tres exemplos F2, incluindo DIP na breadboard.
- LEDs e correntes em fios acompanham amostras transitorias, com sentido e faixas de magnitude.
- Sobrecarga estatica didatica reversivel; transientes nao usam inferencias de queima por equivalente DC.
- Resultados obsoletos invalidados e causa de indisponibilidade das correntes preservada.
- Primeira extracao de capturas/eventos de teste para dev_capture; imagem automatica identica antes/depois.
- QUALIDADE_CODIGO.md e CONTINUIDADE.md orientam a continuidade; AGENTS.md referencia ambos.
- Validacao: build estrito, autoteste GUI, testes de processo/job/exemplos, RC/RL e tabelas digitais com ngspice.
- Ainda experimental: transientes em lote, capacidades fixas, catalogo parcial, sem modelo termico ou runtime distribuido.


## 0.16.0

- Bancada com zoom, encaixe de componentes, rotação, breadboard ampliada com trilhos e fios coloridos.
- Análise DC experimental com ngspice, inspeção de corrente e visualização em fios sem caminhos ideais ambíguos.
- Sondagem transitória de tensão ou corrente, gráfico navegável e parâmetros configuráveis; não constitui simulação completa tick a tick.
- Valores editáveis, desfazer/refazer e projeto versão 10 com configurações; leitura de formatos anteriores.
- ngspice continua como dependência experimental de desenvolvimento, não distribuída com o aplicativo; modelos e catálogo ainda são incompletos.

## 0.15.0

- Persistência da bancada com componentes, fios e joelhos, em formato versionado separado.

## 0.14.0

- Fios com joelhos arrastáveis, remoção por duplo clique, curvas arredondadas e sombreamento.
- Painel de configurações de grade, pontos de controle e arredondamento.
- Capturas reproduzíveis para revisão visual e testes de geometria dos fios.
## 0.13.0

- Breadboard instanciável com 100 furos e 20 grupos independentes de cinco contatos; fios podem ser ligados aos furos.

## 0.12.0

- Nova bancada experimental separada, com biblioteca, componentes posicionáveis e fios entre terminais selecionados.
- Primeiro modelo discreto de conectividade para fonte, interruptor e LED; pausa e avanço por passo.
- Protótipo anterior preservado, incluindo seu formato de projeto.

## 0.11.0

- Instruções e estado aparecem na janela usando texto básico do Windows/OpenGL.

## 0.10.0

- Avanço de um passo com a simulação pausada e inspeção dos estados observados da montagem.

## 0.9.0

- Terminais visíveis permitem conectar ou desconectar os três pares previstos com o mouse; pares não suportados são rejeitados.
- Espaço pausa ou retoma a simulação, mantendo o último estado observado enquanto pausada.

## 0.8.0

- Três ligações fixas podem ser conectadas ou desconectadas com as teclas 1, 2 e 3; o LED depende do circuito completo. O formato 2 salva essas ligações e continua lendo projetos do formato 1.

## 0.7.0

- Grade e símbolos visuais identificam fonte, interruptor e LED; interruptor também indica estado pela posição.

## 0.6.0

- Desfazer e refazer ações de componentes e carregamento, com histórico limitado em memória.

## 0.5.0

- Salvar e carregar posições e estados em arquivo local versionado; validação impede carregamento parcial.

## 0.4.0

- Zoom com a roda do mouse, deslocamento com setas e restauração da vista com Home.

## 0.3.0

- Componentes podem ser reposicionados arrastando com o botão direito; as ligações acompanham os componentes.

## 0.2.0

- Fonte e interruptor podem ser alternados por clique ou teclado; a fonte mostra seu estado visualmente.
- Inclusão de script para compilar e testar e de política de versões e recuperação.

## 0.1.0

- Circuito fixo didático e primeira janela SDL3/OpenGL.


## 0.21.0 - 15/11/2026
- Editor isolado para interior de blocos, com modelo, historico e simulacao independentes.
- Renomeacao UTF-8, salvar modelo separado de aplicar a instancia, e confirmacao ao sair.
- Aplicacao transacional com remapeamento de portas por proveniencia e undo; rejeita remover
  terminal conectado em vez de reconectar silenciosamente a outro componente.
- Formato 17 com nomes, leitura retrocompativel 1-16 e cache de texto Unicode via GDI/OpenGL.
- Testes de isolamento assincrono e referencia eletrica; GUI ampliada de 106 para 121 fases.
- Limites dinamicos, instrumentos com pontas e demais criterios da 1.0 continuam pendentes.


## 0.22.0 - 15/11/2026
- Grid configuravel com encaixe opcional, suspensao por Alt e controle separado da breadboard.
- Coordenadas do mundo compartilhadas pelo desenho e encaixe, sem depender do zoom.
- Prioridade da breadboard para pecas individuais; movimento de grupos preserva distancias.
- Formato 18 conserva preferencias de alinhamento e continua lendo formatos 1-17.
- Testes puros de grid, persistencia invalida/antiga e consulta de breadboard sem mutacao;
  suite completa e 132 fases graficas passaram. Painel revisado em capture-grid-settings.bmp.


## 0.23.0 - 15/11/2026
- Duplo clique em trecho de fio insere joelho; em ponto existente remove. Undo/redo e persistencia.
- Hit testing e insercao compartilham caminho renderizado e identificam o trecho nas curvas.
- Tolerancia dos joelhos passa a acompanhar zoom; inserir torna os pontos de controle visiveis.
- Casos puros de projecao, ordem, curvas, rejeicao e capacidade; GUI cobre inserir/remover,
  desfazer e salvar/reabrir. Modelo eletrico e formato 18 preservados.


## 0.24.0 - 15/11/2026
- Tamanho ajustavel no inspetor, com escala uniforme para corpo, pinos e selecao, incluindo breadboard.
- Transformacao local compartilhada evita divergencia entre desenho, contatos e rotacao.
- Formato 19 preserva escala; leitura 1-18, undo/redo, clipboard e editor isolado conservados.
- Pinos e rotulos se adaptam a pecas reduzidas; enquadramento do editor considera escala.
- Testes: resistor entre furos adjacentes, rotacoes/inversa, rejeicao, persistencia e bloco com
  resistor reduzido validado com ngspice; GUI ampliada de 138 para 146 fases.
- Dobra dos terminais, presets fisicos e demais criterios 1.0 continuam pendentes.


## 0.24.1 - 15/11/2026
- Corrente preserva a cor dos fios em DC e transientes. Setas neutras indicam sentido e faixa;
  ponto animado DC neutro. Removida sobreposicao vermelha/laranja que ocultava a paleta.
- Capturas DC desligado/ligado com fios azul/vermelho/verde e transiente revisadas; amostras de
  pixels nos tres fios conservaram RGB. Suite completa e 146 fases GUI passaram.
- Integrada preparacao interna de caminhos dinamicos e ciclos de vida de snapshots, historico,
  workers, clipboard e packing. Terminais flexiveis ainda NAO integrados nem entregues.
