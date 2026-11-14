# Decisões e próximos passos

Consulta: 14/11/2026.

Fatos: a visão está em `prompt.md`. GCC do MSYS2 UCRT64 foi encontrado neste computador; a presença no PATH não é pressuposta. A [Microsoft](https://learn.microsoft.com/en-us/windows/win32/learnwin32/prepare-your-development-environment) documenta suporte do Windows SDK a C e Windows x64. [Win32](https://learn.microsoft.com/en-us/cpp/windows) oferece APIs nativas de interface; [GDI](https://learn.microsoft.com/en-us/windows/win32/gdi/windows-gdi) oferece desenho e texto para C/C++. A [documentação SDL3 para Windows](https://wiki.libsdl.org/SDL3/README-windows) descreve compilação com MinGW-w64 e CMake.

Alternativas para investigar: Win32/GDI reduz dependências externas e facilita uma janela inicial, mas exige trabalho manual de interface e desenho; SDL3 fornece uma camada de janelas e renderização multiplataforma, com dependência e distribuição adicionais e necessidade de solução para UI acessível. A escolha definitiva, versões, licenças e distribuição ainda precisam de comparação e verificação. Nenhuma biblioteca gráfica foi escolhida.

Decisão provisória: começar com um modelo discreto em C independente da interface, exposto por um programa de console. O circuito tem topologia fixa e o LED acende somente quando fonte e interruptor estão ligados. É uma regra booleana didática, sem tensão, corrente, polaridade, resistência, curto ou precisão física. Não é SPICE e não diagnostica circuitos. Simulação por eventos e solucionadores analógicos continuam alternativas para estudo quando o escopo exigir.

Próximos incrementos: escolher e testar janela e desenho 2.5D; representar componentes, terminais e conexões editáveis; incluir tutorial e persistência versionada. Cada etapa precisa de critérios verificáveis e validação antes de expandir. São pendentes a API gráfica, a versão mínima do Windows, a distribuição, a licença do projeto e a extensão do modelo elétrico.
