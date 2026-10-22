# Runs before the game starts: back up the deployed config (put back by config_reload.restore.ps1)
# and start from the defaults, so the scenario knows every option's initial value.
$cfgDir = Join-Path $script:ModsDir "$script:ModName\config"
$cfg = Join-Path $cfgDir 'config.json'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
if ((Test-Path $cfg) -and -not (Test-Path "$cfg.config_reload-e2e")) { Copy-Item $cfg "$cfg.config_reload-e2e" -Force }
@'
{
  "version": 1,
  "showFoodValuesInTooltip": true,
  "showFoodValuesInTooltipAlways": true,
  "showSaturationHudOverlay": true,
  "showFoodValuesHudOverlay": true,
  "showFoodValuesHudOverlayWhenOffhand": true,
  "showFoodExhaustionHudUnderlay": true,
  "showFoodHealthHudOverlay": true,
  "showVanillaAnimationsOverlay": true,
  "maxHudOverlayFlashAlpha": 0.65,
  "showFoodDebugInfo": false
}
'@ | Set-Content -Encoding utf8NoBOM $cfg
Write-Step "config_reload: defaults written to $cfg"
