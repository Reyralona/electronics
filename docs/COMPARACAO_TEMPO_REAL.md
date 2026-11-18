# Simulação próxima do tempo real — comparação inicial

Consulta: 17/11/2026. Escopo: investigação técnica revisável; nenhuma mudança de backend foi decidida aqui.

## Fatos

- O projeto já mantém uma sessão ngspice em processo separado (`spice_session.c`, `spice_live.c`). O roadmap registra estado contínuo para capacitor e DFF, mas reinício ao mudar topologia ou frequência, crescimento dos vetores e tempo simulado visível muito abaixo do tempo de parede.
- A [API oficial da biblioteca compartilhada](https://ngspice.sourceforge.io/shared.html) permite iniciar, interromper e retomar a simulação, alterar parâmetros e receber dados em cada ponto calculado.
- O [manual atualizado do ngspice](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf), seção 15.4.2, descreve `bg_run`, `bg_halt` e `bg_resume` e informa que outros comandos são recusados enquanto o cálculo de fundo está em execução.

## Hipóteses a medir

- Para circuitos pequenos, o custo de troca de mensagens, coleta de vetores e atualização da UI pode ser relevante frente ao custo do solver.
- Para circuitos grandes ou modelos difíceis de convergir, o solver e o tamanho do passo dominarão. A linguagem C, isoladamente, não garante uma razão de 1 segundo simulado por 1 segundo real.
- Reduzir a quantidade de vetores coletados e limitar sua retenção pode reduzir memória e tempo sem alterar o resultado elétrico; isso exige comparar os valores antes e depois.

## Alternativas e recomendação

| Alternativa | Vantagem | Custo ou risco |
|---|---|---|
| Sessão atual, com instrumentação | Preserva isolamento do processo e dá uma linha de base | Ainda há comunicação por mensagens e crescimento dos vetores |
| Biblioteca compartilhada no processo da UI | Callback por amostra e controle direto | Falha do solver pode derrubar a UI; precisa thread, sincronização e política de recuperação |
| Biblioteca compartilhada no processo auxiliar | Callback por amostra e isolamento preservado | Implementação de transporte e controle mais complexa |

Recomendação: medir primeiro, na sessão atual, tempo de parede e tempo simulado, custo de avanço do solver, mensagens, renderização, número de amostras/vetores e memória em circuitos RC, portas e circuitos maiores reproduzíveis. Exibir a razão observada na interface após validação. Só então testar coleta limitada e callbacks no processo auxiliar, comparando resultados com a sessão atual.

## Medição inicial e incremento de agendamento

17/11/2026, Windows local, circuito RC de `tests/test_spice_live.c`, passo elétrico de 10 µs. Com `BANCADA_BENCH_LIVE=1`, 20 chamadas sucessivas de 1 ms simulado levaram 2,062 s reais (0,010×); uma chamada de 20 ms simulados levou 0,110 s (0,182×). São medidas de um circuito e uma máquina, sem promessa de desempenho para outros modelos. O teste normal de regressão continuou passando.

Execução: `./build.ps1 -GuiTests`; para repetir a medição no PowerShell, definir `$env:BANCADA_BENCH_LIVE='1'` e executar `./build/test_spice_live.exe ./build/spice-local/ucrt64/bin/ngspice_con.exe`. O executável do teste é compilado pelo build; o script ajusta o PATH para o toolchain UCRT64.

O agendador passou a solicitar até 20 ms simulados por lote, com até 2.000 passos do valor configurado, conforme o tempo de parede decorrido. A UI continua em outra thread. Isso reduz chamadas de controle sem alterar o passo elétrico escolhido. A memória dos vetores ngspice ainda cresce durante a sessão; medir e limitar a retenção continua sendo requisito antes de anunciar operação contínua em tempo real.

A barra inferior mostra uma estimativa suavizada da razão entre tempo simulado aceito e tempo real entre amostras (`1.00x` = tempo real). A estimativa inclui espera e comunicação com o processo auxiliar; não é apenas a velocidade interna do solver.

Experimento adicional: trocar a leitura de resposta do pipe de um byte por vez para blocos não mudou o benchmark RC (2,062 s em 20 lotes; 0,110 s em um lote). A alteração foi descartada. A próxima investigação deve separar tempo de avanço do ngspice, espera por resposta e coleta de vetores; o tamanho da leitura do pipe não foi o gargalo observável nesse caso.

A espera fixa de 1 ms ao consultar o pipe mostrou ser o custo dominante para comandos curtos neste Windows. A resposta agora cede a CPU com `Sleep(0)` durante os primeiros 5 ms e volta a `Sleep(1)` para respostas demoradas. Em três execuções do RC, 20 lotes de 1 ms levaram 0,023–0,041 s reais (0,49–0,86×); um lote de 20 ms levou cerca de 0,003 s (5,86–5,99×). Um lote de 20 ms no circuito de quatro entradas AND levou 0,024 s (0,85×). A diferença entre lotes reforça que o controle por chamada ainda importa. Esses resultados não se estendem automaticamente a circuitos maiores ou sessões longas; o crescimento dos vetores continua pendente.

## Controle de ritmo solicitado (17/11/2026)

Fato: o agendador pedia um intervalo simulado proporcional ao tempo de parede desde o lote
anterior, limitado a 20 ms e 2.000 passos. O passo elétrico já era configurado à parte.
Hipótese: multiplicar apenas o intervalo solicitado por um fator menor que 1 reduz o ritmo
observado sem alterar a netlist, as frequências dos componentes nem a resolução elétrica.
Alternativa considerada: aumentar o passo elétrico para acelerar o cálculo. Isso alteraria a
resolução da análise e foi descartado para este controle.
Decisão: oferecer 0,05×, 0,1×, 0,25×, 0,5× e 1× como preferência da sessão contínua.
O fator limita a solicitação, não garante a razão observada se o solver for mais lento.
Guardar na preferência, não no arquivo do circuito: o ritmo é uma escolha de visualização.
As fontes oficiais e data de consulta da comparação técnica estão no início deste documento.

## Decisões pendentes

- Meta de razão mínima e circuitos de referência para declarar “tempo real”.
- Política para ritmos mais rápidos que 1× e circuitos que não acompanham o ritmo escolhido.
- Perfil da variação residual de desempenho com retenção mínima do histórico.

## Retenção do histórico na sessão contínua (17/11/2026)

Fato: o aplicativo executava `delete all` antes de cada avanço e mantinha `.save all`
na netlist da sessão. No Cockcroft-Walton de `tests/fixtures/`, a 1 Hz e passo de 10 µs,
o vetor `time` cresceu até 1.001.511 valores em 10 s simulados. Cinquenta lotes de
20 ms passaram de cerca de 0,47 s reais no primeiro segundo a cerca de 4–5 s no décimo.
Os valores variam entre execuções e com a carga da máquina.

A [documentação oficial do ngspice](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf)
descreve `save none` para uso da biblioteca compartilhada: o programa pode ler o último
valor sem reter toda a curva. O teste isolado com o ngspice 45.2 empacotado confirmou
`length(time) == 1` e acesso ao último valor. `delete all` remove a regra de `save`,
portanto ela precisa ser restabelecida antes de cada avanço.

Decisão implementada: a netlist da sessão contínua usa `.save none`, e `advance` emite
`save none` após `delete all`. A análise de gráfico solicitada com `T` continua separada
e guarda sua curva inteira. O teste do Cockcroft-Walton preservou as tensões e leituras
dos instrumentos; o vetor `time` permaneceu com 1 valor até 10 s. No mesmo benchmark,
cinquenta lotes de 20 ms custaram cerca de 0,4–1,1 s reais por segundo simulado.
A variação residual ainda precisa de perfil de CPU/solver para garantir 1,00× em
circuitos diferentes. O passo elétrico configurado não foi alterado.

No RC montado pelo próprio `tests/test_spice_live.c`, 500 lotes de 20 ms com passo de
10 µs levaram 0,19–0,33 s reais por segundo simulado durante os 10 s. Execução:
`$env:BANCADA_BENCH_LIVE_LONG='1'; ./build/test_spice_live.exe ./build/spice-local/ucrt64/bin/ngspice_con.exe`.
É uma medição de uma máquina, não garantia de tempo real para todos os circuitos.

Experimento descartado: o pacote ngspice 47-1 foi baixado em `build/` e verificado pelo
SHA256 publicado pelo MSYS2. Com `tran` de longa duração, essa versão tentou reservar
gigabytes antes de calcular o primeiro ponto, mesmo com `save none`. O pacote fixado
permanece em 45.2; não houve mudança de dependência nem de modelo elétrico.

## Passo do solver ao vivo, recuperação e gráfico ao vivo (17/11/2026)

Fontes: as mesmas já registradas neste documento, consultadas em 17/11/2026: a
[API da biblioteca compartilhada](https://ngspice.sourceforge.io/shared.html), que descreve o
callback de dados enviado a cada ponto calculado, e o
[manual do ngspice](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf) (`.tran` com
passo máximo `TMAX`; fontes `PULSE` geram pontos de quebra nos cantos da onda). O comportamento
abaixo foi confirmado em testes locais com o ngspice 45.2 empacotado.

### Fatos medidos

- O app usava o "passo" das Configurações (padrão 10 µs) como `TMAX` da sessão contínua
  (`tran passo 1e12 0 passo uic`). Cada lote de 20 ms custava pelo menos 2.000 pontos, mesmo
  com o circuito parado. O custo por ponto cresce com o número de portas: cada porta vira ADC,
  porta XSPICE, DAC, fontes `B` e duas chaves `SW`. Com `rusage`, quase todo o tempo ficava na
  avaliação dos dispositivos (load), e pouco na fatoração da matriz.
- Mesmo computador (Ryzen 5 5500), lotes de 20 ms, tempo real por lote:

  | Circuito | passo 10 µs | passo máximo 1 ms |
  |---|---|---|
  | decodificador BCD (~30 portas, sem clock) | ~240–260 ms | ~11 ms |
  | contador JK + decodificador, clock 10 Hz | ~380 ms | ~18 ms |
  | contador 4 bits + BCD (398 fontes `B`, 204 chaves) | ~820 ms | ~33 ms |

  O ritmo de 0,8× observado em outro computador vinha de um circuito salvo com passo de 1 ms;
  com o passo padrão, o mesmo arquivo roda a ~0,025× aqui. Nem o build nem o ngspice explicavam
  a diferença.
- Com clock de 10 kHz, as bordas dominam: ~21.000 pontos aceitos e ~3.400 rejeitados em 20 ms.
  Um lote pode passar de 10 s. Depois desse estouro, a resposta atrasada do processo auxiliar
  respondia o pedido seguinte, e todos os lotes falhavam com "resultado incompleto".
- Com `save none`, o callback de dados do ngspice continua entregando todos os vetores a cada
  ponto, sem reter histórico. Registrar o callback não mudou o custo medido (slot de
  ~30 portas: ~240 ms por lote, contra ~260 ms antes).

### Alternativas consideradas

| Alternativa | Vantagem | Custo ou risco |
|---|---|---|
| Manter `TMAX` = passo do gráfico | Nada muda | Lento em todo circuito digital; o "passo" vira uma configuração escondida de desempenho |
| `TMAX` fixo de 1 ms | Simples; bordas de clock e quadradas continuam como pontos de quebra | Uma senoide rápida ficaria com poucos pontos |
| `TMAX` = 1 ms, ou 1/32 do período da senoide mais rápida | Mantém a resolução da senoide | Ainda não cobre fontes futuras sem pontos de quebra |
| Gráfico ao vivo com `save <nó>` | Usa vetores comuns | O vetor cresce durante toda a sessão (o problema resolvido pelo `save none`) |
| Gráfico ao vivo com várias paradas por lote | Não muda o processo auxiliar | Cada parada custa ~1–2 ms de ida e volta |
| Gráfico ao vivo pelo callback de dados, amostrado no processo auxiliar | Sem histórico no ngspice; uma resposta por lote | Protocolo novo (`P`, `S`) e limite de amostras por resposta |

### Decisões

- Passo máximo da sessão contínua: 1 ms, ou 1/32 do período da senoide mais rápida do gerador
  de funções. Clock e ondas quadrada/triangular dependem dos pontos de quebra das fontes `PULSE`.
  Capacitores e indutores continuam sob o controle de erro do solver. O passo e a duração das
  Configurações passam a valer só para a curva completa (`Shift+T`); a duração também define a
  janela do gráfico ao vivo.
- O lote ao vivo encolhe quando o ritmo observado é baixo, mirando ~0,1 s de tempo real por lote.
- Um lote que falha fecha o processo auxiliar; o próximo recria a sessão em t = 0 e avisa
  ("sessão reiniciada"). O teste reproduz o estouro com um limite de 1 ms por pedido.
- `T` abre o gráfico ao vivo: o processo auxiliar amostra o ponto em grade regular (400 amostras
  por janela), com interpolação linear entre pontos do solver, e devolve até 256 amostras por lote.
  Se um lote gerar mais, a resolução guardada cai pela metade. `Shift+T` mantém a curva completa
  antiga, calculada de 0 até a duração com a simulação pausada.

### Limites e pendências

- O gráfico ao vivo mostra o que o solver calculou: entre dois pontos do solver a curva é uma
  reta. Com passo máximo de 1 ms, uma rampa lenta aparece em segmentos de até 1 ms.
- Mudanças de topologia ainda reiniciam a sessão. Mudar o número de peças ou fios fecha o
  gráfico ao vivo, porque o índice sondado pode passar a apontar outra peça.
- Não há meta de razão mínima definida. Clocks de kHz com dezenas de portas continuam bem abaixo
  de 1×.
