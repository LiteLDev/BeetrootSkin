# Runs before the game starts: food values in tooltips only while Shift is held
# (showFoodValuesInTooltipAlways = false), so the scenario can compare both states.
# Any existing config is backed up and put back by tooltip.restore.ps1.
$cfgDir = Join-Path $script:ModsDir "$script:ModName\config"
$cfg = Join-Path $cfgDir 'config.json'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
if ((Test-Path $cfg) -and -not (Test-Path "$cfg.tooltip-e2e")) { Copy-Item $cfg "$cfg.tooltip-e2e" -Force }
@'
{
  "version": 1,
  "showFoodValuesInTooltip": true,
  "showFoodValuesInTooltipAlways": false
}
'@ | Set-Content -Encoding utf8NoBOM $cfg
Write-Step "tooltip: food values only while Shift is held ($cfg)"
