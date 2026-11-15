# Histórico de versões

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
