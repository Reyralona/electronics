param([switch]$GuiTests, [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$OutputName = 'build')
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
# The host may be locked by an open Bancada session; keep it if it is not older than its source.
& gcc -std=c11 -Wall -Wextra -Werror -Isrc src/spice_live_host.c -o build/spice-live-host.new.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
try { Move-Item -Force build/spice-live-host.new.exe build/spice-live-host.exe -ErrorAction Stop }
catch {
    $hostExe = Get-Item build/spice-live-host.exe -ErrorAction SilentlyContinue
    if ($null -eq $hostExe -or $hostExe.LastWriteTime -lt (Get-Item src/spice_live_host.c).LastWriteTime) {
        Write-Error "spice-live-host.exe is locked and older than src/spice_live_host.c; close Bancada and rebuild."
        exit 1
    }
    Write-Warning "spice-live-host.exe is in use; keeping the current host (source unchanged)."
    Remove-Item build/spice-live-host.new.exe -ErrorAction SilentlyContinue
}
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_spice_session.c src/spice_session.c -o build/test_spice_session.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_spice_live.c src/spice_live.c src/spice_session.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_spice_live.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path "$ngspiceBin/ngspice_con.exe") {
    & ./build/test_spice_session.exe "$ngspiceBin/ngspice_con.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & ./build/test_spice_live.exe "$ngspiceBin/ngspice_con.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& gcc -std=c11 -Wall -Wextra -Werror src/workbench_window.c src/workbench_grid.c src/app_text_utf8.c src/app_commands.c src/app_editing.c src/app_settings.c src/app_preferences.c src/app_layout.c src/app_packing.c src/app_editor.c src/workbench_group_edit.c src/workbench_selection.c src/workbench_pack.c src/app_input.c src/app_code_editor.c src/app_render.c src/app_palette.c src/app_state.c src/mcu_runtime.c src/mcu_lang.c src/code_text.c src/app_simulation.c src/sim_state.c src/dev_capture.c src/dev_scenarios.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c src/workbench_examples.c src/workbench_view.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/workbench_history.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/spice_process.c src/spice_job.c src/spice_live.c src/spice_session.c src/spice_trace.c src/sim_auto.c -o "build/$OutputName.exe" -lSDL3 -lopengl32 -lgdi32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_app_preferences.c src/app_preferences.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_app_preferences.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_app_preferences.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_leads.c src/workbench_history.c src/workbench_selection.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c -o build/test_leads.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path "$ngspiceBin/ngspice_con.exe") { & ./build/test_leads.exe "$ngspiceBin/ngspice_con.exe" } else { & ./build/test_leads.exe }
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_wire_path.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_wire_path.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_wire_path.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Isrc tests/test_mcu_lang.c src/mcu_lang.c -o build/test_mcu_lang.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_mcu_lang.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_code_text.c src/code_text.c src/mcu_lang.c -o build/test_code_text.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_code_text.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_mcu.c src/mcu_runtime.c src/mcu_board.c src/mcu_lang.c src/spice_live.c src/spice_session.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/component_devices.c src/workbench.c src/bench_path.c src/component_registry.c -o build/test_mcu.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path "$ngspiceBin/ngspice_con.exe") { & ./build/test_mcu.exe "$ngspiceBin/ngspice_con.exe" }
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_analog_catalog.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/workbench_history.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/spice_live.c src/spice_session.c src/sim_auto.c -o build/test_analog_catalog.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path "$ngspiceBin/ngspice_con.exe") { & ./build/test_analog_catalog.exe "$ngspiceBin/ngspice_con.exe" } else { & ./build/test_analog_catalog.exe }
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_catalog.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c -o build/test_catalog.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path "$ngspiceBin/ngspice_con.exe") { & ./build/test_catalog.exe "$ngspiceBin/ngspice_con.exe" } else { & ./build/test_catalog.exe }
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_bench_path.c src/bench_path.c -o build/test_bench_path.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_bench_path.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_component_registry.c src/component_registry.c src/component_devices.c -o build/test_component_registry.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_component_registry.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_revision.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c src/workbench_history.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c -o build/test_workbench_revision.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_revision.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_selection.c src/workbench_selection.c src/workbench_pack.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_workbench_selection.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_selection.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_pack.c src/workbench_examples.c src/workbench_pack.c src/workbench_selection.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_workbench_pack.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path 'build/spice-local/ucrt64/bin/ngspice_con.exe') {
    & ./build/test_workbench_pack.exe 'build/spice-local/ucrt64/bin/ngspice_con.exe'
} else {
    & ./build/test_workbench_pack.exe
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_group_edit.c src/workbench_group_edit.c src/workbench_group_name.c src/workbench_pack.c src/workbench_selection.c src/workbench_file.c src/workbench_pack_file.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_workbench_group_edit.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path 'build/spice-local/ucrt64/bin/ngspice_con.exe') {
    & ./build/test_workbench_group_edit.exe 'build/spice-local/ucrt64/bin/ngspice_con.exe'
} else {
    & ./build/test_workbench_group_edit.exe
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_sim_state.c src/sim_state.c src/spice_trace.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_sim_state.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_sim_state.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_app_editor.c src/app_simulation.c src/sim_state.c src/spice_trace.c src/app_commands.c src/app_editing.c src/app_settings.c src/app_preferences.c src/app_layout.c src/app_packing.c src/app_editor.c src/workbench_group_edit.c src/workbench_selection.c src/workbench_pack.c src/app_state.c src/mcu_runtime.c src/mcu_lang.c src/code_text.c src/app_palette.c src/workbench_examples.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/workbench_history.c src/spice_job.c src/spice_live.c src/spice_session.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_app_editor.exe -lSDL3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_app_editor.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_app_commands.c src/sim_state.c src/spice_trace.c src/app_commands.c src/app_editing.c src/app_settings.c src/app_preferences.c src/app_layout.c src/app_packing.c src/app_editor.c src/workbench_group_edit.c src/workbench_selection.c src/workbench_pack.c src/app_state.c src/mcu_runtime.c src/mcu_lang.c src/code_text.c src/app_palette.c src/workbench_examples.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/workbench_history.c src/spice_job.c src/spice_live.c src/spice_session.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_app_commands.exe -lSDL3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_app_commands.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_app_live.c src/workbench_history.c src/app_state.c src/mcu_runtime.c src/mcu_lang.c src/code_text.c src/app_simulation.c src/sim_state.c src/spice_trace.c src/spice_job.c src/spice_live.c src/spice_session.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_app_live.exe -lSDL3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path "$ngspiceBin/ngspice_con.exe") {
    & ./build/test_app_live.exe "$ngspiceBin/ngspice_con.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_app_simulation.c src/workbench_history.c src/app_state.c src/mcu_runtime.c src/mcu_lang.c src/code_text.c src/app_simulation.c src/sim_state.c src/spice_trace.c src/spice_job.c src/spice_live.c src/spice_session.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_app_simulation.exe -lSDL3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_app_simulation.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_sim_auto.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_sim_auto.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$ngspiceExe = Join-Path $ngspiceBin 'ngspice_con.exe'
if (Test-Path $ngspiceExe) { & ./build/test_sim_auto.exe $ngspiceExe } else { & ./build/test_sim_auto.exe }
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_workbench.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_file.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_workbench_file.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_file.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_history.c src/workbench_history.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_workbench_history.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_history.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_grid.c src/workbench_grid.c -o build/test_workbench_grid.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_grid.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_view.c src/workbench_view.c -o build/test_workbench_view.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_view.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_netlist.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_netlist.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_netlist.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_workbench_examples.c src/workbench_examples.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_workbench_examples.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_workbench_examples.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_examples_live.c src/workbench_examples.c src/spice_live.c src/spice_session.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_examples_live.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path "$ngspiceBin/ngspice_con.exe") { & ./build/test_examples_live.exe "$ngspiceBin/ngspice_con.exe" }
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_devices.c src/spice_live.c src/spice_session.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/component_devices.c src/workbench.c src/bench_path.c src/component_registry.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c -o build/test_devices.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path "$ngspiceBin/ngspice_con.exe") { & ./build/test_devices.exe "$ngspiceBin/ngspice_con.exe" }
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_spice_trace.c src/spice_trace.c -o build/test_spice_trace.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_spice_trace.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_spice_process.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_spice_process.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_spice_process.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_spice_job.c src/spice_job.c src/spice_live.c src/spice_session.c src/sim_auto.c src/spice_process.c src/netlist.c src/netlist_devices.c src/mcu_board.c src/workbench.c src/bench_path.c src/component_registry.c src/component_devices.c -o build/test_spice_job.exe -lSDL3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_spice_job.exe
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
