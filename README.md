# Simulador de elétrica e eletrônica

Projeto de um aplicativo educacional nativo para Windows x64, principalmente em C. A proposta é aprender montando e simulando circuitos em uma área de trabalho 2.5D.

A visão, o escopo proposto e as orientações de trabalho estão em [prompt.md](prompt.md). As decisões provisórias estão em [DECISOES.md](DECISOES.md).

O primeiro incremento é um circuito fixo de fonte, interruptor e LED em console. Use `i` para alternar o interruptor, `f` para alternar a fonte e `q` para sair. É um modelo booleano didático, sem cálculo elétrico ou edição de conexões; não representa precisão de SPICE.

Há também uma janela experimental SDL3/OpenGL: os três blocos representam fonte, interruptor e LED, nessa ordem. Clique com o botão esquerdo na fonte ou interruptor para alterná-los, ou use `F` e `I`. Arraste componentes com o botão direito; `Esc` fecha. A barra de título mostra os estados. Para compilar e testar com GCC e SDL3 do MSYS2 UCRT64 instalados:

```powershell
./build.ps1
./build/window.exe
```

A DLL SDL3 deve estar no PATH ou junto do executável. As posições não são salvas; as ligações e a topologia são fixas. A janela ainda não tem texto na cena nem apresentação 2.5D.

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
