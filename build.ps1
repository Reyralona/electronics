$ErrorActionPreference = 'Stop'
$toolchain = 'C:\msys64\ucrt64\bin'
if (Test-Path (Join-Path $toolchain 'gcc.exe')) { $env:Path = "$toolchain;$env:Path" }
New-Item -ItemType Directory -Force build | Out-Null
& gcc -std=c11 -Wall -Wextra -Werror src/window.c src/circuit.c -o build/window.exe -lSDL3 -lopengl32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_circuit.c src/circuit.c -o build/test_circuit.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_circuit.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output 'Compilação e teste concluídos. Execute .\build\window.exe nesta sessão.'
