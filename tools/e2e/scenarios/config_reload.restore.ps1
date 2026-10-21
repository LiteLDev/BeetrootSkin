# Undo config_reload.prelaunch.ps1: put the original config back (or remove the scenario's).
. "$PSScriptRoot/../lib.ps1"
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"
if (Test-Path "$cfg.config_reload-e2e") { Move-Item "$cfg.config_reload-e2e" $cfg -Force }
elseif (Test-Path $cfg) { Remove-Item $cfg -Force }
Write-Step "config_reload: config restored"
