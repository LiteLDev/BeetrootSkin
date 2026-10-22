# Undo api.prelaunch.ps1: remove the test-only mod and restore the config.
. "$PSScriptRoot/../lib.ps1"
Remove-ExtraMod 'beetroot-api-sample'
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"
if (Test-Path "$cfg.api-e2e") { Move-Item "$cfg.api-e2e" $cfg -Force }
elseif (Test-Path $cfg) { Remove-Item $cfg -Force }
Write-Step "api: config restored"
