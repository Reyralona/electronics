@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\package-windows.ps1"
if errorlevel 1 (
    echo.
    echo Falha. Consulte docs\DISTRIBUICAO_WINDOWS.md e a mensagem acima.
    pause
    exit /b 1
)
start "" "%~dp0build\dist"
