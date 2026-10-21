# Undo smoke.prelaunch.ps1.
. "$PSScriptRoot/../lib.ps1"
$marker = Join-Path $script:ModsDir "$script:ModName\debug_draw"
if (Test-Path $marker) { Remove-Item $marker -Force; Write-Step "debug drawing disabled" }
