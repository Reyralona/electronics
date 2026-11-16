# Histórico de versões

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
