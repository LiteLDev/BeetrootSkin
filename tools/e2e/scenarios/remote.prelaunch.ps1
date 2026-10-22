# Runs before the game starts: start the official Bedrock Dedicated Server (fetched by
# tools/e2e/get-bds.ps1) with a copy of the fixture world, make run.ps1 join it instead of the
# local world, and start with the food debug info hidden (F6 turns it on in the scenario; the
# previous value is put back by remote.restore.ps1).
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"
if (Test-Path $cfg) {
    $json = Get-Content $cfg -Raw | ConvertFrom-Json
    $saved = Join-Path (Get-UnitOutDir 'remote') 'showFoodDebugInfo.orig'
    New-Item -ItemType Directory -Force (Split-Path $saved) | Out-Null
    # Kept from an interrupted run: that one holds the user's value.
    if (-not (Test-Path $saved)) { Set-Content $saved ([string]($json.showFoodDebugInfo -eq $true)) }
    $json | Add-Member -Force -NotePropertyName showFoodDebugInfo -NotePropertyValue $false
    $json | ConvertTo-Json -Depth 8 | Set-Content $cfg -Encoding utf8
}
Remove-BdsServerEntries   # leftovers of an interrupted run
Start-Bds $script:Unit
$script:EnterWorld = { Join-Bds }
