# Decisões e próximos passos

Consulta: 14/11/2026.

Fatos: a visão está em `prompt.md`. GCC do MSYS2 UCRT64 foi encontrado neste computador; a presença no PATH não é pressuposta. A [Microsoft](https://learn.microsoft.com/en-us/windows/win32/learnwin32/prepare-your-development-environment) documenta suporte do Windows SDK a C e Windows x64. [Win32](https://learn.microsoft.com/en-us/cpp/windows) oferece APIs nativas de interface; [GDI](https://learn.microsoft.com/en-us/windows/win32/gdi/windows-gdi) oferece desenho e texto para C/C++. A [documentação SDL3 para Windows](https://wiki.libsdl.org/SDL3/README-windows) descreve compilação com MinGW-w64 e CMake.

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

Correcao P0 em 14/11/2026: setup solicita ngspice, gcc-libs e libwinpthread explicitamente ao pacman, preservando verificacao de integridade e rejeitando runtime sem as quatro DLLs. Uma instalacao nova em build/spice-quality-runtime passou nos testes automaticos DC/transitorios e exemplos com PATH limitado ao Windows. Falhas do processo exibem codigo de saida, incluindo DLL ausente. Executavel canonico build/bancada.exe conforme politica incorporada de main.

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
