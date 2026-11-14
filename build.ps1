param([switch]$Legacy, [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$OutputName = 'bancada')
$ErrorActionPreference = 'Stop'
$toolchain = 'C:\msys64\ucrt64\bin'
if (Test-Path (Join-Path $toolchain 'gcc.exe')) { $env:Path = "$toolchain;$env:Path" }
New-Item -ItemType Directory -Force build | Out-Null
& gcc -std=c11 -Wall -Wextra -Werror src/workbench_window.c src/workbench.c src/workbench_file.c src/netlist.c src/spice_process.c -o "build/$OutputName.exe" -lSDL3 -lopengl32 -lgdi32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench.c src/workbench.c -o build/test_workbench.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_file.c src/workbench_file.c src/workbench.c -o build/test_workbench_file.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_file.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_netlist.c src/netlist.c src/workbench.c -o build/test_netlist.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_netlist.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_spice_process.c src/spice_process.c src/netlist.c src/workbench.c -o build/test_spice_process.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_spice_process.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Legacy) {
    & gcc -std=c11 -Wall -Wextra -Werror src/window.c src/circuit.c src/project.c src/history.c src/simulation.c -o build/window.exe -lSDL3 -lopengl32 -lgdi32
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_circuit.c src/circuit.c -o build/test_circuit.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_circuit.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_project.c src/project.c -o build/test_project.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_project.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_history.c src/history.c -o build/test_history.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_history.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_simulation.c src/simulation.c src/circuit.c -o build/test_simulation.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_simulation.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output "Compilação e testes concluídos. Execute .\build\$OutputName.exe nesta sessão."
