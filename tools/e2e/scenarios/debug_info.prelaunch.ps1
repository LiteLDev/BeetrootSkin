# Runs before the game starts: make sure the food debug info starts hidden (the deployed
# config\ survives between runs), so the scenario's first F6 press turns it on.
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"
if (Test-Path $cfg) {
    $json = Get-Content $cfg -Raw | ConvertFrom-Json
    $json.showFoodDebugInfo = $false
    $json | ConvertTo-Json -Depth 8 | Set-Content $cfg -Encoding utf8
    Write-Step "showFoodDebugInfo reset to false in $cfg"
}
