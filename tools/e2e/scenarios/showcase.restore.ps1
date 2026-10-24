# Undo showcase.prelaunch.ps1 (options.txt is restored by run.ps1).
. "$PSScriptRoot/../lib.ps1"
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"
if (Test-Path "$cfg.showcase-e2e") { Move-Item "$cfg.showcase-e2e" $cfg -Force }
elseif (Test-Path $cfg) { Remove-Item $cfg -Force }
Write-Step "showcase: config restored"
