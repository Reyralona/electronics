# Decisões e próximos passos

## Geometria e encaixe (24/11/2026)

Fatos, hipóteses e fontes oficiais consultadas nesta data: [comparação de geometria](COMPARACAO_GEOMETRIA_2026_11_24.md). Decisão: permitir 45° como orientação adicional salva no formato 39, manter os pinos calculados pelo modelo como fonte do contato, alinhar pontas próximas aos furos após encaixe e quantizar a largura de chips redimensionados quando o encaixe na protoboard estiver ativo. O espaçamento entre portas de chips não encolhe abaixo do passo físico; uma rotação diagonal de encapsulamento rígido pode impedir a inserção simultânea de todos os pinos numa malha ortogonal. Marcadores verdes são preferência visual independente do circuito.

Complemento de 24/11: a escala principal altera o corpo com proporção constante. A largura efetiva é quantizada ao passo de 14 da protoboard e determina o mesmo fator para a altura. O mínimo explícito da caixa é 20 × 8; projetos antigos com altura automática preservam 44 ou a altura necessária para suas portas. Portas não mudam de passo e o corpo não fica mais baixo que a coluna de pinos (revisto após captura com nomes e pinos fora do corpo). Altura e largura independentes permanecem apenas nos atalhos específicos. Revisão posterior: o passo das portas passou de 20 para 14 (`BENCH_DIP_PITCH`), porque 20 não coincide com as colunas da protoboard; a largura encaixada usa o passo do eixo atravessado pelos lados (14 em pé, 10 deitado) ao redimensionar; girar não altera o tamanho, para que quatro giros voltem ao original. Limite: supõe a protoboard atual (fileiras a 10); placas antigas com fileiras a 7 podem não alinhar. O nome usa fonte proporcional à área disponível.

> **Registro de decisões técnicas**, em ordem cronológica, com fatos, fontes e datas de consulta. Uma decisão posterior pode substituir uma anterior; o comportamento atual está no [guia](GUIA.md).

Consulta: 14/11/2026.

Fatos: a visão está em `VISAO.md`. GCC do MSYS2 UCRT64 foi encontrado neste computador; a presença no PATH não é pressuposta. A [Microsoft](https://learn.microsoft.com/en-us/windows/win32/learnwin32/prepare-your-development-environment) documenta suporte do Windows SDK a C e Windows x64. [Win32](https://learn.microsoft.com/en-us/cpp/windows) oferece APIs nativas de interface; [GDI](https://learn.microsoft.com/en-us/windows/win32/gdi/windows-gdi) oferece desenho e texto para C/C++. A [documentação SDL3 para Windows](https://wiki.libsdl.org/SDL3/README-windows) descreve compilação com MinGW-w64 e CMake.

Decisão do usuário: SDL e OpenGL. SDL3, sob [licença zlib](https://github.com/libsdl-org/SDL), fornece a janela e eventos; OpenGL desenha a cena. A primeira experiência usa OpenGL 1.1 para validar a integração, sem fixar a versão definitiva. No Windows, funções posteriores à 1.1 exigem carregamento em tempo de execução, conforme a [documentação SDL](https://wiki.libsdl.org/SDL3/SDL_GL_CreateContext). A interface acessível e a distribuição, incluindo a DLL e a licença SDL, precisam ser definidas antes de um lançamento. Win32/GDI permanece uma alternativa sem dependência gráfica externa, mas exigiria trabalho próprio de interface e desenho.

O protótipo original tinha topologia fixa. A partir da versão 0.12, uma bancada separada experimenta componentes e fios instanciáveis: o modelo de conectividade fica em `workbench.c`, independente de SDL e OpenGL. Ligações definem redes, interruptores fechados unem redes e um LED responde à polaridade relativa de uma fonte ligada. Esse modelo não calcula tensão, corrente ou resistência, nem representa o comportamento de LEDs em série ou redes com fontes em conflito. Não é SPICE; diagnósticos elétricos e uso para dimensionamento físico ficam fora do escopo.

Direção reafirmada: bancada 2.5D própria, biblioteca de componentes, fios definidos pelo usuário e breadboard com conectividade real dos contatos. Na versão 0.13, a breadboard representa 20 grupos isolados de cinco furos, sem trilhos ou encaixe direto de componentes; conexões aos furos usam fios. Persistência versionada e tutorial ainda são necessários. Os desenhos são provisórios e não reproduzem assets das referências. O protótipo antigo e seus projetos ficam preservados; a nova bancada ainda não lê esses projetos. São pendentes a versão definitiva do OpenGL, a versão mínima do Windows, a distribuição, a licença do projeto e a extensão do modelo elétrico.

Investigação em 14/11/2026: pacote MSYS2 UCRT64 ngspice 45.2 foi baixado com verificação de integridade do pacman e extraído isoladamente em `build/spice-local`. O arquivo `spinit` do pacote aponta para `/ucrt64/lib/ngspice`; para esse experimento, seus caminhos foram ajustados na cópia extraída para o diretório local. Um resistor de 1 kΩ alimentado por 5 V retornou 5 V e 5 mA. `tools/test-ngspice.ps1` reproduz a verificação após a preparação do runtime. Isso não integra o simulador à bancada nem valida modelos digitais, transientes ou distribuição. A [interface compartilhada oficial](https://ngspice.sourceforge.io/shared.html) e [XSPICE](https://ngspice.sourceforge.io/xspice.html) continuam candidatos a avaliar; consulte as licenças e os requisitos de distribuição antes de empacotar.

`tools/setup-ngspice.ps1` automatiza a preparação local usando o catálogo e a verificação de pacotes do pacman, sem substituir destinos existentes. Testado com ngspice UCRT64 45.2 em uma pasta nova: referências DC, RC, RL e DLL passaram. O catálogo não está travado em uma versão e a preparação ainda exige caminho sem espaços; não é um pacote de distribuição nem valida todos os componentes da licença ou modelos digitais.

O experimento `tools/test-ngspice-shared.ps1` carrega a DLL dinamicamente, usa o cabeçalho oficial extraído e confirma 5 V e -5 mA no vetor de corrente da fonte. Os valores dos vetores são copiados imediatamente: a estrutura retornada é reutilizada em chamadas seguintes. `SPICE_LIB_DIR` aponta para `share/ngspice` na instalação isolada. A experiência é executada em processo próprio; segurança de threads, isolamento de múltiplas simulações e recuperação de erros ainda precisam ser investigados.

O mesmo teste carrega um circuito RC de 1 kΩ e 1 µF com capacitor inicialmente descarregado, executa análise transitória e compara uma amostra próxima de 1 ms com a solução analítica de carga. Os vetores de tempo são copiados antes de consultar a tensão e sua ordem é verificada. Isso valida apenas uma análise completa de referência: não há ainda execução interativa por tick, controle de clock ou garantia de passo interno fixo.

Na versão 45.2 testada, o experimento pela DLL também configura `stop when time = 1m`, `stop when time = 2m`, executa o transitório e usa `resume` para chegar ao segundo ponto preservando o estado do capacitor. `step 1` avança um ponto interno adaptativo; não é um tick de duração fixa. Os resultados são verificados contra a solução analítica. É um experimento síncrono em processo isolado, não uma integração da interface. Consulte o [manual do ngspice](https://ngspice.sourceforge.io/docs/ngspice-44-manual.pdf) para semântica de `stop`, `resume` e `step`.

O tradutor experimental `netlist_write_dc` transforma conexões de fios e grupos de breadboard em nós SPICE. Suporta uma fonte ideal fixa de 5 V, resistores fixos de 1 kΩ, interruptores ideais e LED modelado como diodo genérico (IS=1e-18, N=2); rejeita outros componentes e fontes múltiplas. `tools/test-workbench-ngspice.ps1` verifica bancada → netlist → ngspice, inclusive LED polarizado nos dois sentidos. Os parâmetros do diodo são ilustrativos, não dados de um componente real. Na branch experimental, a tecla `P` executa ngspice em processo separado e apresenta a corrente da fonte, resistores e LEDs; rejeita erros, avisos, resultados ausentes e prazos excedidos. A tarefa roda fora da thread da interface e descarta resultados de uma montagem modificada; ao fechar, o programa aguarda a conclusão ou o prazo de dez segundos. Depende da instalação local em `build/spice-local`; a análise é pontual e não substitui a simulação simplificada em tempo real. Valores editáveis, limites elétricos e resultados por fio permanecem pendentes.

O modelo experimental também representa capacitores fixos de 1 µF e exporta análise transitória com condição inicial zero e parâmetros explícitos de passo de saída e tempo final. Um teste constrói RC no modelo de bancada, executa ngspice e compara uma amostra próxima de 1 ms à solução analítica. A branch experimental oferece capacitores e resistores na interface; sem uma análise DC válida, indica cálculo indisponível. O passo solicitado não garante passo interno fixo do solucionador nem avanço interativo.

Na análise DC experimental, correntes de LEDs são consultadas individualmente por `@dN[id]`; resultado incompleto é rejeitado. O LED aparece aceso quando a corrente direta calculada é pelo menos 1 µA. Esse limiar é uma convenção visual provisória, não um modelo de luminosidade validado. Resultados não são usados após mudança da montagem.

Com a visualização de corrente ativada, cada fio é representado por uma fonte ideal de 0 V, cuja corrente assinada é lida do ngspice. A união prévia de contatos da breadboard e interruptores fechados permite detectar ciclos de ligações ideais, nos quais as correntes individuais seriam indeterminadas; tais casos são recusados em vez de exibir um valor arbitrário. O realce só aparece após uma análise DC válida e acima de 1 µA; o marcador mostra sentido convencional, com velocidade visual sem significado físico. Isso não mede correntes internas da breadboard nem fornece análise transitória.

O indutor experimental tem valor fixo de 10 mH e corrente inicial zero no transitório. Um circuito RL de 1 kΩ é exportado pelo modelo e comparado com a resposta analítica em `tools/test-workbench-ngspice.ps1`. O formato de bancada passou à versão 3 para registrá-lo; a leitura de versões 1 e 2 é mantida. O teste transitório ainda é externo à interface.

O encaixe experimental alinha o terminal mais próximo a um furo e reconhece como contato cada terminal geometricamente coincidente. A conectividade é compartilhada pelo modelo discreto e pelo tradutor SPICE e pode ser reconstruída após salvar ou desfazer. Mover a peça ou a placa desfaz o contato. Ainda não há rotação da placa, colisão, controle de ocupação nem suporte para placas sobrepostas; a escolha de contatos em placas sobrepostas é apenas a primeira encontrada.

Componentes de dois terminais podem ser girados em incrementos de 90°. A geometria dos terminais e fios acompanha a rotação; o encaixe é recalculado e a alteração integra o histórico. O formato 8 registra a orientação; versões anteriores usam orientação zero. O espaçamento fixo das pernas não garante que ambos os terminais encaixem após girar; apenas contatos efetivamente alinhados são conectados.

A breadboard foi ampliada de 100 para 180 furos (36 grupos independentes de cinco). Os 100 índices e posições originais foram preservados, e os novos grupos ficam nas extremidades. O formato de bancada 5 distingue projetos com novos furos; formatos 1 a 4 permanecem legíveis. As pontas dos fios são desenhadas sobre a placa com trecho metálico, sem mudar a conectividade elétrica.

Ajuste posterior de escala: uma [captura pública de projeto do Tinkercad](https://makeabilitylab.github.io/physcomp/electronics/assets/images/Tinkercad_ShowingInterfaceWithBasicLEDCircuitOnBreadboard.png), consultada em 14/11/2026, foi usada como referência de proporção, sem reutilizar seus assets. A placa agora tem 30 colunas por lado e quatro trilhos de 30 furos, totalizando 420 contatos; cada trilho é independente. Os 180 índices e posições anteriores permanecem iguais. O formato 7 registra os trilhos e mantém leitura das versões anteriores. O corpo do resistor e o LED foram reduzidos em relação à placa; a geometria das pernas anteriores foi preservada.

No formato 9, placas novas usam espaçamento vertical de 10 unidades entre furos, em vez de 7; projetos das versões 1 a 8 mantêm a geometria antiga para preservar encaixes. O formato também registra uma de seis cores por fio, sem afetar a conectividade elétrica.

Na branch experimental, fonte, resistor, capacitor e indutor têm valores editáveis em unidades SI, com limites de 10⁻¹² a 10¹²; os padrões são 5 V, 1 kΩ, 1 µF e 10 mH. O formato de bancada 4 os armazena em precisão dupla; versões 1 a 3 recebem os padrões. Testes incluem ida e volta, migração e simulação de 10 V em 2 kΩ. O limite numérico não substitui limites físicos por componente.

A análise SPICE experimental aceita múltiplas fontes ideais; a primeira define a referência de tensão e sua corrente aparece na barra inferior. Uma fonte desligada é modelada como fonte ideal de 0 V. Curtos por fios, contatos ou interruptores em qualquer fonte são rejeitados; conflitos entre fontes ideais dependem do diagnóstico do ngspice. Duas fontes de 5 V em série com 1 kΩ resultam em 10 mA, com e sem instrumentação de fios.

A sondagem transitória experimental (`T`, depois `N`) executa ngspice fora da thread gráfica e lê a tensão do terminal escolhido. O menu F1 ajusta passo e duração, limitados a 500 intervalos solicitados; as amostras usam passo adaptativo. A montagem alterada invalida o resultado. Capacitores e indutores iniciam com condições nulas. Os testes comparam RC e RL com soluções analíticas e verificam execução em segundo plano. Ainda faltam estados completos e passo de simulação controlado.

A tensão transitória medida aparece em um gráfico com escala de tempo real das amostras adaptativas. `B` e `N` percorrem os pontos, e clicar no gráfico seleciona a amostra temporal mais próxima. A curva representa apenas o terminal sondado, sem alterar estados elétricos da bancada.

A sondagem transitória também pode selecionar a corrente de fonte, resistor, capacitor, indutor ou LED, usando vetores de dispositivo salvos pelo ngspice. O sinal segue o pino 1 para o 2; a corrente da fonte que entrega energia normalmente é negativa. Interruptores ideais não são instrumentados. Testes comparam correntes RC e RL com soluções analíticas e verificam LED e fonte.

A corrente transitória de um fio selecionado usa um sensor ideal de 0 V no netlist. O sentido positivo é da extremidade A para B, conforme a ordem de criação; o gráfico e os marcadores identificam esse sentido. Com a opção de corrente habilitada, o fio é realçado apenas quando a amostra calculada supera 1 µA em módulo. Caminhos ideais redundantes são rejeitados como ambíguos; ainda não se calculam simultaneamente todas as correntes transitórias dos fios.

O formato de projeto 10 armazena configurações visuais, cor padrão dos fios e parâmetros de sondagem. Arquivos anteriores recebem padrões; configurações inválidas rejeitam o arquivo inteiro sem alterar a bancada ou as opções atuais. Desfazer/refazer continua cobrindo somente a montagem, não alterações de configuração.

A primeira porta AND didática usa três terminais (duas entradas e saída) e modelos XSPICE com pontes ADC/DAC fixas de 0/5 V; a saída indefinida é 2,5 V. Não há pinos de alimentação nem modelo de CI comercial. Testes da bancada verificam as quatro entradas em DC e transiente com resistor e LED. O formato 11 registra o novo tipo sem reinterpretar arquivos antigos.

As portas didáticas NAND, OR, XOR e NOT usam a mesma interface ADC/DAC fixa da AND e modelos digitais XSPICE. Todas as tabelas de verdade foram exercitadas na bancada em DC e transiente com carga LED; AND seguida de NOT também foi testada. O formato 12 distingue os novos tipos, e Tab alterna as páginas da biblioteca. Alimentação, conflitos de saída e CIs reais permanecem pendentes.

O clock didático usa fonte ideal PULSE de 0/5 V, com frequência editável em Hz, ciclo ativo próximo de 50% e bordas de 0,1% do período. Desligado, fornece 0 V. Na ausência de fonte DC, o terminal 2 do primeiro clock serve como referência do netlist; na presença de fonte DC, a referência continua sendo seu terminal negativo. Os testes verificam 1 kHz, 2 kHz, 1 MHz e inversão pela porta NOT. Forma e parâmetros de PULSE são documentados no [tutorial oficial ngspice](https://ngspice.sourceforge.io/ngspice-tutorial.html), consultado em 14/11/2026. O formato 13 registra o novo componente.

A sondagem transitória permite reproduzir resultados pré-calculados em cinco segundos reais por intervalo completo, usando os tempos reais das amostras adaptativas. Espaço pausa/retoma e o último ponto pausa automaticamente; B/N continuam percorrendo amostras. A reprodução não altera os estados elétricos dos componentes nem constitui simulação em tempo real.

A sondagem transitória registra correntes de todos os LEDs na mesma execução ngspice; tabelas com tempos ou quantidades divergentes são rejeitadas. O desenho usa corrente direta de pelo menos 1 µA em cada amostra. Um circuito de clock e NOT com LEDs opostos verifica sincronização, e o autoteste gráfico verifica a alternância. A leitura é limitada a 16 MiB de saída e 2048 amostras; resultados maiores são rejeitados, não truncados silenciosamente.

Quando a visualização de corrente está ativada, a análise transitória instrumenta todos os fios com sensores ideais e registra, por amostra, sentido e presença de corrente acima de 1 µA. A interface realça os trechos e mostra setas no sentido calculado; intensidade visual ainda não é escala de magnitude. O parser valida a sincronização de cada vetor; ciclos ideais ambíguos são recusados. Os limites de saída e de amostras permanecem aplicáveis.

A visualização de corrente, tanto DC quanto transitória, distingue as faixas de 1 a menos de 100 µA, 100 µA a menos de 10 mA e 10 mA ou mais por largura e cor. Abaixo de 1 µA não há realce. A escala é discreta, não substitui a leitura numérica; sinais e faixas transitórios são compactados por amostra.

O flip-flop D didático de três terminais (D, C, Q) usa `d_dff` do XSPICE, com set/reset e saída complementar não conectados, estado inicial solicitado zero e pontes didáticas 0/5 V. A prova com dois clocks e LED verifica captura na borda e retenção após mudança de D; entradas indefinidas podem produzir Q indefinido. O modelo não representa alimentação nem temporização de CI real. O formato 14 registra o componente.

O CI 74HC00 didático tem 14 contatos na disposição DIP da folha de dados TI e encaixa atravessando o vão da breadboard. Sua aproximação própria modela quatro NAND, alimentação de 4,5?5,5 V e saídas push-pull resistivas de 50 Ω. Fora da faixa ou com resultado indeterminado, as chaves desligam; o modelo usa fugas numéricas e não modela consumo interno, atrasos nem proteções. Testes cobrem estados, carga, alimentação, entrada intermediária e 14 contatos. O formato 15 registra o novo tipo.

Dois exemplos reproduzíveis são abertos por F2: LED com resistor na breadboard e flip-flop D com dois clocks. A substituição da montagem entra no histórico; os parâmetros de sondagem são ajustados para o exemplo e não participam do desfazer. `test_workbench_examples.exe` verifica a construção e, com o caminho do ngspice como argumento, os resultados elétricos; o autoteste gráfico cobre F2 e desfazer.


Integracao de 14/11/2026: adotada a revisao QUALIDADE_CODIGO.md como direcao incremental. O trabalho local do Claude foi preservado em outra branch e integrado com testes. Calculo automatico por processo isolado passou a ser o fluxo normal; P/T continuam disponiveis. Queima estatica usa heuristica didatica reversivel (30 mA no LED, 0,25 W no resistor), sem modelo termico. Transientes ignoram essa heuristica DC, evitando falso curto de indutores e inferencias incorretas a partir de clocks fixados no nivel alto. LED generico usa RS=2 ohms; rshunt=1e12 facilita montagens incompletas com fuga numerica. Resultados antigos sao invalidados ao mudar a montagem. CONTINUIDADE.md registra origem, validacao e pendencias.

A extracao inicial para dev_capture.c preservou a captura automatica byte a byte. Cenarios e fases ainda precisam sair de main. Fallback transititorio sem instrumentacao preserva a causa no status; teste com ciclo de fios ideais confirma curva valida sem correntes inventadas. Teste adicional confirma indutor diretamente na fonte sem queima baseada no equivalente DC.

Correcao P0 em 14/11/2026: setup solicita ngspice, gcc-libs e libwinpthread explicitamente ao pacman, preservando verificacao de integridade e rejeitando runtime sem as quatro DLLs. Um runtime preparado passou nos testes automaticos DC/transitorios e exemplos com PATH limitado ao Windows. Falhas do processo exibem codigo de saida, incluindo DLL ausente. O executavel atual de desenvolvimento consta no guia de compilacao.

Refatoracao em 14/11/2026: AppState e SimState organizam dados; app_simulation separa o ciclo de atualizacao do desenho.
Cenarios de captura e autoteste foram extraidos para dev_scenarios. Nenhum modelo eletrico foi alterado.
A captura NAND permaneceu identica; teste sem janela cobre invalidacao de resultados e amostras de LED.
Ao investigar evento sintetico de rolagem, consultado o [codigo oficial SDL 3.4.2](https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.2/src/events/SDL_events.c).
A direcao passou a ser definida explicitamente como normal e a estrutura zerada; testes repetidos passaram.
Essa verificacao nao identifica uma falha interna da biblioteca. A separacao de entrada/renderizacao e consolidacao
completa de SimState continuam pendentes; nao considerar apenas o agrupamento de campos como conclusao da arquitetura.


## 15/11/2026 - revisoes da montagem e validade de resultados

Fatos locais: o ciclo comparava snapshots completos em app_simulation/sim_state; Workbench
ocupava 21.512 bytes. O worker ja recebia sua copia isolada. Comparar hashes ainda exigiria
percorrer os dados e introduziria a possibilidade de colisao; manter memcmp preservaria o custo
por quadro. Adotado contador uint64_t por linha de edicao e APIs de mutacao no modelo.
Mover, alternar, editar valores/fios/joelhos, carregar, substituir, desfazer e refazer atualizam a
revisao. Operacoes sem alteracao e falhas nao atualizam; substituicoes sempre criam nova revisao.
O contador nao integra o formato de projeto e nao volta ao valor antigo ao desfazer.

Decisao: snapshots completos ficam no worker/historico, onde sao necessarios. A apresentacao
armazena revisoes e um unico SimResultKind (nenhum/DC/transiente), com transicoes em SimState.
Sensores de corrente sao metadados do resultado, nao outra fonte de validade. Inicializar Workbench
com {0}; publicar substituicoes com bench_replace; editar montagens publicadas pelas APIs do modelo.
Construtores podem preencher uma montagem temporaria antes de publica-la. O backend continua em lote.

Evidencia: testes de mutacoes, no-op, invalidos, encaixe, carregamento, undo/redo e resultado
pendente apos editar/desfazer. sizeof(SimState), medido com GCC UCRT64: 290.576 -> 183.040 bytes.
Nao foi medido ganho de FPS; o ganho demonstrado e a memoria menor e a eliminacao das comparacoes
integrais no ciclo. Nenhuma biblioteca/API externa nova; a comparacao e baseada no codigo local.
Ainda pendentes: estado do ciclo completo de tarefas, cancelamento e backend de passo real.


## 15/11/2026 - packing solicitado pelo usuario: comparacao antes da implementacao

Fonte oficial consultada: [manual ngspice 46, secao 2.6](https://ngspice.sourceforge.io/docs/ngspice-46-manual.pdf),
consulta em 15/11/2026. Fato: instancias .SUBCKT sao expandidas pelo parser em elementos internos;
os terminais externos ligam os nos da instancia ao circuito maior. A hierarquia nao permanece no solver.

Alternativas: (1) emitir .SUBCKT/X e remodelar nomes/sondas e resultados da camada atual; (2) manter
as pecas reais no modelo e adicionar encapsulamento visual/portas, expandido antes do solver existente;
(3) substituir o conjunto por portas booleanas ou equivalente simplificado. A terceira perde comportamento
analogico e foi descartada. A primeira exige mudar o mapeamento de todos os sensores neste incremento.

Decisao para o incremento: segunda alternativa, com membros/portas persistidos, conexoes de fronteira
explicitas e instancias copiadas independentes. O solver continua analisando os componentes originais.
Compactar geometricamente os componentes internos criaria contatos falsos em breadboards: o layout
interno sera preservado e contatos implicitos externos serao materializados em fios antes do packing.
A camada visual mostrara apenas o corpo compacto e os terminais expostos. Interligacoes internas
continuam no modelo. Nao inferir direcao logica de um terminal analogico; portas sao terminais eletricos.

Hipotese a validar: a conectividade e os resultados DC/transitorios antes/depois devem coincidir dentro
das tolerancias existentes. Aceite: criar/desfazer/descompactar, conectar terminais, duplicar/instanciar,
salvar/reabrir e resultados de referencia. Primeiro nivel de packing; composicao aninhada deve ser
tratada explicitamente, sem aceitar juncoes geometricas acidentais. Os limites atuais de componentes
continuam contando os membros reais e sao comunicados quando uma instancia nao cabe.
O pedido do usuario prioriza essa funcionalidade sobre a ordem anterior de registro de componentes.


Packing validado em 0.19.0: formato 16 acrescenta grupos/membros/portas; a migracao de tipos para
IDs persistentes proposta na revisao de qualidade ficou para outro numero de formato. Preservar
leitura 1-16 nessa migracao. O primeiro nivel de encapsulamento foi validado com DC, RC e fronteiras
de breadboard. Encapsular blocos ja encapsulados e recusado com motivo, para evitar unir layouts
internos de instancias antes de definir uma representacao hierarquica segura. Sem modelo eletrico
simplificado novo. Conexoes de portas compactas sao explicitas; nao presumir encaixe em breadboard.


## 15/11/2026 - funcao dos pinos no registro de componentes

Fonte oficial consultada: [TI SNx4HC00, SCLS181H, tabela 5-1](https://www.ti.com/lit/ds/symlink/sn74hc00.pdf),
consulta em 15/11/2026. Fatos: no PDIP de 14 pinos, 1A/1B/1Y ficam em 1/2/3;
3Y em 8, 3A/3B em 9/10; GND em 7 e VCC em 14. Rotulos dos demais componentes didaticos
seguem o modelo existente do projeto, nao sao alegacoes de equivalencia com CIs comerciais.

Comparacao: codificar funcao por cor/posicao no renderer duplicaria o modelo e confundiria
polaridade construtiva com tensao medida. Adotados descritores de nome/funcao no registro,
consultados pelo renderer e pela ajuda ao apontar. Passivos sao bidirecionais/sem polaridade;
contatos de breadboard nao viram automaticamente terra ou neutro. Portas de blocos mostram
funcao do terminal interno quando conhecida; classificacao personalizada continua pendente.
O renderer permanece separado do registro. Nenhuma alteracao de modelo eletrico e necessaria.


Registro inicial em 0.20.0: tabela C compilada foi escolhida em vez de manifesto externo neste
incremento, para compartilhar metadados sem acrescentar parser/dependencia de arquivo ao runtime.
IDs textuais ja existem na API; persistencia continua 16 para separar migracao de formato da
refatoracao. Teste verifica o contrato historico dos codigos, nomes de IDs e funcoes de pinos.
Padroes/faixas foram preservados. Isso nao conclui emissores locais, geometria ou parametros
completos propostos em QUALIDADE_CODIGO.md. Nenhuma dependencia externa nova foi introduzida.


## 15/11/2026 - editor isolado e nomes UTF-8: comparacao antes da implementacao

Fatos locais: AppState possui montagem/historico/worker; copiar a montagem para a mesma instancia
misturaria resultados e historico. Alternativas: trocar campos globais temporariamente ou manter
um AppState proprio do editor. Decisao: contexto proprio, trabalhador e historico separados;
a bancada externa fica suspensa e conservada. Voltar/descartar nao altera o modelo externo.
Salvar modelo e aplicar a instancia sao acoes distintas; aplicar usa substituicao transacional
com validacao das portas conectadas. Proveniencia temporaria dos membros acompanha as mutacoes
no editor, e e apagada em copias/importacoes; remover uma porta externa conectada impede aplicar,
em vez de reconectar silenciosamente um componente que ocupou o mesmo indice.

Nomes precisam preservar UTF-8, espacos e pontuacao. Formato 17 acrescentara nomes a grupos;
leitura 1-16 sera mantida. A migracao de tipos para IDs textuais continua separada. Capacidade
provisoria nomeada de 127 bytes, com rejeicao visivel de excesso, ate a migracao de dados dinamicos.

Fontes oficiais consultadas em 15/11/2026:
- [MultiByteToWideChar](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar):
  conversao CP_UTF8 com MB_ERR_INVALID_CHARS evita descartar sequencias invalidas.
- [CreateDIBSection](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection):
  bitmap acessivel pela aplicacao; GdiFlush sincroniza a escrita GDI antes de ler pixels.

Comparacao de texto: manter glCallLists de bytes nao renderiza nomes UTF-8 corretamente; adicionar
SDL_ttf introduziria outra dependencia de distribuicao. Adotado GDI Unicode para nomes, enviado
a textura com cache/evicao; fontes ASCII existentes podem continuar no caminho rapido.
Texto e graficos permanecem na camada de UI, sem SDL/OpenGL no modelo.


## 15/11/2026 - Grid configuravel: comparacao antes da implementacao
Fato: grid atual usa 24 unidades no mundo, origem Y=20 e oculta linhas intermediarias em zoom
baixo; nao participa do posicionamento. Breadboard tem encaixe geometrico separado. Fato oficial:
SDL_GetModState retorna mascara dos modificadores; consulta 15/11/2026:
https://wiki.libsdl.org/SDL3/SDL_GetModState .
Alternativa A: arredondar pixels de tela. Barata, mas muda montagem com zoom/pan; rejeitada.
Alternativa B: quantizar coordenadas do mundo com origem e espacamento explicitos. Custo constante,
independente da camera; escolhida. Ocultar linhas por densidade sera apenas visual, sem mudar snap.
Hipotese de UX: Alt suspende encaixe durante arraste, sem competir com Shift (selecao) e Ctrl
(clipboard). Validar por eventos/capturas antes de publicar. Encaixe em breadboard tem prioridade
no fim do arraste; grupos devem receber uma unica translacao para preservar geometria interna.
Primeiro incremento: matematica pura validada e compartilhada por desenho/entrada; depois controles
visiveis, persistencia retrocompativel e testes de GUI. Escala e terminais flexiveis continuam
pendentes e nao devem ser simulados apenas mudando o zoom.


## 15/11/2026 - Insercao de joelhos por trecho
Comparacao tecnica: escolher trecho pela polilinha original ignora curva renderizada; gerar uma
segunda aproximacao duplica geometria e pode escolher ramo errado em cruzamentos. Decisao:
reutilizar wire_path com proveniencia de cada segmento renderizado; projetar o clique no segmento
mais proximo e inserir antes/depois do canto correspondente. Primeira metade da curva pertence ao
trecho de entrada, segunda ao de saida. Empates deterministas pela ordem do caminho. Sem nova
biblioteca ou mudanca de modelo eletrico. A implementacao segue a mesma geometria quadratica ja
usada no desenho; testes devem cobrir linhas, curvas, cruzamentos, limites e undo/persistencia.
API SDL de contagem de cliques ja usada para remocao; nao introduzir temporizador paralelo.


## 15/11/2026 - Geometria local antes de escala e terminais flexiveis
Fato observado: bench_pin_position repete rotacao em tres ramos; desenho usa transformacao GL,
hit testing reconstrui retangulos orientados. Alterar apenas glScale deixaria terminais e contatos
fora do desenho. Alternativas: deformar coordenadas globais em cada consumidor (duplicacao e
risco de perder pinagem ao girar) ou armazenar geometria local e transformar numa fronteira unica.
Decisao: coordenadas locais para corpo/pinos/terminais, com transformacoes diretas e inversas
puras no modelo. Rotacao de quartos de volta usa trocas/sinais exatos, sem trigonometria. Primeiro
separar transformacao e preservar capturas/testes; escala e deformacao em commits funcionais
posteriores, com persistencia e validacao. Breadboard conserva orientacao atual nao rotacionavel.
Nenhuma nova dependencia ou escolha externa neste incremento; comparacao baseada no codigo.


## 15/11/2026 - Ownership dos caminhos flexiveis
Fato: Workbench, clipboard, historico e jobs sao copiados por valor. Introduzir ponteiro de
terminais em Part sem rever essas copias faria alias/double-free. Comparacao: arrays por todos
os 420 pinos desperdicam memoria e adicionam limites; buffer global imutavel sem reclamacao
acumula memoria; caminhos dinamicos com copia profunda e destruicao explicitas permitem crescer
ate falha de memoria e compartilhar infraestrutura com fios. Escolhida terceira alternativa.
Primeiro implementar/testar container de pontos independente; integrar somente apos trocar cada
fronteira de snapshot por clone/clear transacional. Nao colocar ponteiros em Workbench ainda.
A migracao para C++ pode facilitar ownership, mas nao e requisito para esse container C puro.
Fato oficial: realloc pode mover armazenamento e preserva bloco original quando falha. Consulta
15/11/2026: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/realloc . Manter
ponteiro antigo ate sucesso; verificar overflow antes de multiplicar capacidade por sizeof.
Hipotese de modelo: deformacao geometrica nao modela plasticidade/esforco; comprimento fisico e
limites mecanicos precisam ser definidos antes de apresentar a dobra como mecanica realista.


## 15/11/2026 - Fronteiras de snapshots antes de dados com ownership
Refatoracao incremental escolhida: clone/clear/move explicitos para snapshots, mantendo Workbench
sem ponteiros neste commit. Historico usa transferencia em vez de memmove de objetos proprietarios;
redo descartado e entradas consumidas sao limpos. Undo/redo preparam estado e pilha de retorno
antes de publicar revisao nova. Workers mantem snapshot ate resultado ser consumido; join sozinho
nao o apaga, pois app_simulation ainda consulta revision/periodicidade depois do poll.
Encerramento explicito de AppState libera snapshots/historico/clipboard. Nenhuma mudanca de
formato ou novas alocacoes no modelo ainda; faltam migrar copias locais/packing antes de anexar
caminhos dinamicos. Nao confundir este contrato preparatorio com ownership dinamico ja integrado.


## 15/11/2026 - Escopo prioritario e limpeza de snapshots na toolchain atual
Usuario priorizou dobra, catalogo e polimento. Para reduzir retornos manuais esquecidos, avaliar
cleanup de variavel automatica no GCC (toolchain atual): executa funcao de limpeza ao sair do
escopo, inclusive return. Alternativa: goto cleanup em cada handler; mais portavel, mas alonga
os handlers. Fonte oficial consultada em 15/11/2026:
https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html . Nao supor suporte MSVC.
Fonte da UI: reutilizar renderizador Unicode GDI/Segoe UI ja existente, com cache de texturas,
em vez de listas raster WGL serrilhadas. Ctrl+scroll reservado ao tamanho da peca selecionada;
scroll simples continua zoom; sem campo percentual obrigatorio. Validar capturas e eventos.


## 15/11/2026 - Ampliacao inicial do catalogo para testes
NOR/XNOR: usar primitivas XSPICE d_nor/d_xnor, mantendo bridges/limiares e atrasos ja documentados;
nao simular gates pela conectividade. Fonte oficial consultada em 15/11/2026:
https://ngspice.sourceforge.io/docs/ngspice-manual.pdf (Digital Models). Validar quatro combinacoes.
Potenciometro: tres pinos A/W/B, divisor com duas resistencias; posicao de 0 a 1 persistida. Resistencias
minimas de segmento 1 mOhm evitam curto ideal singular nos extremos; aproximacao visivel/documentada,
sem ruido, potencia/curso mecanico ou taper logaritmico. Validar com divisor analitico e carga.
Botao: contato ideal momentaneo, fechado enquanto pressionado, aberto ao soltar. Slots preservam
montagem e configuracao; botao deve carregar solto. Nao representa bounce nem captura de pulso
curto pelo backend de passo real ainda pendente. Catalogo maior precisa de rolagem na biblioteca.

## Catalogo DIP e continuidade (15/11/2026)
Fontes oficiais consultadas em 15/11/2026: https://www.ti.com/lit/ds/symlink/sn74hc08.pdf,
https://www.ti.com/lit/ds/symlink/sn74hc32.pdf, https://www.ti.com/lit/ds/symlink/sn74hc86.pdf.
Fatos: quatro portas de duas entradas, pinagem DIP14 compartilhada com o 74HC00.
Alternativas: modelos completos do fabricante versus modelo didatico alimentado existente.
Decisao: AND/OR/XOR com aproximacao explicita do 74HC00: somente 4.5..5.5 V, entradas
<=1.5/>=3.5 V e saida de 50 ohms; nao representa todos os parametros TI.
Dimensoes visuais independentes de blocos nao alteram netlist. Altura minima preserva
espacamento de pinos. Formato 22 conserva dimensoes e le formatos anteriores.
Continuidade: geometria sem breadboard e com mesmos endpoints/parametros conserva resultado.
Backend batch reinicia IC=0 de capacitores/indutores e DFF. Alternativas pendentes: ngspice
compartilhado persistente ou migracao completa de estados. Migrar apenas capacitor nao resolve DFF.
Nao confundir retencao visual com continuidade eletrica concluida.
Referencia visual: capturas paginas 5/6 do guia oficial Tinkercad Circuits consultadas em 15/11:
https://assets.ctfassets.net/jl5ii4oqrdmc/4sMFqe3rDlbUymJt0I4yh/85a4487f7fe274e74c19870ae4679fc1/tinkercad-guides_circuits-Printable.pdf.
Visual proprio 2.5D. Brilho e curva visual sqrt(I/20mA) da corrente direta medida, limitada
a 1; nao e modelo fotometrico nem rating do LED.


## Perninhas e contato direto (15/11/2026)
Pedido: reutilizar interacao dos fios, sem mecanica complexa. Comparacao: copiar arrays fixos de
joelhos por pino versus BenchPath dinamico existente. Decisao: caminhos locais dinamicos por pino,
criados sob demanda, clone profundo por Part e snapshots. Corpo permanece independente; ultimo
ponto e o terminal eletrico. Alt+arrastar terminal edita perna, duplo clique no caminho insere
joelho, duplo clique em joelho remove. Duplo clique nao exige modo global adicional.
Contato direto: terminais coincidentes dentro da tolerancia geometrica existente (.01 unidades)
formam o mesmo no, inclusive entre pecas; encaixe visual pode aproximar antes de publicar.
Fato: coincidencia e conectividade ideal, nao solda nem resistencia de contato. Cruzar segmentos
sem unir as pontas nao cria contato. Blocos distintos continuam isolados internamente.
Continuidade: fonte oficial https://ngspice.sourceforge.io/shared.html consultada em 15/11/2026
confirma parar, alterar parametros e retomar na mesma sessao. DLL por contexto/processo e a
recomendacao para preservar estados e isolamento; somente reapresentar curvas nao atende.


## Sessao continua nativa (15/11/2026)
Evidencia: experiments/persistent_spice_probe.py preservou RC/DFF; testes nativos repetiram e
mantiveram duas sessoes independentes. Comparacao: DLL global no processo da UI e barata mas
compartilha estado/risco de crash; copias de DLL/codemodel exigem validar globais; host separado
por documento isola tanto estado quanto falhas. Decisao: spice-live-host.exe por contexto,
protocolo interno em pipes, timeout10s, logs de falha preservados. Worker segue sem bloquear UI.
Fontes: https://ngspice.sourceforge.io/shared.html e https://ngspice.sourceforge.io/parallel.html
consultadas em15/11/2026; header sharedspice.h do runtime instalado confirma ABI utilizada.
Novo netlistlive usa contatos finitos1uOhm/1TOhm, evitando mudar topologia quando abre/fecha.
FonteOFF abre contato de alimentacao: nao e mais fonte0V em curto no backendcontinuo.
Ngspice guarda C/L e estadosXSPICE entre resume/alter. Topologia nova ou frequencia de clock
nova ainda reconstroi sessao explicitamente; migracao de estado nesses casos permanece pendente.
Tempo e mostrado em segundos simulados; ritmo deriva do intervalo de visualizacao existente.
Vetores completos da sessao permanecem no solver ate fechar o contexto; memoria cresce com tempo,
um limite pratico a acompanhar antes da1.0. Nao manipular vetores internos para truncar semvalidacao.

Validacao adicional: tentativa de desativar spinit/.spiceinit pelas funcoes pre-init indicadas
no header causou0xC0000005 neste runtime. Chamadas removidas; nao reintroduzir apenas com base
na declaracao do header, sem validar binario/API. Host isolado conteve a falha. Caminho validado
usa ngSpice_Init padrao e carrega digital.cm explicitamente. Configuracao externa do ngspice
ainda precisa de tratamento deterministico antes da1.0. Falhas de host agora reportam exitcode.


## Continuidade, imagens e catalogo ampliado (15/11/2026)
Fato experimental: a restauração isolada de d_dff(ic=1) com D=0/CLK=1 resultou em Q=0.
Portanto transportar apenas Q nao conserva a memoria; nao anunciar migracao de topologia baseada
nessa aproximacao. Alternativas: conservar sessao atual com conectividade parametrizada ou
reproduzir historico eletrico em grafo ampliado; migracao parcial C/L/Q exige tratar bordas e
atrasos digitais. Decisao pendente de prova numerica, preservando o backend atual verificavel.
Fontes oficiais consultadas: ngspice manual (event history, IC e shared API),
https://ngspice.sourceforge.io/docs/ngspice-manual.pdf, e https://www.tinkercad.com/circuits.
Para a revisao visual: capturador por ID, sem modificar limites de zoom do usuario. Desenho nativo
OpenGL e preferido a sprites raster por manter terminais editaveis, cores e zoom independentes.
Inventario integral e ordem de trabalho em CATALOGO_COMPONENTES.md. Sensores devem atuar sobre
parametros/modelos eletricos; sliders nao devem forcar arbitrariamente o estado de saidas.


## Catalogo analogico: comparacao antes da implementacao (15/11/2026)
Fatos: modelos D e Q nativos do ngspice resolvem juncao/BJT, ao contrario de
um limiar booleano ou resistor substituto. Manual oficial consultado:
https://ngspice.sourceforge.io/docs/ngspice-manual.pdf (capitulos D/BJT).
Interlink documenta FSR como resistencia decrescente com forca:
https://www.interlinkelectronics.com/fsr-400-series . Curva generica nao equivale a calibracao.
Referencias mecanicas Energizer revisadas em PNG antes dos desenhos:
https://data.energizer.com/pdfs/522.pdf , https://data.energizer.com/pdfs/E91NA0426.pdf ,
https://data.energizer.com/pdfs/2032GL0626.pdf . AA nova: 150..300 mOhm.
Comparacao: fonte ideal simplifica mas nao representa queda sob carga; fonte com Rserie
permite queda e desligamento por contato sem criar um modelo quimico inexistente.
Decisao: modelos SPICE genericos para D/NPN/PNP; LDR/flex/FSR com curvas aproximadas
identificadas; inclinacao por contato dependente do angulo. Baterias com Rserie
constante (AA .2 ohm, 9V 1 ohm e moeda 15 ohm; ultimas duas escolhas didaticas,
nao curvas medidas). Capacidade, espectro/tempo dos sensores e envelhecimento pendentes.
Sliders editam estimulo normalizado salvo, nunca forcam diretamente uma leitura de saida.
Hipotese verificavel: alter de R/I permite controles ao vivo sem reiniciar C/L/DFF;
validar com ngspice e GUI antes de publicar. Inventario completo de CIs e atuadores
fica pendente, conforme CATALOGO_COMPONENTES.md; nao anunciar cobertura total.


### Temperatura e solo (15/11/2026, antes da implementacao)
Fato: TMP36 tem transferencia 500 mV + 10 mV/C, alimentacao 2.7..5.5 V, faixa
-40..125 C e limite de corrente de saida tipico 250 uA. Fonte ADI consultada:
https://www.analog.com/media/en/technical-documentation/data-sheets/tmp35_36_37.pdf .
Decisao: modelo alimentado com transferencia nominal, Rsaida 100 ohm didatica,
limite 250 uA e consumo em repouso 50 uA; alta impedancia fora da alimentacao.
Nao simula aquecimento proprio, resposta termica nem comportamento garantido fora da faixa.
Pinagem explicita V+/OUT/GND na UI; desenho usa frente plana, sem deduzir pinagem de foto.
Solo: modulo resistivo generico de 3 pinos, Rsonda 100k..1k + divisor 10k;
slider representa umidade relativa ilustrativa, nao concentracao volumetrica calibrada.
Comparacao: fonte ideal na saida ignoraria alimentacao/carga; modelos escolhidos permitem
medir queda sob carga e desligamento. Modulos capacitivos/comparador continuam distintos.


Resultado de validacao TMP36: limitador por Bfonte de corrente saturada falhou com
"Timestep too small" em UIC, embora o OP passasse. Substituido por fonte de tensao com
realimentacao de corrente, Rsaida100 ohm e 1Gohm diferencial alem de +/-250uA. Mantem
inclinacao na matriz MNA e convergiu sob carga de1Mohm/100ohm no backendbatch e continuo.
Consumo50uA + corrente fornecida, alta impedancia fora da faixa de alimentacao. Comparacoes
experimentais motivaram o teste automatico em tests/test_analog_catalog.c.
Nao usar sucesso apenas em DC para aprovar modelo dinamico.


## LED/potenciometro sem intermitencia artificial (15/11/2026)
Diagnostico no codigo: potenciais circuitos estaticos usavam batch/debounce120ms; revisao
nova apagava resultado antes do retorno e descartava todos os resultados durante ajustes
continuos. Decisao: sessao persistente para todo circuito com fonte/bateria, sem debounce;
exibir ultima amostra compativel durante alteracao de parametros, marcada como pendente.
Comparacao de compatibilidade ignora valores/on/controle, mas verifica tipos, grupos, layout
de breadboard, contatos fisicos e ligacoes. Nunca atribuir revisao nova a leitura antiga.
Aceitar amostras intermediarias compativeis evita starvation ao arrastar constantemente.
Invalidar antes de agendar novo worker: o snapshot antigo precisa detectar desconexoes
antes de ser substituido. Nao manter amostra antiga para nova topologia, erro ou modo manual.
Nao usar interpolacao visual para esconder falta de calculo: corrente/brilho continuam
vindos do ngspice. Suite de UI verifica80alteracoes consecutivas com LED sempre aceso e
amostras novas durante ajuste, potenciometro/resistor e presenca de breadboard.


## Capacitor travando ao desligar (15/11/2026, Claude)
Relato: montagem com fonte, chave, 470 ohm, 1 uF em paralelo com 1 kOhm + LED "calcula sem parar"
ao desligar fonte ou chave. Fato reproduzido: no netlist instrumentado (Vwire por fio),
`alter Rpower0 = 1e12` seguido de `resume` colapsava o passo do transitorio; o host excedia 10 s
avancando ~70 us. Sem Vwire, ou com outra ordem de fios, nao travava: sensivel a numeracao/pivotacao.
Experimentos (sessao real via spice_session): fechado 1e-6 com aberto 1e10/1e12 trava; 1e-6/1e9 leva
3 s; 1e-6/1e8 ok; fechado 1e-3 com aberto 1e12 ok. Interpretacao: razao 1e18 excede 1/DBL_EPSILON
(~4.5e15); o nó aberto fica numericamente indefinido.
Decisao: NETLIST_LIVE_CLOSED_OHMS = 1 mOhm (ordem de contato real de chave) e
NETLIST_LIVE_OPEN_OHMS = 10 GOhm (fuga 0.5 nA a 5 V), razao 1e13, tres decadas de margem.
Fonte de bancada usa o mesmo 1 mOhm. Regra: nao reintroduzir razao aberto/fechado > ~1e13.
Regressao: tests/test_spice_live.c check_led_hold monta uma fiacao de referência (ordem importa),
abre fonte e chave, exige 60 quadros de 16.7 us cada um em < 2 s e brilho decrescente gradual.
O teste falhou antes (passo > 2 s) e passou depois.
Escala de tempo: cada quadro avanca transient_stop/300 (~1 ms simulado por segundo real), entao
tau ~1 ms da montagem aparece como apagamento de ~1-3 s na tela. Nao e tempo real fisico.
Build: build.ps1 compila o host em arquivo temporario e mantem o existente se estiver travado por
uma Bancada aberta e nao for mais antigo que src/spice_live_host.c.


## Revisao de menus e preferencias (15/11/2026, Claude)
Fato: render e entrada repetiam coordenadas literais; o autoteste clicava em numeros fixos.
Decisao: app_layout.c (puro) define retangulos nomeados; render, entrada e autoteste usam os mesmos.
Refatoracao comitada separada (187 fases passaram) antes do redesenho.
Configuracoes: um unico dialogo em duas colunas (Aparencia, Grade e encaixe, Fios, Grafico
transitorio), com toggles, stepper, campos, fechar e Restaurar padroes; sem "[x]" nem "(clique)".
Preferencias persistem em saves/preferences.txt (chave valor, versao 1; chaves desconhecidas
ignoradas; gravacao temp + MoveFileEx). Autoteste e capturas NAO leem esse arquivo: precisam ser
reprodutiveis. Tema e tamanho do texto sao do app: carregar slot nao os altera mais (slots
continuam gravando-os por compatibilidade de formato). Removidas teclas S/L sem Ctrl, que
gravavam um bancada.elec oculto (substituidas por slots Ctrl+S/Ctrl+L; S acidental gravava arquivo).
Botoes mostram acao sem atalho (preferencia do usuario); atalhos ficam no rodape contextual.
Arte: capacitor como disco ceramico (nao polarizado), fonte de bancada com visor de 7 segmentos,
bornes e knob; fototransistor com cupula transparente distinta do fotodiodo.


## Exemplo 74HC00 abortando (15/11/2026, Claude)
Fato: exemplo 3 falhava na sessao continua: "Timestep too small; t = 0,3 us, no da saida 1Y".
Causa: CIs quad (16cc02f) usavam fontes B com degrau ideal (?1:0) e chaves SW sem histerese;
a borda do clock cruzando o limiar criava descontinuidade pura. Nao era regressao do contato 1 mOhm.
Decisao: acionamento continuo (rampa linear entre 1,5 V e 3,5 V; logica por produtos; janela de
alimentacao 4,5..5,5 V com bordas de 0,1 V) e SW com Vh = 0,1 V. Alto e baixo sao complementares,
entao a histerese impede conducao simultanea. Em 0/5 V as tabelas-verdade sao identicas (test_catalog).
Entradas entre 1,5 e 3,5 V passam a dar saida intermediaria aproximada, nao garantida pelo fabricante.
Regressao: tests/test_examples_live.c roda TODOS os exemplos na sessao continua e verifica a NAND
seguindo o clock; falhou antes da correcao e passou depois.


## Catalogo de exemplos (15/11/2026, Claude)
Decisao: tabela unica em workbench_examples.c (titulo, categoria, descricao, construtor curto);
indices 0..2 preservados para testes e F2. Botao do cabecalho abre dialogo de cartoes; F2 segue
avancando. Exemplos usam apenas componentes ja validados; cada um foi conferido no ngspice contra
sua descricao (correntes de LED) antes do commit. Layouts evitam a zona do inspector (canto
superior direito). Limite conhecido: o inspector ainda pode cobrir pecas do usuario nessa zona.


## Elementos por peca e dispositivos (15/11/2026, Claude)
Decisao: component_devices.c descreve pecas multi-elemento (terminais, corpo, medicoes, modos,
bancos de contatos) e netlist_devices.c emite seus modelos e controles `alter`. Resultado guarda ate
8 medicoes por peca (correntes, nos ou estados); Part.states guarda bits do DIP ou modo do
instrumento, persistido no formato 26 (le 1-25). Mudanca de forma de onda/frequencia do gerador
reconstroi a sessao (como o clock); bits do DIP, DPST, modo do multimetro e sensores usam `alter`.
Modelos (aproximacoes didaticas, declaradas no inspector):
- LED RGB/7 segmentos: lei do diodo; IS escolhidos para 10 mA em ~1,9/2,2/2,9 V; catodo comum.
- Rele 5 V: bobina 100 ohm + 10 mH; contato NA/NF por chaves SW com histerese (fecha ~25 mA).
- Motores: R-L + forca contraeletromotriz k*w + inercia (capacitor) + atrito (resistor); CC:
  10 ohm, k 0,01; vibracao: 30 ohm, k 0,003. Encoder: 12 pulsos/volta em quadratura a partir do
  angulo integrado, alimentado por VCC-GND.
- Servo: posicao proporcional ao nivel MEDIO do sinal relativo a V+ (0-180 graus); nao decodifica
  pulsos de 1-2 ms. Piezo passivo: 20 nF || 1 MOhm; indica vibracao com corrente alternada.
- Gerador: 0-5 V, 50 ohm, seno/quadrada/triangular. Multimetro: V (10 MOhm), A (shunt ideal entre
  A e COM), Ohm (ate 0,5 mA, tensao de teste ~2 V; leitura invalida com fonte ativa).
  Osciloscopio: 1 MOhm, historico de 160 amostras.
- PIR, gas (MQ), IR de obstaculo e distancia IR analogica (tipo GP2Y0A21): modulos alimentados com
  saida 100 ohm proporcional a V+; estimulo pelo slider de ambiente.
Nao implementados com justificativa: LCD 16x2 I2C (exige mestre I2C/microcontrolador, inexistente
na Bancada); sensor ultrassonico (trigger/echo exige temporizacao por pulso; o sensor de distancia
atual e IR analogico, declarado). Displays 7 segmentos e RGB dentro de blocos aparecem no corpo.
Regressao: tests/test_devices.c verifica cada dispositivo no ngspice.

### Limite da fonte de teste do multimetro (17/11/2026)

Fato: o modelo anterior injetava 1 mA sem limite de tensao. No circuito Cockcroft-Walton a 1 Hz,
alternar V -> A -> Ohm -> V elevou a saida de 1,24 V a 237 V em 200 ms. A corrente carregou os capacitores.
Referencia consultada em 17/11/2026: [Fluke 17B+](https://www.fluke.com/en-ph/product/electrical-testing/digital-multimeters/fluke-17b-plus)
informa tensao de teste em circuito aberto tipica de 2,0 V e corrente de curto inferior a 0,6 mA;
[Fluke, How to Measure Resistance](https://www.fluke.com/en-us/learn/blog/digital-multimeters/how-to-measure-resistance)
orienta desligar a alimentacao e descarregar capacitores antes da medicao.
Decisao: manter uma corrente de teste modelada, agora 0,5 mA, com diodo de isolamento e limitador
de tensao por zener. A interface omite a leitura Ohm se houver fonte ativa. Este e um modelo
generico, nao uma replica do Fluke 17B+: nao inclui autorange, fusivel nem protecao completa.

## Microcontroladores, editor e blocos (0.34.0, 16/11/2026)
Decisao: interpretar um subconjunto de Arduino (mcu_lang: compilador de uma passada para bytecode e
VM retomavel que cede em delay()) em vez de emular AVR/Xtensa. Fatos: Tinkercad roda AVR real;
emular ISA exige toolchain do usuario e foge do MVP. Hipotese aceita: programas didaticos cabem
no subconjunto; falta (pendente) ponteiros, String, bibliotecas alem de LiquidCrystal_I2C.
- Tempo: a placa usa o relogio real (millis = ms reais com a simulacao rodando); o circuito
  analogico tem sua escala propria. Pausar a simulacao pausa a placa.
- Pinos: saida = fonte comportamental nivel*VCC atras de 25 ohm; entrada = aberto; INPUT_PULLUP
  = 30 kohm a VCC. Mudancas de pino usam `alter` sem reconstruir a sessao. PWM = tensao media.
- LCD I2C: o texto e aplicado quando SDA/SCL estao no mesmo no dos pinos I2C da placa e o LCD
  tem alimentacao > 4 V. Nao ha protocolo I2C bit a bit nem endereco verificado (declarado).
- Editor: buffer puro em code_text.c (testado); app_code_editor.c so roteia eventos SDL. O
  rascunho so chega a peca por app_code_apply, que grava historico (Ctrl+Z). Fechar aplica.
- Transparencia (X): corpo das pecas com alfa constante via glBlendColor (GL 1.4 carregado em
  tempo de execucao); sem ele, so o teste de acerto muda. Sem X, terminais cobertos continuam
  inacessiveis (margem de borda 6 px).
- Troca pelo catalogo: bench_swap_type mantem posicao/rotacao/escala e fios em terminais de mesmo
  indice; descarta fios de terminais inexistentes no tipo novo.
- Portas de blocos: por no eletrico (fios, contatos e faixas de breadboard), um unico terminal
  representante e exposto se o no e livre (so um terminal do bloco) ou alcanca peca externa.
  Ligacoes internas que passavam por breadboard externa viram fios internos; fios de fronteira em
  terminais ocultos migram para o representante.

## Entradas logicas flutuantes e identidade de chips (16/11/2026)
Fato: no live, chave aberta = 10 GOhm (NETLIST_LIVE_OPEN_OHMS) e a unica fuga da entrada de porta
era rshunt 1 TOhm; divisor deixava a entrada atras de chave aberta em ~4,95 V (ALTO).
Uma montagem AND de 4 entradas acendia o LED com tudo desligado; o DC (chave aberta omitida) dava 0.
Decisao: cada entrada de porta (gate_adc) e de 74HC tem NETLIST_GATE_INPUT_OHMS = 100 MOhm ate a
referencia (terra ou pino GND do CI): entrada aberta le BAIXO (~0,05 V), fuga 50 nA a 5 V. Nao
amplia a razao aberto/fechado (1e13). Simplificacao didatica: CMOS real flutuante e indefinido.
Pendente/decisao do usuario: pino de MCU em INPUT continua so com rshunt (le ALTO atras de chave
aberta), como no Arduino real o pino fica indefinido; avaliar mesma regra ou aviso na UI.
Regressao: tests/test_spice_live.c check_four_input_and (falhou antes: 3,1 mA com tudo desligado).

Fato: Ctrl+G grava o bloco na biblioteca, mas a instancia nao sabia qual era sua entrada; o editor
isolado salvava sempre em slot livre (duplicava ao renomear/salvar).
Decisao: BenchGroup.model (uint64, formato 28, linha `group ... model len`) identifica o modelo;
copias colocadas da lista herdam. Salvar no editor grava no slot com esse id e liga a instancia
externa (so identidade, circuito intacto). Sem id (arquivos <= 27): usa o slot cujo nome e unico
e igual ao nome inicial do chip e adota o id dele. Id removido da lista -> nova entrada; slot
reaproveitado por outro chip nunca e sobrescrito (id diferente). Renomear na bancada nao altera a
biblioteca. Regressao: tests/test_chip_library_flow.c, passos de usuario (falhou antes no 1o salvar).

## Idiomas da interface e estilo dos textos (16/11/2026)
Pedido: revisar todos os textos no estilo do ASD-STE100 (o que é e o que faz, sem jargão, neutro) e
trocar o idioma da interface por bandeiras, com pt-BR e en-US.
Decisão: catálogo próprio em C, sem gettext. Cada texto é `UI_TEXT(ID, "pt-BR", "en-US")` em
`src/text/<área>.def`; um X-macro gera o enum `TextId` e as tabelas. Motivos: tradução faltando não
compila; os dois idiomas ficam lado a lado para revisão; o módulo é C puro (serve à camada de modelo,
como nomes de peças e erros do compilador do Arduino); sem dependência nem arquivo externo para
distribuir. Alternativa descartada: chave = texto em português (gettext-like), que quebra em silêncio
quando o português muda.
- `tests/test_ui_text.c` aplica o guia (docs/TEXTOS.md): especificadores printf iguais e na mesma ordem
  nos dois idiomas, palavras proibidas, até 20 palavras por frase, português sem acento, espaços.
- O idioma é preferência do programa (`language` em saves/preferences.txt, índice 0 = pt-BR,
  1 = en-US, só acrescentar). Não entra em arquivos de projeto. Primeira execução: idioma do sistema
  (SDL_GetPreferredLocales; "pt" -> pt-BR, outros -> en-US). "Restaurar padrões" mantém o idioma.
- Bandeiras desenhadas com primitivas (a fonte da interface não tem emoji de bandeira), no topo do
  diálogo de Configurações. `--language=en-US` força o idioma em capturas e revisões.
- Textos já guardados em buffers (mensagens de status) ficam no idioma em que foram gerados até a
  próxima mensagem; nomes que o usuário digitou (chips, código) não são traduzidos.
Pendente: números em pt-BR ainda usam ponto decimal na exibição (%g); a entrada aceita os dois.

## Rotação de chips e contato universal (16/11/2026, antes da implementação)

Fatos: o pacote não tem orientação própria; R é bloqueado na seleção de chips. Contatos usam
coordenadas internas e exigem grupos iguais; por isso não unem portas visíveis de chips ao exterior.
O encaixe da UI também exclui chips e breadboards em alguns caminhos.

Comparação: girar os membros internos mudaria a montagem do editor e misturaria dois contextos.
Guardar a orientação no pacote permite uma transformação única para corpo, portas e seleção.
Para contatos, criar fios ocultos acumularia conexões após mover peças. Recalcular a coincidência
dos terminais mantém o comportamento geométrico já usado pela bancada.

Decisão: orientação do pacote em quartos de volta, persistida no formato 29 (anteriores usam zero).
Dentro do mesmo chip, preservar contatos internos; entre contextos, só portas expostas usam a
posição externa. Breadboards e peças comuns participam dos mesmos contatos. Encaixe aproxima;
só coincidência efetiva cria ligação. Mover para longe desfaz o contato.

Fonte oficial: [manual ngspice, seção 2.1](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf),
consultado em 16/11/2026: terminais conectados compartilham nós. A mudança ocorre no mapeamento da
bancada para esses nós; não muda as equações dos dispositivos. Hipótese a validar: a mesma geometria
serve ao desenho, clique, encaixe e netlist, sem expor os membros ocultos de outro chip.

## Nome confirmado separado do rascunho do circuito (16/11/2026, 0.37.1)

Pedido: o nome “4and” deve permanecer, com um único botão de renomeação dentro do chip.
Fato: antes, set_name alterava apenas editor_name. Salvar modelo escrevia na biblioteca, mas
a bancada conservava o nome anterior até Aplicar. Descartar o rascunho também perdia o nome.

Comparação: continuar vinculando o nome ao rascunho exigiria duas confirmações para a mesma ação.
Tratar o nome confirmado como metadado permite preservar o circuito isolado e tornar a ação direta.
Decisão: Enter confirma o nome na instância alvo e na entrada existente da biblioteca. A biblioteca
é renomeada a partir do circuito já salvo, sem incorporar edições pendentes do editor. Confirmar
o nome não salva o projeto inteiro nem cria uma entrada nova na biblioteca. Cópias já colocadas
continuam independentes. Nomes inválidos ou falha ao gravar a entrada não alteram o nome da instância.
O rascunho mantém os fluxos de salvar/aplicar/descartar para alterações elétricas. F4 e o botão
externo são removidos. Não há mudança no formato de arquivo ou no modelo elétrico.

## Painel da peça compacto e recolhível; um painel do topo por vez (16/11/2026, 0.38.0)

Pedido: o painel da peça ocupava muito da bancada; precisa ser menor e poder ser minimizado,
deixando algo visível. Projetos e Exemplos podiam ficar abertos ao mesmo tempo.
Fato: o painel tinha 420 px de largura e até 296 px de altura (LED, sensores). Projetos (topo e F3)
e Configurações por F1 só alternavam o próprio painel.
Decisão: painel com 320 px, linhas de 26 px e notas em até duas linhas; botão – recolhe numa
etiqueta com o nome, mantida ao trocar de peça e herdada pelo editor do chip. Recolhido, cliques
nos controles ocultos não têm efeito. O estado não é salvo em preferências nem no projeto.
`app_state_toggle_panel` é o único caminho para abrir Projetos, Exemplos e Configurações.
Sem mudança no formato de arquivo ou no modelo elétrico.

## Circuitos grandes e terminais de chip reorganizáveis (16/11/2026, 0.39.0)

Pedido: uma montagem grande (34 peças, cerca de 990 × 1920 unidades) não cabia na tela; com pouco zoom,
os nomes das portas e as bolinhas dos terminais cobriam o circuito; num chip, escolher o lado dos terminais.
Fatos: `Ctrl+A` já existia, mas não aparecia na dica; o zoom mínimo era 0,35. Nomes usam fonte bitmap,
que não escala, e bolinhas tinham tamanho fixo na tela. Terminais de chip: metade à esquerda, resto à direita,
na ordem das peças.
Decisões: zoom de 0,12 a 4. Bolinhas com tamanho fixo no mundo abaixo de 100% (mínimo de 1,5 px); rótulos
ocultos abaixo de 60%. Seleção por área rola a vista a até 900 px/s nos 32 px junto às bordas.
Cada chip guarda lado e ordem de até 64 terminais (`port_layout`, `port_places`). Se a contagem de terminais
mudar (por exemplo, ao aplicar uma edição), volta o arranjo automático. Formato 30 grava o arranjo após a
rotação; formatos anteriores leem arranjo automático. Lados de cima e de baixo ficaram fora deste incremento.
Sem mudança no modelo elétrico: a posição do terminal muda só a geometria (desenho, clique, contato direto).

## Portas lógicas alimentadas pela fonte e terminais de chip nomeados (16/11/2026, 0.40.0)

Pedido: com fonte de 2 V, um LED na saída de uma AND queimava; a pessoa quer comportamento realista e não
entende de onde viriam 5 V. Também quer nomear os terminais de chips personalizados (ex.: decodificador de
7 segmentos).

Fatos (fontes oficiais consultadas em 16/11/2026):
- [TI SN74HC08](https://www.ti.com/lit/ds/symlink/sn74hc08.pdf): VCC especificado de 2 a 6 V (nominal 5 V);
  VIH mínimo 1,5 / 3,15 / 4,2 V e VIL máximo 0,5 / 1,35 / 1,8 V em VCC 2 / 4,5 / 6 V; VOH mínimo 1,9 / 4,4 /
  5,9 V com 20 µA; em 6 V com 5,2 mA, VOH mínimo 5,34 V (típico 5,48 V).
- [Nexperia 74HC08](https://assets.nexperia.com/documents/data-sheet/74HC_HCT08.pdf): faixa de 2,0 a 6,0 V.
- Modelo anterior: portas avulsas com pontes XSPICE fixas (entrada 1,5/3,5 V, saída ideal 0/5 V e 2,5 V para
  nível indefinido), sem ligação com a fonte. Com entradas em 2 V, saída ideal de 2,5 V num LED sem resistor.

Comparação: (a) pinos VCC/GND visíveis em cada porta: mais fiel, mas quebra os circuitos existentes e estoura
o limite de 128 fios em circuitos com ~30 portas; (b) alimentação comum, equivalente aos pinos de alimentação
ocultos de programas de esquema; (c) manter 5 V ideais e só avisar. Decisão da pessoa: (b).

Decisões: VCC das portas = positivo da fonte de referência (a primeira fonte, que também define o terra).
Entradas reescalonadas para o domínio de 5 V das pontes XSPICE, de modo que 1,5/3,5 V correspondam a 30%/70%
do VCC. Saída CMOS por duas chaves de 50 Ω (para o VCC e para o terra), o mesmo valor já usado nos 74HC
quádruplos; a corrente vem da fonte. Janela de alimentação 2–6 V para portas avulsas e quádruplas.
Sem fonte, um gerador de clock define o terra e alimenta a lógica com 5 V (módulo de 5 V).

Hipóteses e limites: 30%/70% aproxima a tabela (a 2 V a TI especifica 25%/75%). Os 50 Ω aproximam a saída
típica a 4,5–5 V; em 2 V a saída real é mais fraca. Com várias fontes, só a primeira alimenta a lógica.
A corrente de alimentação em repouso e o limite absoluto de 25 mA por saída não são modelados.

Nomes de terminais: texto por peça (`port_names`, linhas "<pino>\t<nome>"), com a mesma regra de posse do
programa dos microcontroladores; só aparece enquanto o pino é terminal de um chip. Até 15 bytes, sem
caracteres de controle. Formato 31 grava a seção `portnames`. Com terminais nomeados, o nome do chip fica centralizado
no corpo, entre os nomes dos dois lados.

## Limites maiores e junções de fio (16/11/2026, 0.40.0)

Pedido: ligar um fio diretamente em outro, como nos esquemas, e subir bastante o limite de peças.
Fato: um fio liga dois pinos; derivações exigiam fios repetidos até o mesmo terminal. Limites: 64 peças, 128 fios.
Decisão: junção é uma peça de um pino (`PART_JUNCTION`, formato 31), sem corpo, desenhada como ponto cheio;
terminar um fio sobre outro divide o fio de baixo em dois no ponto (as dobras se repartem e a cor se mantém).
Eletricamente é só um nó. Junções sem fio são removidas ao apagar fios. Limites: 256 peças e 512 fios.
Os vetores de união por pino passam a ~430 KB; os executáveis usam pilha de 64 MB (`-Wl,--stack`).
Hipótese a validar: desempenho da simulação contínua perto do limite de peças.

## Chips compostos em vários níveis (16/11/2026, em desenvolvimento)

Pedido: usar chips dentro de outros chips, conservando a possibilidade de entrar e editar cada um.
Comparação prévia e fonte oficial: [CHIPS_RECURSIVOS.md](CHIPS_RECURSIVOS.md).
Decisão: peças elétricas continuam planas; `BenchGroup.parent` e camadas de grupo/portas/nomes
por peça preservam os encapsulamentos. Contatos entre chips são congelados como fios antes de
compor; posições ocultas de filhos diferentes não criam contatos. Não foi alterado o solver.
Formato 32 conserva as camadas e valida a ancestralidade; versões anteriores continuam legíveis.
O editor abre contextos isolados sucessivos. Aplicar publica apenas no pai imediato e conserva
a origem dos componentes para que os fios externos sobrevivam à aplicação dos níveis seguintes.
Salvar modelos continua explícito; instâncias não se atualizam automaticamente.
Desagrupar retira somente um nível. Limites globais continuam sendo 256 peças, 512 fios e 256 grupos.
`test_recursive_chips` falhou antes na criação do segundo nível. Agora verifica salvar/reabrir,
nomes, aplicar/copiar/excluir, vinte níveis, ancestralidade inválida, contato entre filhos e
resistores em série no ngspice. `test_app_editor` cobre entrar, descartar, aplicar e desfazer em
dois editores, incluindo a conservação de fios externos.
## Histórico mínimo na sessão contínua (17/11/2026)

Fato: `.save all` fazia os vetores do ngspice crescerem continuamente. Em 10 s do
Cockcroft-Walton a 10 µs, `time` passou de um milhão de valores. O manual do ngspice
documenta `save none` para leitura imediata pela biblioteca compartilhada.
Decisão: a sessão contínua usa `.save none`, restabelecido após cada `delete all`.
O gráfico solicitado com `T` continua usando análise separada e histórico completo.
Tensão, correntes e instrumentos mantiveram os mesmos valores nos testes. O passo
elétrico e os modelos não mudaram. Medidas e limites: [COMPARACAO_TEMPO_REAL.md](COMPARACAO_TEMPO_REAL.md).

## Passo do solver ao vivo separado do gráfico; gráfico ao vivo (17/11/2026, 0.43.0)

Fato: a sessão contínua usava o passo do gráfico (10 µs) como passo máximo do solver. Circuitos
digitais rodavam a 0,03–0,08× por isso, e o mesmo circuito salvo com passo de 1 ms rodava a ~0,8×.
Decisão: o passo máximo ao vivo é 1 ms, ou 1/32 do período da senoide mais rápida. Passo e duração
das Configurações valem para a curva completa (`Shift+T`); a duração também é a janela do gráfico
ao vivo. `T` não pausa mais a simulação: o processo auxiliar amostra o ponto a cada lote pelo
callback de dados do ngspice, sem reter histórico. Um lote que falha (por exemplo, acima de 10 s)
reinicia a sessão no lote seguinte, em vez de deixar todos os seguintes falhando. Medidas,
alternativas e limites: [COMPARACAO_TEMPO_REAL.md](COMPARACAO_TEMPO_REAL.md).

## Paleta única de cores, painel do fio e espelhar (17/11/2026, 0.44.0)

Pedido: mais cores, as mesmas para LEDs e fios; LED apagado mais escuro; escolha da cor do fio fora das
Configurações; espelhar peças para trocar terminais sem estragar mostradores.
Decisão do responsável: paleta de 10 cores (vermelho, verde, azul, amarelo, branco, laranja, roxo, rosa,
ciano, verde-limão), comum a LEDs e fios. A pedido do responsável, o preto foi mantido só para os fios
(não existe LED preto comum); é a última posição da paleta. Cores de arquivos anteriores ao formato 33
mantêm o tom, preto incluído. Preferências na versão 1 são convertidas.
Modelos de junção das cores novas: roxo, rosa e ciano usam o modelo do azul (~2,9 V a 10 mA), como o branco
já usava; verde-limão usa o do verde. É uma aproximação didática, sem curva específica de fabricante.
LED apagado: lente com ~28% da cor acesa (antes ~48%).
O painel do fio ocupa o lugar do painel da peça e mostra origem, destino, corrente (quando os fios são
medidos) e as amostras de cor; a escolha também vale para fios novos.
Espelhar: inversão esquerda-direita no quadro da peça, antes da rotação, aplicada às coordenadas dos
terminais, às pernas do LED, ao quadro dos chips e ao mapeamento de cliques. No desenho, o corpo espelha e as
áreas legíveis (visores, dígitos, telas, LCD, marcas de tensão) são desespelhadas no lugar. Vale para peças
que giram e para chips; breadboard e instrumentos não espelham. Formato 34 salva o espelho de peças e chips.


## Correções da auditoria (18/11/2026, 0.44.1)

Escopo: cinco falhas reproduzidas na branch `fix/audit-runtime`, mantida separada da `main`
a pedido do responsável. Sem alteração de arquitetura, dependências ou formato de projeto.

- O interpretador verifica toda a reserva de variáveis locais antes de escrevê-las. O teste
  `test_mcu_stack` inspeciona essa fronteira: a mensagem de erro final, sozinha, não detectava
  uma escrita indevida anterior. O teste falhou antes da correção.
- `random` trunca os limites, valida valores finitos e usa resto em ponto flutuante para
  evitar divisão inteira por zero e conversão de intervalos grandes para `unsigned`.
  Intervalos vazios ou invertidos continuam retornando o limite inferior; não finitos retornam zero.
- O editor conta o tamanho após normalizar tabulações e CR antes de preencher o rascunho.
  Se não couber, informa o motivo e conserva o programa original. O limite continua 16 KiB,
  incluindo o terminador; projetos continuam aceitando programas de até 256 KiB.
- Durante a pausa, a interface atualiza apenas a referência de tempo da placa, sem executar
  seu programa. Retomar não acrescenta ao relógio da placa o período passado em pausa.
- Alterações de `Part.states`, como posições de DIP, não são mais classificadas como edições
  apenas visuais. A comparação geométrica existente continua preservando resultados ao mover peças.

Regressões: `test_mcu_lang` (limites de random), `test_mcu_stack` (reserva local),
`test_app_simulation` (pausa/retomada e DIP) e `test_app_commands` (abrir, aplicar, salvar e
reabrir código no limite; rejeitar programas maiores e expansão excessiva de tabulações).
Os cinco casos falharam no código anterior; os testes direcionados passaram após as correções.

Validação concluída: `./build.ps1 -GuiTests -OutputName audit-fixes` passou com a suíte completa,
ngspice, 18 exemplos contínuos e autoteste gráfico com PATH mínimo. O nome alternativo foi
necessário porque `build/build.exe` estava aberto; o executável temporário foi removido após
os testes. Não foram gerados pacotes, tag ou release.

## Usabilidade da interface (18/11/2026, 0.45.0)

Escopo autorizado: as cinco prioridades da revisão Impeccable, em `feature/interface-usability`,
a partir da main 0.44.1. Comparação anterior à implementação: [PLANO_INTERFACE.md](PLANO_INTERFACE.md).

- Preservar SDL/OpenGL e os comandos existentes. A nova lista de controles compartilha a geometria
  com desenho e entrada; o foco guarda o alvo geométrico para não saltar quando uma ação fica indisponível.
- Salvar em slot ocupado exige confirmação. O alvo é o slot mostrado, não uma seleção posterior.
  Resumo de peças/fios identifica o conteúdo sem migração do formato 34.
- A preferência existente `gui_font_size` passa a solicitar escala da janela inteira (15 = 100%).
  O desenho mantém coordenadas lógicas e recebe viewport proporcional; mouse e capturas usam a transformação inversa.
  Escala que não cabe no monitor tem recusa visível. Não introduzimos reflow global ou um novo toolkit.
- A busca usa nomes e identificadores do registro do catálogo padrão. A normalização cobre ASCII e Latin-1,
  incluindo português; não acrescentamos biblioteca Unicode. A biblioteca de chips continua na aba Chips.
- Medida e terminal do gráfico ao vivo se referem ao ponto plotado, mesmo com outra peça selecionada.
  Alterar a escolha reinicia a série. Corrente de fio não oferece tensão fictícia.
- Tab percorre controles; Ctrl+Tab conserva a categoria e dá acesso aos controles do editor de código.
  F2 abre a escolha de exemplos; Ctrl+F2 conserva o avanço direto.

Foco de teclado não fornece suporte a leitor de tela. Provedores de UI Automation e validação
em múltiplos monitores/DPI ficam pendentes; a 1.0 não foi declarada. Mapa, capturas e fluxos estão em
[REVISAO_INTERFACE.md](REVISAO_INTERFACE.md).

## CIs de média escala e computador de 8 bits (19/11/2026)

Comparação e fontes: [COMPARACAO_COMPUTADOR_8_BITS.md](COMPARACAO_COMPUTADOR_8_BITS.md).

- Limites fixos sobem para 1024 peças e 2048 fios (decisão do responsável). Listas dinâmicas seguem pendentes.
- Cada CI de média escala gera primitivas digitais do XSPICE; só os pinos cruzam para o analógico.
  Saída de três estados: as duas chaves de saída abrem juntas. Medido: um computador SAP-1 de teste
  (25 CIs, 312 peças) simulou 4 s em 4,8 s com clock de 10 Hz e o passo do modo contínuo.
- O exemplo do computador foi retirado a pedido do responsável (19/11/2026): a montagem gerada ficou
  densa demais para leitura. Os CIs permanecem.
- A AT28C16 é só leitura: o conteúdo em texto vira um decodificador de endereços. Gravação e editor na
  interface ficam pendentes.
- O desenho DIP passa a valer para qualquer número de pinos, com o passo do 74HC00. A AT28C16 usa o
  mesmo corpo estreito; o encapsulamento real de 600 mil não é reproduzido.

## Escala real e nitidez da interface (19/11/2026)

Fatos: a interface era desenhada numa tela lógica fixa de 1280×720 esticada para a janela por `glViewport`.
A "escala" só redimensionava a janela (e tirava a janela do modo maximizado), e o texto, rasterizado a 15 px
pelo GDI, era ampliado com filtro linear, ficando borrado; janelas fora de 16:9 eram distorcidas. Não havia
antisserrilhado. Revisão guiada pela skill Impeccable (modo Operate; "reestruturar, não esticar").

- A tela lógica passa a ser a janela dividida pela escala (preferência × escala do Windows), nunca menor que
  1280×720. Painéis guardam o desenho de 1280×720 e seguem âncoras: centro (diálogos), direita (inspetor,
  editor, gráfico), borda inferior (rodapé). A bancada e o catálogo crescem. Testes: `tests/test_app_layout.c`.
- Texto rasterizado em pixels reais e posicionado em pixels inteiros quando não há rotação.
- MSAA 4× pedido ao criar a janela, com nova tentativa sem ele se o driver recusar.
- Sombras de deslocamento sólido trocadas por sombra suave em camadas translúcidas.
- Pendente: leitor de tela; um layout que reorganize os painéis (e não só os ancore) em telas muito largas.

## Proteção pelos datasheets, fonte CV/CC e coleção de CIs (20/11/2026, 0.52.0)

Fontes oficiais e datas: `docs/datasheets/LOGICA_74HC.md` e `docs/datasheets/ANALOGICOS_E_LIMITES.md`
(TI, Microchip, Espressif, Vishay, onsemi, Nichicon, Rigol; consultadas em 20/11/2026). Valores marcados
"(reconstr.)" vieram de tabelas mal extraídas e não viraram constantes, exceto onde indicado.

- **Política de queima (decisão do responsável, 20/11):** acima do máximo absoluto de corrente, aviso; a partir de
  1,8× o máximo, queima. 1,8 e não 2 porque duas saídas 74HC opostas a 5 V passam exatamente 2× (5 V em 100 Ω)
  e devem queimar. Tensão de alimentação acima do máximo absoluto queima na hora; acima do recomendado, aviso.
  A peça queimada abre as saídas (fonte `alive` do CI ou resistor `Rgout` da porta solta) até a ligação mudar,
  como o LED.
- **Verificação fora do solver:** uma árvore de `max()` em fontes B dentro do ngspice custou 6× no circuito do
  slot 2 (0,10× contra 0,58×) e foi descartada. A sessão lê os nós em lote (pedido `M` do host), até 21 saídas
  por passo em rodízio, e todas as alimentações a cada passo. A detecção tem atraso de alguns passos e nenhum
  custo de equações. Ler as respostas do host em blocos, em vez de byte a byte, acelerou todas as leituras.
- **Chave contada como ligada** só acima de 0,6 do controle (Vt + Vh do modelo SW): estado lógico indefinido
  leva os dois controles a 0,5 e não pode contar como meio curto.
- **Fonte de bancada CV/CC:** fonte B em série com queda `-G·suave(I - Ilim)` controlada por I(Vsource), G = 10⁴ V/A
  (curto a 30 V passa 3 mA do limite). A forma controlada pela própria tensão (`Ilim·tanh(V/…)`) é plana no início
  `uic` da sessão e falhou o primeiro passo. Consequência aceita: a corrente do diodo em série no teste DC
  difere da do resistor em 3e-4, dentro do reltol de 1e-3.
- **Placas:** ATmega328P/ATtiny85 40 mA por pino, pinos até VCC + 0,5 V, ATtiny85 VCC máximo 6 V; ESP32 até
  3,6 V nos pinos e cerca de 40 mA (valor típico; a Espressif não publica máximo absoluto por pino).
- **Entrada solta:** análise só das ligações, recalculada quando a montagem ou um controle muda. Avisa apenas
  entradas em uso (portas soltas; portas de CI com saída ligada; CIs médios alimentados; pino de placa ligado a
  botão), para não inundar de avisos as portas não usadas.
- **CIs lineares** (`netlist_linear.c`): modelos ideais sem banda, slew rate nem temperatura. O LM358 tem um polo
  de 1 µs só para quebrar o laço algébrico de ganho 10⁵ no início da sessão (sem ele não convergia). O LM7805 usa a
  mesma queda controlada por corrente da fonte de bancada, bloqueando também a corrente reversa. Nesses CIs só a
  alimentação é verificada; corrente de saída não (pendente).
- **Formato 38:** novos tipos entram no fim do enum e de `legacy_types` (código do arquivo = índice).
- **Pendentes conhecidos:** estado de registradores ao ligar continua 0 (real: indefinido); corrente de entrada
  acima de VCC (diodos de grampo, ±20 mA) não é verificada; fusível do multímetro e capacitor eletrolítico
  invertido não são modelados.

# Fonte AC (26/11/2026)

A fonte AC usa a forma `SIN(0 amplitude frequencia)` do ngspice, amplitude de pico de 0,1 a 30 V,
frequencia de 0,1 a 1000 Hz e resistencia serie de 0,1 ohm. O controle de frequencia e salvo no
campo `control` existente; o tipo novo exige formato 40. Sem carga, a tensao oscila entre os
picos positivo e negativo. Analise DC ve o ponto de 0 V. A comparacao esta em
`COMPARACAO_FONTE_AC_2026_11_26.md`.
