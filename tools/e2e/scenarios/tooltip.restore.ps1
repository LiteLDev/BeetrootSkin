# Undo tooltip.prelaunch.ps1.
. "$PSScriptRoot/../lib.ps1"
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"
if (Test-Path "$cfg.tooltip-e2e") { Move-Item "$cfg.tooltip-e2e" $cfg -Force }
elseif (Test-Path $cfg) { Remove-Item $cfg -Force }
Write-Step "tooltip: config restored"
