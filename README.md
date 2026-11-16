# Bancada: simulador de circuitos

Aplicativo para Windows que serve para aprender eletrônica montando circuitos numa bancada virtual. Você coloca fontes, resistores, LEDs, portas lógicas, CIs, sensores e até um Arduino, liga tudo com fios ou numa breadboard e vê o circuito funcionar: LEDs acendem conforme a corrente calculada, capacitores carregam e descarregam, clocks piscam.

O cálculo elétrico é feito pelo [ngspice](https://ngspice.sourceforge.io/), um simulador de circuitos de código aberto. A interface é escrita em C com [SDL3](https://www.libsdl.org/) e OpenGL.

![Bancada com um CI 74HC00 numa breadboard, um clock e um LED](docs/img/bancada.png)

> **Estado do projeto:** em desenvolvimento (versão 0.36.0, antes da 1.0). Funciona só no Windows 10/11 de 64 bits, e a interface oferece português (Brasil) e inglês (EUA). Os modelos elétricos são aproximações didáticas: servem para aprender, não para dimensionar um circuito real.

## O que dá para fazer

- **Montar circuitos** com mais de 50 tipos de peça: fontes e baterias, resistores, capacitores, indutores, potenciômetro, LEDs (inclusive RGB), lâmpada, diodos, transistores, interruptores, botões, relé, motores, servo, display de 7 segmentos, sensores (luz, temperatura, gás, distância e outros) e instrumentos (multímetro, osciloscópio, gerador de função).
- **Usar uma breadboard** de 420 contatos com quatro trilhos de alimentação. As peças encaixam nos furos.
- **Lógica digital:** portas AND, OR, NOT, NAND, NOR, XOR e XNOR, flip-flop D, clock e os CIs 74HC00/08/32/86.
- **Programar microcontroladores** (Arduino Uno, ESP32, ATtiny85) num subconjunto da linguagem do Arduino, com monitor serial e LCD 16x2 I2C.
- **Simular ao vivo:** a simulação roda continuamente enquanto você mexe no circuito, e dá para pausar, reiniciar e ver gráficos de tensão e corrente ao longo do tempo.
- **Criar seus próprios chips:** selecione um pedaço do circuito, transforme num bloco reutilizável, dê um nome e use quantas cópias quiser.
- **Começar pelos exemplos:** há 18 circuitos prontos com explicação curta.

## Instalação

O projeto pode gerar um ZIP portátil e um instalador experimental para Windows x64. Eles ainda
não estão publicados como release. Para gerar os pacotes, veja
[Distribuição Windows](docs/DISTRIBUICAO_WINDOWS.md); para compilar pelo fluxo anterior, siga abaixo.

Ao receber um pacote do projeto: extraia o ZIP inteiro e abra `bancada.exe`, ou execute o arquivo
`-setup.exe`. Não é preciso instalar MSYS2 ou ngspice para usar os pacotes. O instalador funciona
por usuário, sem administrador. Os artefatos ainda não têm assinatura digital.

### 1. Instale o MSYS2

O MSYS2 fornece o compilador C (GCC) e as bibliotecas. Baixe em <https://www.msys2.org/> e instale no caminho padrão, **`C:\msys64`** (os scripts procuram nele).

### 2. Instale o GCC e a SDL3

Abra o terminal **MSYS2 UCRT64** (pelo menu Iniciar) e rode:

```sh
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sdl3
```

Se o primeiro comando pedir para fechar o terminal, feche, abra de novo e repita.

### 3. Baixe o projeto

Num terminal **PowerShell**, numa pasta **sem espaços no caminho** (exigência do ngspice por enquanto):

```powershell
git clone https://github.com/Reyralona/electronics.git
cd electronics
```

Sem Git, use **Code → Download ZIP** no GitHub e extraia.

### 4. Prepare o ngspice

```powershell
./tools/setup-ngspice.ps1
```

O script baixa o ngspice pelos pacotes do MSYS2, confere a integridade e instala uma cópia isolada em `build/spice-local`, sem mexer no resto do sistema.

### 5. Compile e abra

```powershell
./build.ps1
./build/build.exe
```

O `build.ps1` compila o programa e roda os testes automáticos. No fim deve aparecer "Compilação e testes concluídos".

> Se o PowerShell recusar os scripts ("execução de scripts foi desabilitada"), rode uma vez
> `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`, ou chame cada script com
> `powershell -ExecutionPolicy Bypass -File .\build.ps1`.

## Primeiros passos

1. Clique em **Exemplos** no topo e escolha **LED na breadboard**.
2. Dê dois cliques na fonte para desligá-la e ligá-la: o LED acompanha.
3. Escolha uma peça no catálogo à esquerda e clique na bancada para colocá-la.
4. Para ligar um fio, clique num terminal (as bolinhas brancas) e depois em outro. Cliques em pontos vazios no caminho criam dobras.
5. Selecione uma peça para ver e editar seus valores no painel à direita.

Os comandos principais:

| Ação | Como |
|---|---|
| Colocar peça | Clique no catálogo, depois na bancada |
| Ligar fio | Clique num terminal e depois em outro |
| Ligar/desligar fonte ou interruptor | Dois cliques na peça |
| Girar | `R` |
| Editar valor | `E` ou clique em **Valor** no painel |
| Apagar | `Delete` |
| Desfazer / refazer | `Ctrl+Z` / `Ctrl+Y` |
| Pausar / continuar a simulação | `Espaço` |
| Zoom / mover a vista | Roda do mouse / arrastar com o botão do meio |
| Salvar / carregar projeto | `Ctrl+S` / `Ctrl+L` (escolha o slot em **Projetos**) |
| Criar chip com a seleção | `Ctrl+G` |

Todos os atalhos e recursos estão no **[guia de uso](docs/GUIA.md)**.

## Onde ficam seus arquivos

Projetos, preferências e chips são salvos em pastas relativas à **pasta de onde o programa foi aberto**:

- `saves/slot-1.elec` … `slot-9.elec`: os 9 slots de projeto, mais `preferences.txt`;
- `blocks/block-1.elec` … `block-8.elec`: a biblioteca de chips. Chips removidos vão para `blocks/lixeira`.

Abrindo com dois cliques em `build/build.exe`, elas ficam dentro de `build/`. Rodando `./build/build.exe` a partir da raiz do projeto, ficam na raiz. Nenhuma das duas vai para o Git.

Nos pacotes: o ZIP usa `saves/` e `blocks/` ao lado de `bancada.exe`; a versão instalada usa
`%APPDATA%/Bancada/Bancada`. Desinstalar preserva os dados. Não há migração automática de projetos antigos.

## Limites atuais

- **Só Windows x64.** Não há versão para Linux ou macOS.
- **Distribuição experimental:** os pacotes incluem o ngspice; ainda faltam assinatura digital e teste numa VM Windows limpa.
- **Modelos didáticos:** LEDs, transistores, CIs e sensores usam parâmetros genéricos. Uma entrada lógica desligada é lida como 0; num CI real ela ficaria indefinida.
- **Capacidade fixa:** até 64 peças e 128 fios por montagem, 9 slots de projeto e 8 chips na biblioteca.
- **Microcontroladores** rodam um subconjunto da linguagem do Arduino (sem ponteiros, `String` ou bibliotecas além do LCD I2C), não o código de máquina real.

O que falta para a 1.0 está em [docs/ROADMAP_1_0.md](docs/ROADMAP_1_0.md).

## Para desenvolver

```powershell
./build.ps1 -GuiTests   # compila, roda todos os testes e o autoteste da interface (abre uma janela)
```

| Pasta | Conteúdo |
|---|---|
| `src/` | Código C. O modelo do circuito (`workbench*`, `netlist*`, `component_*`) não depende de SDL/OpenGL; a interface fica em `app_*`. |
| `tests/` | Testes automáticos. Os que usam o ngspice rodam quando ele está instalado. |
| `tools/` | Scripts de preparação do ngspice, capturas de tela para revisão visual e testes do runtime. |
| `docs/` | Guia de uso, decisões técnicas, roadmap e diretrizes de qualidade. |

A arquitetura e as convenções de código estão em [docs/QUALIDADE_CODIGO.md](docs/QUALIDADE_CODIGO.md), e as regras de branches e versões em [docs/VERSIONAMENTO.md](docs/VERSIONAMENTO.md). O histórico de versões fica no [CHANGELOG.md](CHANGELOG.md).

### Documentação

| Documento | Para quê |
|---|---|
| [docs/GUIA.md](docs/GUIA.md) | Guia de uso completo: montagem, simulação, chips, microcontroladores e atalhos |
| [docs/TEXTOS.md](docs/TEXTOS.md) | Guia de estilo e glossário da interface em português e inglês |
| [docs/DISTRIBUICAO_WINDOWS.md](docs/DISTRIBUICAO_WINDOWS.md) | Pesquisa, geração e testes do ZIP e instalador Windows |
| [docs/DECISOES.md](docs/DECISOES.md) | Decisões técnicas e de modelo elétrico, com fontes e datas |
| [docs/ROADMAP_1_0.md](docs/ROADMAP_1_0.md) | Histórico consolidado e o que falta para a 1.0 |
| [docs/META_1_0.md](docs/META_1_0.md) | Critérios para declarar a versão 1.0 |
| [docs/CATALOGO_COMPONENTES.md](docs/CATALOGO_COMPONENTES.md) | Componentes pedidos, implementados e pendentes |
| [docs/QUALIDADE_CODIGO.md](docs/QUALIDADE_CODIGO.md) | Revisão de código e direção de arquitetura |
| [docs/QUALIDADE_DE_USO.md](docs/QUALIDADE_DE_USO.md) | Critérios de usabilidade e de instrumentos |
| [docs/LOGICA_DIGITAL.md](docs/LOGICA_DIGITAL.md) | Como a lógica digital é simulada (XSPICE) |
| [docs/REVISAO_VISUAL.md](docs/REVISAO_VISUAL.md) | Revisão da arte dos componentes por capturas |
| [docs/VISAO.md](docs/VISAO.md) | Visão original do produto e escopo do MVP |
| [docs/VERSIONAMENTO.md](docs/VERSIONAMENTO.md) | Versões, branches e tags |
| [docs/CONTINUIDADE.md](docs/CONTINUIDADE.md) | Diário de trabalho: ponto de retomada entre sessões |
| [AGENTS.md](AGENTS.md) | Instruções para agentes de IA que trabalham no código |

## Licença

[GNU GPL versão 3](LICENSE). Você pode usar, estudar, modificar e redistribuir o programa. Quem distribuir uma versão modificada precisa mantê-la sob a mesma licença e publicar o código-fonte.

Dependências que o projeto não inclui no repositório: a [SDL3](https://www.libsdl.org/) (licença zlib) e o [ngspice](https://ngspice.sourceforge.io/), que o `tools/setup-ngspice.ps1` baixa dos pacotes do MSYS2. Cada um segue a sua própria licença.
