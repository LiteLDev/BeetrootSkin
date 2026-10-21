# Undo hud_jitter_still.prelaunch.ps1.
. "$PSScriptRoot/../lib.ps1"
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"
if (Test-Path "$cfg.jitter-e2e") { Move-Item "$cfg.jitter-e2e" $cfg -Force }
elseif (Test-Path $cfg) { Remove-Item $cfg -Force }
Write-Step "hud_jitter_still: config restored"
