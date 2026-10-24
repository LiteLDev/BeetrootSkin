# Runs before the game starts: English game language and BeetrootSkin's default settings for the
# showcase shots. Any existing config is backed up and put back by showcase.restore.ps1.
Set-GameOption game_language en_US
$cfgDir = Join-Path $script:ModsDir "$script:ModName\config"
$cfg = Join-Path $cfgDir 'config.json'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
if ((Test-Path $cfg) -and -not (Test-Path "$cfg.showcase-e2e")) { Copy-Item $cfg "$cfg.showcase-e2e" -Force }
'{ "version": 1 }' | Set-Content -Encoding utf8NoBOM $cfg
Write-Step "showcase: default settings ($cfg)"
