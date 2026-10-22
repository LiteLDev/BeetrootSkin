# Runs before the game starts: deploy the test-only beetroot-api-sample mod (needs
# `xmake f --api_sample=y` + `xmake`) next to BeetrootSkin, and show food values in tooltips
# without Shift. Undone by api.restore.ps1.
Deploy-ExtraMod 'beetroot-api-sample'

$cfgDir = Join-Path $script:ModsDir "$script:ModName\config"
$cfg = Join-Path $cfgDir 'config.json'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
if ((Test-Path $cfg) -and -not (Test-Path "$cfg.api-e2e")) { Copy-Item $cfg "$cfg.api-e2e" -Force }
@'
{
  "version": 1,
  "showFoodValuesInTooltip": true,
  "showFoodValuesInTooltipAlways": true
}
'@ | Set-Content -Encoding utf8NoBOM $cfg
Write-Step "api: food values in tooltips always ($cfg)"
