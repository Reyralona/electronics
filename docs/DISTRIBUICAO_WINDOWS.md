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
