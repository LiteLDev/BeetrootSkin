# Runs before the game starts: showVanillaAnimationsOverlay = true, opaque flashing overlays.
# Any existing config is backed up and put back by hud_jitter.restore.ps1.
$cfgDir = Join-Path $script:ModsDir "$script:ModName\config"
$cfg = Join-Path $cfgDir 'config.json'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
if ((Test-Path $cfg) -and -not (Test-Path "$cfg.jitter-e2e")) { Copy-Item $cfg "$cfg.jitter-e2e" -Force }
@'
{
  "version": 1,
  "showVanillaAnimationsOverlay": true,
  "maxHudOverlayFlashAlpha": 1.0
}
'@ | Set-Content -Encoding utf8NoBOM $cfg
Write-Step "hud_jitter: showVanillaAnimationsOverlay = true ($cfg)"
