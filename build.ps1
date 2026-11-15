param([switch]$Legacy, [switch]$GuiTests, [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$OutputName = 'bancada')
$ErrorActionPreference = 'Stop'
$toolchain = 'C:\msys64\ucrt64\bin'
if (Test-Path (Join-Path $toolchain 'gcc.exe')) { $env:Path = "$toolchain;$env:Path" }
New-Item -ItemType Directory -Force build | Out-Null
# Runtime DLLs: the app and ngspice must start outside this session, without MSYS2 on the PATH.
$ngspiceBin = 'build/spice-local/ucrt64/bin'
if (Test-Path (Join-Path $toolchain 'SDL3.dll')) {
    if (!(Test-Path 'build/SDL3.dll')) { Copy-Item (Join-Path $toolchain 'SDL3.dll') 'build/' }
    if (Test-Path $ngspiceBin) {
        foreach ($dll in 'libgcc_s_seh-1.dll', 'libgomp-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll') {
            if (!(Test-Path (Join-Path $ngspiceBin $dll))) { Copy-Item (Join-Path $toolchain $dll) $ngspiceBin }
        }
    }
}
& gcc -std=c11 -Wall -Wextra -Werror src/workbench_window.c src/app_commands.c src/app_editing.c src/app_packing.c src/workbench_selection.c src/workbench_pack.c src/app_input.c src/app_render.c src/app_palette.c src/app_state.c src/app_simulation.c src/sim_state.c src/dev_capture.c src/dev_scenarios.c src/workbench.c src/component_registry.c src/workbench_examples.c src/workbench_view.c src/workbench_file.c src/workbench_pack_file.c src/workbench_history.c src/netlist.c src/spice_process.c src/spice_job.c src/spice_trace.c src/sim_auto.c -o "build/$OutputName.exe" -lSDL3 -lopengl32 -lgdi32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_component_registry.c src/component_registry.c -o build/test_component_registry.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_component_registry.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_revision.c src/workbench.c src/component_registry.c src/workbench_history.c src/workbench_file.c src/workbench_pack_file.c -o build/test_workbench_revision.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_revision.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_selection.c src/workbench_selection.c src/workbench_pack.c src/workbench.c src/component_registry.c -o build/test_workbench_selection.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_selection.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_pack.c src/workbench_examples.c src/workbench_pack.c src/workbench_selection.c src/workbench_file.c src/workbench_pack_file.c src/spice_process.c src/netlist.c src/workbench.c src/component_registry.c -o build/test_workbench_pack.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path 'build/spice-local/ucrt64/bin/ngspice_con.exe') {
    & ./build/test_workbench_pack.exe 'build/spice-local/ucrt64/bin/ngspice_con.exe'
} else {
    & ./build/test_workbench_pack.exe
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_sim_state.c src/sim_state.c src/spice_trace.c src/workbench.c src/component_registry.c -o build/test_sim_state.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_sim_state.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_app_commands.c src/sim_state.c src/spice_trace.c src/app_commands.c src/app_editing.c src/app_packing.c src/workbench_selection.c src/workbench_pack.c src/app_state.c src/app_palette.c src/workbench_examples.c src/workbench_file.c src/workbench_pack_file.c src/workbench_history.c src/spice_job.c src/sim_auto.c src/spice_process.c src/netlist.c src/workbench.c src/component_registry.c -o build/test_app_commands.exe -lSDL3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_app_commands.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_app_simulation.c src/workbench_history.c src/app_state.c src/app_simulation.c src/sim_state.c src/spice_trace.c src/spice_job.c src/sim_auto.c src/spice_process.c src/netlist.c src/workbench.c src/component_registry.c -o build/test_app_simulation.exe -lSDL3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_app_simulation.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_sim_auto.c src/sim_auto.c src/spice_process.c src/netlist.c src/workbench.c src/component_registry.c -o build/test_sim_auto.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$ngspiceExe = Join-Path $ngspiceBin 'ngspice_con.exe'
if (Test-Path $ngspiceExe) { & ./build/test_sim_auto.exe $ngspiceExe } else { & ./build/test_sim_auto.exe }
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench.c src/workbench.c src/component_registry.c -o build/test_workbench.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_file.c src/workbench_file.c src/workbench_pack_file.c src/workbench.c src/component_registry.c -o build/test_workbench_file.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_file.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_history.c src/workbench_history.c src/workbench.c src/component_registry.c -o build/test_workbench_history.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_history.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_view.c src/workbench_view.c -o build/test_workbench_view.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_view.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_netlist.c src/netlist.c src/workbench.c src/component_registry.c -o build/test_netlist.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_netlist.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_examples.c src/workbench_examples.c src/spice_process.c src/netlist.c src/workbench.c src/component_registry.c -o build/test_workbench_examples.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_examples.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_spice_trace.c src/spice_trace.c -o build/test_spice_trace.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_spice_trace.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_spice_process.c src/spice_process.c src/netlist.c src/workbench.c src/component_registry.c -o build/test_spice_process.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_spice_process.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_spice_job.c src/spice_job.c src/sim_auto.c src/spice_process.c src/netlist.c src/workbench.c src/component_registry.c -o build/test_spice_job.exe -lSDL3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_spice_job.exe
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
if ($GuiTests) {
    $guiTest = Start-Process -FilePath (Resolve-Path "build/$OutputName.exe") -ArgumentList '--self-test-history' -WindowStyle Hidden -PassThru -RedirectStandardOutput 'build/gui-test.out' -RedirectStandardError 'build/gui-test.err'
    $null = $guiTest.Handle
    if (!$guiTest.WaitForExit(10000)) { Stop-Process -Id $guiTest.Id; throw 'Teste gráfico excedeu dez segundos.' }
    $guiTest.Refresh()
    Get-Content 'build/gui-test.out'
    if ($guiTest.ExitCode -ne 0) { Get-Content 'build/gui-test.err'; throw "Teste gráfico falhou: $($guiTest.ExitCode)" }
}
Write-Output "Compilação e testes concluídos. Execute .\build\$OutputName.exe nesta sessão."
