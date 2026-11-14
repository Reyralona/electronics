# Decisões e próximos passos

Consulta: 14/11/2026.

Fatos: a visão está em `prompt.md`. GCC do MSYS2 UCRT64 foi encontrado neste computador; a presença no PATH não é pressuposta. A [Microsoft](https://learn.microsoft.com/en-us/windows/win32/learnwin32/prepare-your-development-environment) documenta suporte do Windows SDK a C e Windows x64. [Win32](https://learn.microsoft.com/en-us/cpp/windows) oferece APIs nativas de interface; [GDI](https://learn.microsoft.com/en-us/windows/win32/gdi/windows-gdi) oferece desenho e texto para C/C++. A [documentação SDL3 para Windows](https://wiki.libsdl.org/SDL3/README-windows) descreve compilação com MinGW-w64 e CMake.

Decisão do usuário: SDL e OpenGL. SDL3, sob [licença zlib](https://github.com/libsdl-org/SDL), fornece a janela e eventos; OpenGL desenha a cena. A primeira experiência usa OpenGL 1.1 para validar a integração, sem fixar a versão definitiva. No Windows, funções posteriores à 1.1 exigem carregamento em tempo de execução, conforme a [documentação SDL](https://wiki.libsdl.org/SDL3/SDL_GL_CreateContext). A interface acessível e a distribuição, incluindo a DLL e a licença SDL, precisam ser definidas antes de um lançamento. Win32/GDI permanece uma alternativa sem dependência gráfica externa, mas exigiria trabalho próprio de interface e desenho.

Decisão provisória: começar com um modelo discreto em C independente da interface, exposto por um programa de console. O circuito tem topologia fixa e o LED acende somente quando fonte e interruptor estão ligados. É uma regra booleana didática, sem tensão, corrente, polaridade, resistência, curto ou precisão física. Não é SPICE e não diagnostica circuitos. Simulação por eventos e solucionadores analógicos continuam alternativas para estudo quando o escopo exigir.

Próximos incrementos: evoluir o desenho para 2.5D; representar componentes, terminais e conexões editáveis; incluir tutorial e persistência versionada. Cada etapa precisa de critérios verificáveis e validação antes de expandir. São pendentes a versão definitiva do OpenGL, a versão mínima do Windows, a distribuição, a licença do projeto e a extensão do modelo elétrico.
