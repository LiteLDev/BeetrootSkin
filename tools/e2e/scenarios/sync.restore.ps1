# Undo what sync.ps1 may have left behind when it failed midway (the world fixture is restored
# by every run, so only the force_client_sync marker needs removing).
. "$PSScriptRoot/../lib.ps1"
$marker = Join-Path $script:ModsDir "$script:ModName\force_client_sync"
if (Test-Path $marker) { Remove-Item $marker -Force; Write-Step "force_client_sync marker removed" }
