# Simulador de elétrica e eletrônica

Projeto de um aplicativo educacional nativo para Windows x64, principalmente em C. A proposta é aprender montando e simulando circuitos em uma área de trabalho 2.5D.

A visão, o escopo proposto e as orientações de trabalho estão em [prompt.md](prompt.md). As decisões provisórias estão em [DECISOES.md](DECISOES.md).

O primeiro incremento é um circuito fixo de fonte, interruptor e LED em console. Use `i` para alternar o interruptor, `f` para alternar a fonte e `q` para sair. É um modelo booleano didático, sem cálculo elétrico ou edição de conexões; não representa precisão de SPICE.

Há também uma janela experimental SDL3/OpenGL: os três blocos representam fonte, interruptor e LED, nessa ordem. Use `F` para alternar a fonte, `I` para o interruptor e `Esc` para sair; a barra de título mostra os estados. Para compilar com GCC e SDL3 do MSYS2 UCRT64 instalados:

```powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
New-Item -ItemType Directory -Force build
gcc -std=c11 -Wall -Wextra -Werror src/window.c src/circuit.c -o build/window.exe -lSDL3 -lopengl32
./build/window.exe
```

A DLL SDL3 deve estar no PATH ou junto do executável. A janela é uma prova de integração, ainda sem edição de circuitos, texto na cena ou apresentação 2.5D.

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
