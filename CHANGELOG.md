# Histórico de versões

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
