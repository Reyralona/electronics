# Codex Stop hook: mostra um lembrete local sem bloquear o encerramento do turno.
$notification = @'
Add-Type -AssemblyName System.Windows.Forms
$icon = New-Object System.Windows.Forms.NotifyIcon
$icon.Icon = [System.Drawing.SystemIcons]::Information
$icon.Visible = $true
$icon.BalloonTipTitle = 'Codex pausou'
$icon.BalloonTipText = 'Se houver trabalho pendente, envie: continua.'
$icon.ShowBalloonTip(10000)
Start-Sleep -Seconds 12
$icon.Dispose()
'@
try {
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($notification))
    Start-Process -FilePath 'powershell.exe' -WindowStyle Hidden -ArgumentList @('-NoProfile', '-EncodedCommand', $encoded) | Out-Null
} catch {
    [Console]::Error.WriteLine('Nao foi possivel iniciar o lembrete do Codex.')
}
[Console]::Out.WriteLine('{}')
