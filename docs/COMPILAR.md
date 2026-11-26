# Compilar no Windows

Para usar o programa sem compilar, baixe um pacote na
[página de releases](https://github.com/Reyralona/electronics/releases).

## Preparar o ambiente

1. Instale o [MSYS2](https://www.msys2.org/). Adicione os executáveis UCRT64 e `bsdtar` ao `PATH` da sessão de compilação.
2. Abra o terminal **MSYS2 UCRT64** e atualize os pacotes:

```sh
pacman -Syu
```

Se o comando pedir para fechar o terminal, feche, abra novamente e repita.
Depois instale o compilador e a SDL3:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sdl3
```

## Compilar e testar

Os scripts procuram `gcc.exe` e `bsdtar.exe` no `PATH`; se não estiverem lá, usam a instalação
padrão do MSYS2 (`C:\msys64`) ou a pasta indicada em `MSYS2_ROOT`, só na sessão atual.
Clone o projeto numa pasta sem espaços no caminho.
Essa restrição é do script de preparação abaixo; os pacotes distribuídos aceitam espaços.

```powershell
git clone https://github.com/Reyralona/electronics.git
cd electronics
./tools/setup-ngspice.ps1
./build.ps1 -GuiTests
./build/build.exe
```

Para compilar o aplicativo e seus auxiliares sem executar testes, use `./build-notests.ps1`.
Feche a Bancada antes para que o Windows permita atualizar `build/build.exe`.

O script de preparação baixa e verifica as dependências do ngspice.
Ele preserva um destino existente. Não precisa executá-lo em todo build.
O build exige o runtime: verifica os arquivos essenciais e a referência DC antes de compilar.
A referência DC e o teste da sessão contínua rodam com PATH mínimo, sem depender do MSYS2.
Se faltar um arquivo ou uma DLL não carregar, o build para com instruções de recuperação.
Para refazer uma instalação incompleta, mova `build/spice-local` para backup e execute
`./tools/setup-ngspice.ps1` novamente; o script não sobrescreve uma instalação existente.
O build compila com `-Wall -Wextra -Werror` e roda a suíte de testes.
`-GuiTests` inclui o autoteste da interface. Testes do solver usam o runtime preparado.

Para verificar apenas os diagnósticos de instalação, execute `./tests/test_ngspice_runtime.ps1`.
Esse teste usa arquivos temporários, sem modificar o runtime instalado. Foi confirmado em
19/11/2026 que ele falha antes da correção da 0.47.1 e passa depois dela; verifica também
que uma falha ao iniciar o solver restaura o PATH da sessão.
Validação da 0.47.1: `./build.ps1 -GuiTests` passou em 19/11/2026, incluindo solver e autoteste gráfico.

Se o PowerShell bloquear um script, execute apenas aquela chamada com:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -GuiTests
```

Essa chamada não altera a política de execução permanentemente.

## CMake e pacotes

O fluxo de distribuição usa CMake/Ninja, CPack e NSIS.
Com as ferramentas instaladas e o código commitado, `Empacotar.cmd` prepara o runtime,
compila e testa os pacotes. Os comandos estão em [DISTRIBUICAO_WINDOWS.md](DISTRIBUICAO_WINDOWS.md).
`build.ps1` continua sendo o comando da suíte completa; CTest cobre os testes do pacote.

## Organização do código

| Pasta | Conteúdo |
|---|---|
| `src/` | Modelo, simulação e interface. Os módulos `app_*` cuidam da interface. |
| `tests/` | Testes nativos e circuitos de referência. |
| `tools/` | Preparação, testes, capturas e publicação. |
| `packaging/` | Instalador, dependências fixadas e instruções dos pacotes. |
| `docs/` | Guias, requisitos, decisões e registros históricos. |

Siga [QUALIDADE_CODIGO.md](QUALIDADE_CODIGO.md) e [VERSIONAMENTO.md](VERSIONAMENTO.md).
Para retomar trabalho anterior, consulte o fim de [CONTINUIDADE.md](CONTINUIDADE.md).
