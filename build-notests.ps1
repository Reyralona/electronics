param([ValidatePattern('^[A-Za-z0-9_-]+$')][string]$OutputName = 'build')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'tools/msys2-path.ps1')
Add-Msys2ToPath
$compiler = Get-Command gcc.exe -ErrorAction Stop
$toolchain = Split-Path $compiler.Source -Parent
New-Item -ItemType Directory -Force build | Out-Null
$ngspiceBin = 'build/spice-local/ucrt64/bin'
if (!(Test-Path $ngspiceBin)) {
    throw "ngspice ausente: execute ./tools/setup-ngspice.ps1 antes do build."
}
foreach ($dll in 'SDL3.dll', 'libiconv-2.dll') {
    $source = Join-Path $toolchain $dll
    if (!(Test-Path $source)) { throw "Dependencia ausente: $source" }
    Copy-Item -LiteralPath $source -Destination build -Force
}
foreach ($dll in 'libgcc_s_seh-1.dll', 'libgomp-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll') {
    $destination = Join-Path $ngspiceBin $dll
    if (!(Test-Path $destination)) { Copy-Item (Join-Path $toolchain $dll) $destination }
}
& gcc -std=c11 -Wall -Wextra -Werror -shared -Isrc src/eeprom_cosim.c src/eeprom.c -o build/bancada-eeprom.dll
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Copy-Item -LiteralPath build/bancada-eeprom.dll -Destination (Join-Path $ngspiceBin 'bancada-eeprom.dll') -Force
& gcc -O2 -std=c11 -Wall -Wextra -Werror '-Wl,--stack,67108864' -Isrc src/spice_live_host.c src/ui_text.c -o build/spice-live-host.new.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
try { Move-Item -Force build/spice-live-host.new.exe build/spice-live-host.exe -ErrorAction Stop }
catch {
    $hostExe = Get-Item build/spice-live-host.exe -ErrorAction SilentlyContinue
    if ($null -eq $hostExe -or $hostExe.LastWriteTime -lt (Get-Item src/spice_live_host.c).LastWriteTime) {
        throw 'spice-live-host.exe esta em uso e desatualizado; feche a Bancada e tente novamente.'
    }
    Write-Warning 'spice-live-host.exe esta em uso; mantendo a versao atual.'
    Remove-Item build/spice-live-host.new.exe
}
& gcc -O2 -std=c11 -Wall -Wextra -Werror '-Wl,--stack,67108864' src/workbench_window.c src/workbench_grid.c src/app_text_utf8.c src/app_commands.c src/app_editing.c src/app_settings.c src/app_preferences.c src/app_layout.c src/app_packing.c src/app_editor.c src/workbench_group_edit.c src/workbench_selection.c src/workbench_pack.c src/app_input.c src/app_controls.c src/app_code_editor.c src/app_render.c src/app_palette.c src/app_state.c src/mcu_runtime.c src/mcu_lang.c src/code_text.c src/app_simulation.c src/sim_state.c src/dev_capture.c src/dev_scenarios.c src/workbench_fit.c src/workbench.c src/bench_path.c src/component_registry.c src/chip_docs.c src/component_devices.c src/workbench_examples.c src/workbench_view.c src/workbench_file.c src/workbench_pack_file.c src/workbench_group_name.c src/workbench_history.c src/netlist.c src/netlist_protection.c src/netlist_linear.c src/netlist_devices.c src/netlist_logic.c src/rom_image.c src/mcu_board.c src/spice_process.c src/spice_job.c src/spice_live.c src/spice_session.c src/spice_trace.c src/sim_auto.c src/ui_text.c -o "build/$OutputName.exe" -lSDL3 -lopengl32 -lgdi32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output "Compilacao concluida sem testes: build/$OutputName.exe"
