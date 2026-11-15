# Simulador de elétrica e eletrônica

Projeto de um aplicativo educacional nativo para Windows x64, principalmente em C. A proposta é aprender montando e simulando circuitos em uma área de trabalho 2.5D.

A visão, o escopo proposto e as orientações de trabalho estão em [prompt.md](prompt.md). As decisões provisórias estão em [DECISOES.md](DECISOES.md).

## Bancada de montagem experimental

Clique em um terminal, depois em pontos vazios para acrescentar até 16 joelhos, e termine em outro terminal. Arraste os pequenos quadrados dos joelhos para ajustar o trajeto; dois cliques removem um joelho. Os cantos são arredondados com raio limitado pelo espaço disponível. `F1` abre configurações de grade, pontos de controle e curvas. `bancada.exe --capture` gera `build/bancada-capture.bmp` com uma cena reproduzível para revisão visual; `--capture-settings` inclui o painel aberto.

Em `F1`, escolha uma das seis cores para os próximos fios. Para mudar um fio existente, clique no seu traçado, abra `F1` e selecione a cor; a alteração pode ser desfeita e é salva no projeto. Clique numa área vazia para deixar de selecionar o fio. A cor é apenas visual e não altera a simulação.

Para compilar e testar com GCC e SDL3 do MSYS2 UCRT64 instalados:

```powershell
./build.ps1
./build/bancada.exe
```

Se `bancada.exe` estiver aberto, `./build.ps1 -OutputName bancada-next` compila uma cópia sem fechar a janela existente.

Escolha **Fonte**, **Interruptor**, **LED**, **Breadboard**, **Resistor**, **Capacitor**, **Indutor** ou **Porta AND** na biblioteca e clique na bancada para posicionar. Arraste um componente com o botão esquerdo para movê-lo e pressione `R` para girá-lo em 90°; a placa ainda não gira. Clique em dois terminais ou furos para conectar um fio; clique no mesmo par para removê-lo. Dois cliques na fonte ou no interruptor alternam seu estado. `Espaço` pausa/retoma, `N` avança um passo quando pausado e `Delete` remove o componente selecionado e seus fios.

Pressione `S` para salvar componentes, estados, fios, joelhos e configurações em `bancada.elec` na pasta de execução; salvar novamente substitui esse arquivo. `L` carrega a bancada; arquivos inválidos preservam a montagem aberta. O formato da bancada é independente do protótipo antigo. Ao carregar, o estado do LED é recalculado mesmo se a simulação estiver pausada.

Na bancada, `P` executa uma análise de ponto DC no ngspice isolado em `build/spice-local/ucrt64` e mostra a corrente da primeira fonte adicionada na barra inferior, com a convenção de sinal do SPICE. Os LEDs são desenhados acesos se sua corrente direta calculada atingir 1 µA; esse limiar é apenas uma aproximação visual. A análise ocorre em uma tarefa separada; se a montagem mudar durante ou após o cálculo, o resultado é descartado. Ao fechar o programa durante uma análise, pode haver uma espera de até dez segundos. A instalação isolada é uma dependência de desenvolvimento ainda não empacotada: se estiver ausente, aparece uma mensagem de erro. A análise aceita fontes ideais, resistores, capacitores, indutores, interruptores e LEDs genéricos. A sondagem transitória é descrita ao final deste documento.

Para preparar o ngspice de desenvolvimento com MSYS2 UCRT64 instalado em `C:\msys64`, execute `./tools/setup-ngspice.ps1` na raiz. O script baixa os pacotes disponíveis no catálogo do pacman, verifica sua integridade e extrai uma cópia em `build/spice-local`; ele preserva um destino que já exista. Depois, execute `./tools/test-ngspice.ps1` e `./tools/test-workbench-ngspice.ps1`. A preparação atual requer caminho sem espaços e não fixa a versão dos pacotes, portanto não é ainda um instalador de distribuição reproduzível.

Selecione uma fonte, resistor, capacitor ou indutor e pressione `E` para digitar seu valor em unidades SI, aceitando ponto ou vírgula decimal e notação científica (por exemplo, `2.2e-6` para 2,2 µF). `Enter` confirma e `Esc` cancela. Também é possível usar `+`/`-` para dobrar ou reduzir à metade. O painel mostra a unidade SI; os padrões são 5 V, 1 kΩ, 1 µF e 10 mH. Os valores entre 10⁻¹² e 10¹² são salvos no projeto; arquivos das versões anteriores recebem esses padrões ao serem carregados.

Após calcular, selecione um resistor ou LED para inspecionar sua corrente individual no canto superior direito. O sinal positivo representa corrente do primeiro terminal para o segundo; antes de girar a peça, eles estão respectivamente à esquerda e à direita. O negativo indica o sentido oposto.

Para visualizar corrente nos fios, abra `F1`, ative **Visualizar corrente nos fios** e pressione `P`. Os fios com corrente calculada acima de 1 µA recebem um realce e um marcador no sentido convencional; a velocidade do marcador é ilustrativa. O modelo usa sensores ideais de 0 V. Laços de fios ideais podem tornar a divisão de corrente indeterminada: nesses casos, a análise instrumentada é recusada e uma mensagem é exibida. Desative a opção para voltar à análise sem medição por fio.

Sem uma análise DC válida, a simulação antiga é discreta: o LED acende quando seu terminal esquerdo alcança o positivo de uma fonte ligada e o direito alcança o negativo por fios e interruptores fechados. Se houver resistor, capacitor ou indutor, a interface indica **cálculo indisponível** e mostra o LED em cinza até uma análise DC bem-sucedida. A breadboard tem 420 furos: 30 colunas de cinco contatos em cada lado e quatro trilhos de alimentação, cada um com 30 contatos conectados entre si. Os dois lados e os quatro trilhos são independentes. As pontas metálicas dos fios são mostradas sobre os furos. Ao soltar uma peça próxima de um furo, seus terminais são alinhados à placa quando possível; terminais encaixados aparecem em verde e entram na conectividade. Mover a peça ou a placa rompe o contato. O encaixe é uma aproximação geométrica: a placa ainda não gira e não há verificação física de ocupação dos furos; placas sobrepostas não são suportadas. Fontes em curto não acendem LEDs no modelo discreto. A análise SPICE usa parâmetros genéricos e não deve ser usada para dimensionar uma montagem física. O desenho usa formas originais provisórias, ainda não modelos finais. Os projetos antigos não são alterados.

Na bancada experimental, `Ctrl+Z` desfaz e `Ctrl+Y` refaz até 24 alterações por sessão. O histórico inclui componentes, fios, joelhos, valores, estados e carregamento; não é salvo no projeto. A roda do mouse ajusta o zoom ao redor do cursor; arraste com o botão central para deslocar a vista e use `Home` para restaurá-la. A posição e o zoom da vista ainda não são salvos.

As placas recém-criadas têm furos mais espaçados. Placas carregadas de projetos anteriores mantêm a geometria original para preservar os contatos já montados.

Para incluir um teste automatizado dos eventos de janela (componentes, fios, joelhos, desfazer/refazer, zoom e deslocamento), execute `./build.ps1 -OutputName bancada-next -GuiTests`. Esse teste requer uma sessão gráfica Windows com OpenGL; não substitui uma avaliação manual completa da interface.

## Protótipo anterior

O primeiro incremento é um circuito fixo de fonte, interruptor e LED em console. Use `i` para alternar o interruptor, `f` para alternar a fonte e `q` para sair. É um modelo booleano didático, sem cálculo elétrico ou edição de conexões; não representa precisão de SPICE.

O protótipo anterior de circuito fixo permanece disponível para comparação. Para recompilá-lo, use:

```powershell
./build.ps1 -Legacy
./build/window.exe
```

A DLL SDL3 deve estar no PATH ou junto do executável. Pressione `S` para salvar posições e estados em `circuito.elec` na pasta em que o programa foi iniciado; salvar novamente substitui esse arquivo. `L` carrega esse arquivo e informa o resultado no título. O formato é versionado e arquivos inválidos não alteram o circuito aberto. Os pontos possíveis de ligação são fixos; a vista de zoom não é salva. A interface de texto é básica e a apresentação 2.5D ainda está em desenvolvimento.

Use `Ctrl+Z` e `Ctrl+Y` para desfazer e refazer mudanças de posição, estados e carregamento. O histórico mantém até 32 etapas na sessão e não é salvo no arquivo.

Pressione `Espaço` para pausar ou retomar a simulação. Ao pausar, o LED mantém o último resultado mesmo que o circuito seja editado; `N` executa um passo e, ao retomar, o resultado volta a ser atualizado. O estado de execução não é salvo. Pressione `D` para listar na barra de título os estados observados da montagem (fonte, interruptor e ligações); quando a simulação está pausada, eles podem ser diferentes do resultado congelado do LED. Essa inspeção não identifica falhas elétricas além do modelo simplificado.

Clique em dois terminais brancos para conectar ou desconectar o par; o primeiro fica amarelo. Os três pares possíveis são fonte–interruptor, interruptor–LED e retorno entre LED e fonte. Clique novamente no terminal ou pressione `Esc` para cancelar; pares diferentes dos previstos são rejeitados. As teclas `1`, `2` e `3` também alternam essas ligações. O LED só acende com todas conectadas. Essas alterações podem ser desfeitas e são salvas no projeto. Ligações arbitrárias ainda não são suportadas.

Opcionalmente, o script `tools/codex-stop-notify.ps1` exibe um lembrete local quando invocado por um hook `Stop` do Codex. A configuração é local em `.codex/hooks.json` e não é versionada; hooks não gerenciados precisam ser revisados e aceitos no Codex antes de funcionarem. O aviso depende das notificações do Windows e não envia mensagens automaticamente.

A política de branches, versões e recuperação está em [VERSIONAMENTO.md](VERSIONAMENTO.md); o histórico está em [CHANGELOG.md](CHANGELOG.md).

Com GCC disponível no PowerShell, compile e execute a partir da raiz:

```powershell
New-Item -ItemType Directory -Force build
gcc -std=c11 -Wall -Wextra -Werror src/main.c src/circuit.c -o build/electronics.exe
./build/electronics.exe
```

Para testar o modelo:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_circuit.c src/circuit.c -o build/test_circuit.exe
./build/test_circuit.exe
```

Selecione um componente de dois terminais e pressione `T` para calcular, em segundo plano, a tensão de um terminal em relação ao negativo da primeira fonte ou a corrente do componente. Em `F1`, escolha tensão ou corrente, escolha o terminal para tensão e clique nos campos para ajustar o passo solicitado e a duração em segundos; os padrões são 10 µs e 5 ms. A duração dividida pelo passo não pode exceder 500. Capacitores e indutores iniciam com condições nulas. A bancada pausa ao concluir; `Espaço` reproduz ou pausa a curva calculada, `N` avança e `B` volta pelas amostras calculadas pelo ngspice; clique no gráfico para selecionar uma amostra e use `G` para ocultar ou mostrar o gráfico. Os intervalos podem variar por causa do passo adaptativo. Alterar a montagem descarta a série. Os ajustes do menu são salvos no projeto; ainda falta o controle completo da simulação tick a tick. A corrente tem sinal positivo do pino 1 para o pino 2 e está disponível em fontes, resistores, capacitores, indutores e LEDs; interruptores ideais não oferecem essa medição. Para um fio, selecione seu trajeto e pressione `T`: o gráfico mostra corrente positiva de A para B, na ordem em que o fio foi criado. Em F1, ative a visualização de corrente antes de calcular para realçar os fios cuja corrente calculada ultrapassar 1 µA; as setas indicam o sentido em cada amostra. Ciclos de fios ideais têm corrente indeterminada e são rejeitados.

A comparação de alternativas digitais e a prova de conceito XSPICE estão em [LOGICA_DIGITAL.md](LOGICA_DIGITAL.md). Após preparar o runtime de desenvolvimento, execute `./tools/test-xspice-gates.ps1`; AND, NAND, OR, XOR e NOT estão disponíveis como blocos didáticos na bancada.

A porta AND didática tem entradas nos terminais 1 e 2 e saída no terminal 3. Usa níveis fixos de 0 e 5 V, limiares de entrada de 1,5 e 3,5 V e saída indefinida de 2,5 V; não tem pinos de alimentação e não representa um CI comercial. Para observar a saída, selecione a porta, escolha tensão e terminal 3 em F1 e pressione T. Os cálculos dependem do runtime XSPICE.

Pressione `Tab` ou clique no título da biblioteca para alternar para as portas AND, NAND, OR, XOR e NOT. As portas de duas entradas usam terminais 1 e 2 de entrada e terminal 3 de saída; a NOT usa terminal 1 de entrada e 2 de saída. Esses blocos têm os mesmos níveis didáticos fixos da AND, sem pinos de alimentação.

Na página digital, o **Clock** gera uma tensão ideal entre o terminal 1 (saída) e o 2 (referência): onda periódica de 0 a 5 V, aproximadamente 50% de ciclo ativo, iniciando em 0 V. O padrão é 1 kHz; selecione e pressione `E` para editar a frequência em Hz (por exemplo, `1e6` para 1 MHz). Dois cliques ligam/desligam; desligado, o componente fornece 0 V. Em F1, ajuste passo e duração para enxergar períodos suficientes no gráfico. A sondagem tem limite de amostras e ainda não equivale a um clock interativo em tempo real.

A reprodução percorre o intervalo completo da curva em cinco segundos de relógio, independentemente da duração simulada. Ela mostra a última amostra disponível em cada instante e para no fim; ao retomar do fim, reinicia. É uma reprodução de resultados já calculados, sem recalcular ou atualizar todos os componentes da bancada.

Durante a sondagem e a reprodução transitória, os LEDs da montagem acompanham a corrente direta calculada na mesma amostra; o limiar visual é 1 µA. Isso também funciona ao percorrer a curva com `B` e `N`. Os demais estados de componentes ainda não são atualizados simultaneamente.

Com **Visualizar corrente nos fios** ativado em F1 antes de pressionar `T`, a sondagem transiente mede todos os fios na mesma execução. O realce e as setas acompanham a amostra mostrada, inclusive na reprodução. A cor e a largura indicam três faixas: 1 a menos de 100 µA, 100 µA a menos de 10 mA e 10 mA ou mais. Abaixo de 1 µA não há realce; o gráfico de um fio selecionado mostra o valor. Caminhos redundantes de fios ideais continuam sendo recusados quando não há divisão de corrente única.

O **Bloco DFF** na página digital é um flip-flop D didático: D recebe o dado, C recebe o clock e Q apresenta o estado retido. Q é atualizado na borda de subida de C; para sondá-lo, selecione o terminal 3 em F1. O modelo solicita estado inicial zero, mas bordas simultâneas ou entradas indefinidas podem produzir estado indefinido (2,5 V no modelo). Não há pinos de alimentação, set/reset ou saída complementar, e os limites de temporização de um CI real não são modelados.

Na página digital, o **CI 74HC00** é um modelo didático com encapsulamento DIP de 14 pinos e quatro portas NAND. Pinos: 1A=1, 1B=2, 1Y=3; 2A=4, 2B=5, 2Y=6; GND=7; 3Y=8, 3A=9, 3B=10; 4Y=11, 4A=12, 4B=13, VCC=14. Encaixe-o sobre a divisão central da breadboard e conecte VCC e GND. Selecione o CI e use F1 para escolher um dos 14 terminais para sondagem.

A simulação é uma aproximação própria, válida apenas para alimentação entre 4,5 e 5,5 V: abaixo de 1,5 V a entrada é baixa, acima de 3,5 V é alta e regiões intermediárias sem resultado definido deixam a saída em alta impedância. As saídas usam resistência idealizada de 50 Ω, e sem alimentação ficam em alta impedância. Conecte entradas não usadas a um nível conhecido. Consumo interno, atrasos, proteções e limites reais não são modelados; não use o modelo para dimensionamento. A pinagem segue a [folha de dados da TI](https://www.ti.com/lit/ds/symlink/sn74hc00.pdf).

Pressione **F2** ou clique em **Exemplos** no topo para alternar entre duas montagens prontas: LED com resistor encaixado na breadboard e flip-flop D com dois clocks e LED. No primeiro exemplo, pressione `P`; no segundo, `T` calcula a saída Q, e `Espaço`, `B` e `N` controlam a reprodução. `Ctrl+Z` recupera a montagem anterior. Ao abrir um exemplo, a sondagem volta a 10 µs / 5 ms e escolhe a tensão indicada; desfazer restaura a montagem, mas não as configurações. Os exemplos dependem do runtime ngspice de desenvolvimento.


Integracao de qualidade: `./build.ps1 -OutputName bancada-quality -GuiTests` gera `build/bancada-quality.exe`. O botao executar/pausar controla o calculo automatico apos edicoes. F2 percorre tres exemplos, incluindo 74HC00 na breadboard. Indicacao de queima e uma heuristica reversivel apenas em circuitos estaticos; nao modela aquecimento nem dano fisico. Transientes continuam pre-calculados em lote. Veja `QUALIDADE_CODIGO.md` e `CONTINUIDADE.md` para arquitetura e proximos passos.

Na versao 0.17.1, use `./build.ps1 -GuiTests` e abra **`build/bancada.exe`**. `tools/setup-ngspice.ps1` prepara tambem as DLLs do solver. `./tools/test-runtime-isolated.ps1` verifica a simulacao com PATH minimo; isso nao substitui o empacotamento final.

### Organizacao interna (0.17.4)

O ciclo da janela chama entrada, atualizacao e desenho em modulos separados. `app_input.c`
despacha interacoes; `app_commands.c` executa atalhos de edicao; `app_render.c` le o estado.
`sim_state.c` invalida resultados, avanca a reproducao e calcula a apresentacao dos LEDs sem
SDL/OpenGL. `test_sim_state.exe` testa essas regras com tempo fornecido pelo teste, sem solver.
A reproducao continua sendo de curvas calculadas em lote, com os limites eletricos documentados acima.


Na 0.17.5, a validade de resultados usa um unico tipo (DC ou transiente) e revisoes da montagem.
Desfazer/refazer e carregar criam novas revisoes: resultados de tarefas anteriores sao descartados,
mesmo quando o conteudo e restaurado. As revisoes sao apenas de runtime e nao mudam o formato salvo.
A verificacao de mudancas nao percorre mais a montagem inteira por quadro. Testes de revisoes e
transicoes integram ./build.ps1; o modelo continua sem dependencia de janela ou renderer.


### Slots e edicao de conjuntos (0.18.0)

F3 abre nove slots de montagem e configuracoes. Clique num slot (ou pressione 1-9), depois
Salvar/Carregar; Ctrl+S e Ctrl+L operam o slot escolhido. Os arquivos ficam em saves/slot-N.elec,
relativos a pasta de execucao. S/L continuam usando bancada.elec. Carregar valida o arquivo antes
de substituir a montagem; a gravacao usa arquivo temporario e substituicao atomica ja existentes.
Estes slots nao guardam o estado interno do ngspice nem o instante de uma simulacao em andamento.

Arraste numa area vazia, ou Shift+arraste sobre a bancada, para selecionar por retangulo.
Entram os componentes cujos centros estao dentro da area. Ctrl+A seleciona tudo; Ctrl+C copia,
Ctrl+X recorta, Ctrl+V cola perto do mouse e Delete remove. A copia inclui somente fios cujas duas
pontas estao na selecao, preservando cores e joelhos. Arrastar um componente selecionado move o
conjunto e os joelhos dos fios internos. Desfazer/refazer agrupa cada operacao. Os limites atuais
continuam 64 componentes/128 fios; colagem que nao cabe falha sem alterar a montagem.

A biblioteca possui abas Basicos/Digital e miniaturas desenhadas a partir das pecas reais.
A versao 0.19.0 acrescenta o packing descrito abaixo; copiar um conjunto continua sendo uma
operacao separada de encapsular seus componentes.


### Blocos reutilizaveis / packing (0.19.0)

Selecione as pecas e pressione **Ctrl+G**. Elas passam a aparecer como uma unica peca compacta,
com terminais P1, P2 etc. Sao expostos os terminais com ligacoes para fora da selecao e os pinos
livres dos componentes (furos livres de breadboard nao viram centenas de portas). Para escolher
uma fronteira especifica, ligue os terminais desejados a componentes externos antes de encapsular.
Os terminais sao eletricos: sua funcao de entrada, saida ou alimentacao depende do circuito interno.

O bloco e salvo automaticamente no primeiro dos oito slots livres em `blocks/block-N.elec`.
Na aba **Blocos**, clique na miniatura e depois na bancada para colocar uma instancia independente.
As instancias possuem pecas, valores e conexoes proprios. Ctrl+C/V tambem duplica blocos.
Arrastar move o conjunto; Delete remove; Ctrl+Z/Y desfaz/refaz. **Ctrl+Shift+G** restaura o interior
para editar. Recriar um bloco modificado gera outra entrada da biblioteca; nao atualiza instancias
antigas automaticamente. Salvar um projeto ou slot preserva os blocos e seus terminais.

O modelo eletrico mantem os componentes originais. Contatos implicitos que atravessam a fronteira
com breadboards sao convertidos em fios explicitos; breadboards internas de instancias diferentes
ficam isoladas mesmo se seus layouts ocultos coincidirem. Conexoes externas dos blocos usam fios;
encaixe automatico dos terminais compactos em breadboards ainda nao esta implementado.

Primeira versao: um nivel de encapsulamento. Abra blocos existentes antes de combinar seus interiores.
O limite atual de 64 pecas/128 fios conta os componentes internos, e a biblioteca possui oito slots.
Falhas por capacidade preservam a montagem e exibem a causa. Os arquivos novos usam formato 16,
com grupos/membros/portas; formatos 1-15 continuam legiveis. A migracao para IDs estaveis de tipos
continua pendente e exigira outra versao de formato.

Validacao: `./build.ps1 -GuiTests` inclui testes de packing com ngspice quando o runtime esta instalado,
comparando DC e transiente RC antes/depois, contatos de fronteira e instancias independentes.
O autoteste GUI possui 100 fases. `--capture-packed` gera uma cena para revisao visual.


### Identificacao dos pinos e registro (0.20.0)

Selecione uma peca para ver os rotulos dos terminais e aponte o mouse para um pino para ver seu
nome e funcao. IN/I = entrada, OUT/O = saida, CLK/C = entrada de clock, V+/+ = alimentacao positiva,
V- = retorno da fonte, G/GND = terra, A/K = anodo/catodo e IO/B = terminal passivo/bidirecional.
Contatos de breadboard nao tem polaridade propria. Essas identificacoes descrevem o componente;
nao sao leituras da tensao atual do no. O 74HC00 usa nomes/numero de pinos da ficha TI indicada em DECISOES.md.

Blocos usam circulos de terminal maiores e espaco entre portas aumentado. A funcao mostrada em
cada porta vem do componente interno ligado a ela; um terminal de resistor, por exemplo, continua
bidirecional. Classificacao personalizada de portas pertence ao editor de chip ainda pendente.
Ctrl+Shift+G esta rotulado como desempacotar na bancada, pois altera a montagem atual.
O futuro editor isolado, renomeacao e instrumentos seguem os requisitos de QUALIDADE_DE_USO.md.

component_registry.c concentra IDs estaveis, nomes, numero de pinos, capacidades, valor primario,
unidades/faixas e funcao dos terminais. Os arquivos ainda usam o formato 16 com codigos numericos;
a leitura possui um mapeamento historico independente da ordem atual do enum. Geometria, emissao
SPICE e parametros completos de modelo ainda precisam migrar para descritores. Os limites numericos
do parametro primario foram preservados e nao representam limites fisicos de todos os dispositivos.

Build validado com a bancada antiga aberta: ./build.ps1 -OutputName bancada-next -GuiTests.
Nesse caso, a copia testada e build/bancada-next.exe; a sessao existente nao e fechada nem substituida.


### Interior de chips (0.21.0)
Selecione um bloco e use **Ver interior** (Enter). O editor isolado tem bancada, simulacao e
historico proprios. **Renomear** (F4) aceita acentos; **Salvar modelo** grava a biblioteca;
**Aplicar ao chip** altera somente a instancia aberta e permite Ctrl+Z na bancada externa.
**Voltar** (Esc) preserva a bancada e oferece salvar modelo, descartar ou cancelar se necessario.
Formato 17 conserva nomes e le formatos 1-16. Limites atuais: nomes de 127 bytes UTF-8, um nivel,
oito modelos, 64 pecas e 128 fios. Instrumentos com pontas ainda estao pendentes.


### Grid e alinhamento (0.22.0)
F1 abre Alinhamento e encaixe: espacamento configuravel de 1 a 10000 unidades da montagem,
encaixe na grade e na breadboard independentes. Alt suspende encaixe durante posicionamento e
arraste. Ocultar a grade apenas oculta linhas. Zoom muda a densidade visual, preservando snap.
Mover selecoes e chips preserva a geometria interna. Os furos da breadboard tem prioridade para
pecas individuais. Preferencias acompanham arquivos e slots no formato 18; leitura 1-17 mantida.
Ainda pendentes: escala fisica ajustavel, terminais flexiveis e insercao de joelho por duplo clique.
