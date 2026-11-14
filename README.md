# Simulador de elétrica e eletrônica

Projeto de um aplicativo educacional nativo para Windows x64, principalmente em C. A proposta é aprender montando e simulando circuitos em uma área de trabalho 2.5D.

A visão, o escopo proposto e as orientações de trabalho estão em [prompt.md](prompt.md). As decisões provisórias estão em [DECISOES.md](DECISOES.md).

O primeiro incremento é um circuito fixo de fonte, interruptor e LED em console. Use `i` para alternar o interruptor, `f` para alternar a fonte e `q` para sair. É um modelo booleano didático, sem cálculo elétrico ou edição de conexões; não representa precisão de SPICE.

Há também uma janela experimental SDL3/OpenGL: os três blocos representam fonte, interruptor e LED, nessa ordem, identificados por símbolos. Clique com o botão esquerdo na fonte ou interruptor para alterná-los, ou use `F` e `I`. Arraste componentes com o botão direito. A roda do mouse ajusta o zoom, as setas deslocam a vista e `Home` a restaura; `Esc` fecha. A barra de título mostra os estados. Para compilar e testar com GCC e SDL3 do MSYS2 UCRT64 instalados:

```powershell
./build.ps1
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
