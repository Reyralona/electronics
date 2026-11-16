# Distribuição Windows

## Comparação técnica (consulta em 16/11/2026)

Fatos verificados nas fontes oficiais:

| Referência | Como distribui | Aplicação aqui |
|---|---|---|
| [Wireshark](https://www.wireshark.org/docs/wsdg_html_chunked/ChSrcBinary) | Build CMake, instalador NSIS e alternativas portáteis | Separar build de empacotamento |
| [VLC](https://www.videolan.org/vlc/download-windows.html) e [script oficial](https://github.com/videolan/vlc/blob/master/extras/package/win32/NSIS/vlc.win32.nsi.in) | Instalador NSIS e arquivos ZIP/7z | Oferecer instalador e ZIP do mesmo conteúdo |
| [CMake/CPack](https://cmake.org/cmake/help/latest/module/CPack.html) | Empacota os arquivos declarados com install() | Um manifesto para os arquivos distribuídos |
| [NSIS](https://nsis.sourceforge.io/Docs/Chapter4.html) | RequestExecutionLevel user permite instalação sem elevação | Instalar em LocalAppData, registro HKCU |
| [MSYS2](https://www.msys2.org/dev/package-licensing/) | Metadados e arquivos de licença dos pacotes | Registrar versões, origem, hashes e licenças |

Alternativas: manter só PowerShell reduz a mudança inicial, mas mantém a compilação em listas de
comandos; WiX/MSI acrescenta ferramentas e conceitos sem necessidade atual. CMake não é instalador,
não baixa automaticamente um compilador e não substitui a verificação do pacote.

Decisão: CMake para build incremental e install(), CPack ZIP e um script NSIS pequeno para o
instalador por usuário. Manter build.ps1 como suíte de regressão existente neste incremento.
Não migrar o modelo elétrico nem introduzir gerenciador de dependências adicional.

Hipótese a verificar: os binários UCRT64 preparados funcionam fora do MSYS2 quando todas as DLLs
não pertencentes ao Windows acompanham o pacote. Testar com PATH mínimo, diretório movido e espaços.
Os testes locais não substituem uma VM Windows limpa; documentar essa limitação.

Critérios: simulação DC e ao vivo relocáveis; instalação sem administrador; arquivos do usuário
fora da instalação; desinstalação preserva dados; hashes SHA256; licenças e origem das dependências.
Não distribuir saves pessoais nem ferramentas do compilador. Não publicar automaticamente artefatos
como release estável. Assinatura Authenticode depende de identidade/certificado do mantenedor e fica
pendente; checksums detectam mudanças, mas não autenticam o editor por si só.

## Gerar os pacotes

Pré-requisito do desenvolvedor: MSYS2 em `C:/msys64`. No terminal UCRT64:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-nsis
```

Com o código commitado, dê dois cliques em `Empacotar.cmd` ou execute no PowerShell:

```powershell
./tools/package-windows.ps1
```

O script baixa os pacotes e fontes fixados em `packaging/dependencies.lock.json`, confere SHA256,
prepara um runtime privado e compila contra a SDL3 desse runtime. O MSYS2 instalado fornece apenas
as ferramentas de build. `build.ps1` continua disponível para a suíte completa de regressão.
Nenhuma alteração permanente de PATH ou de política do PowerShell é feita por Empacotar.cmd.

Saída em `build/dist/`: `Bancada-VERSAO-windows-x64.zip`,
`Bancada-VERSAO-windows-x64-setup.exe`, `Bancada-VERSAO-sources.zip` e
`Bancada-VERSAO-SHA256SUMS.txt`. Distribuir os quatro arquivos juntos na mesma página de download.
O arquivo de fontes inclui o projeto no commit do build e os arquivos MSYS2 com fontes e patches
das dependências. GCC runtime usa sua exceção de runtime, com licença e URL da origem incluídas.
Isso não constitui uma promessa de builds idênticos byte a byte entre versões do compilador.

O ZIP conserva a pasta `spice-local` e todas as DLLs ao lado de `bancada.exe`.
O instalador NSIS não baixa nem executa código da internet durante a instalação, usa HKCU e
`LocalAppData/Programs/Bancada` e cria atalhos no menu Iniciar do usuário. Não modifica o PATH.
Instalações atualizadas no mesmo diretório preservam projetos fora dele. A desinstalação remove
somente a lista de arquivos distribuídos, preservando outros arquivos adicionados naquela pasta.

Dados: o marcador `portable.mode` usa a pasta do executável; `installed.mode` usa
`%APPDATA%/Bancada/Bancada`. Sem marcador, o build de desenvolvimento mantém o diretório atual.
Não há migração automática dos saves antigos; copiar `saves/` e `blocks/` com o programa fechado.
`--print-data-directory` permite verificar o destino sem abrir ou modificar projetos.
Os caminhos ainda usam MAX_PATH em partes do solver; caminhos muito longos ou caracteres fora da
página de código Windows não estão validados neste incremento.

## Verificações

O empacotamento compila com `-Wall -Wextra -Werror`, mantendo asserts ativos nos testes mesmo em
Release. CTest executa o catálogo de textos e o autoteste GUI. `test-package.ps1` executa testes DC,
transientes digitais, sessões RC/DFF, simulação ao vivo e captura da UI com PATH mínimo, num
diretório com espaços. Confere também o diretório de dados dos modos portátil e instalado.
`test-installer.ps1` compila o mesmo NSIS com chave de registro/atalhos exclusivos para o teste,
instala silenciosamente numa pasta de teste, repete os testes e desinstala, preservando um arquivo
adicionado. Não substitui uma instalação real. `-SkipTests` é apenas para diagnóstico local.

O pacote usa ngspice 45.2, já empregado pelo projeto, sem atualizar o solver junto ao empacotamento.
O código de `src/spicelib/devices/dev.c` desse ngspice resolve modelos relativos contra o executável
do processo. O host usa esse comportamento no layout canônico do bundle, evitando nomes curtos 8.3.
Instalações alternativas do runtime mantêm o caminho anterior; não misturar runtimes no bundle.

Pendentes antes de uma distribuição estável: VM limpa Windows 10/11, assinatura Authenticode,
pipeline automático por tag e teste de atualização entre duas versões publicadas. Não desligar Defender,
SmartScreen ou outras proteções para executar o programa.

## Publicar cada versão

A partir de 0.36.0, cada versão distribuída tem uma GitHub Release com instalador, ZIP portátil,
fontes correspondentes e SHA256SUMS. Versões antigas não ganham binários reconstruídos sem validação.
Enquanto os critérios de distribuição estável estiverem pendentes, publicar como pré-release.

Após compilar e testar, integrar a main e enviar a tag anotada ao remoto. Com GitHub CLI autenticado:

```powershell
./tools/publish-release.ps1 -Tag v0.36.0
```

O comando verifica os três hashes, a versão dentro do ZIP, a ancestralidade do commit compilado e
a ausência de mudanças no código/empacotamento entre o build e a tag. Cria um rascunho, envia os quatro
arquivos, confere tamanho e digest SHA256 fornecido pelo GitHub e publica como pré-release. Não substitui
arquivos de releases já publicadas. A opção `-ResumeDraft` retoma somente um rascunho existente com
os mesmos arquivos, inclusive após uma falha de rede. Se o upload ficou incompleto, completar os
arquivos no rascunho antes de retomar; não tornar público um conjunto parcial.

Este comando é uma etapa explícita por versão; ainda não há workflow automático ao enviar tags.
Notas são geradas do CHANGELOG da própria tag. Usar os artefatos aprovados pelos testes completos;
não publicar resultados de `-SkipTests`. Fonte oficial da operação:
[GitHub CLI release create](https://cli.github.com/manual/gh_release_create), consultada em 16/11/2026.
