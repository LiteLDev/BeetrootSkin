# Runs before the game starts (see run.ps1): enable the scaffold's debug drawing (src/debug/),
# which outlines the HUD icon rects, draws icons.png over the hunger bar and reserves a debug
# area in item tooltips. Removed again by smoke.restore.ps1 (and by every Deploy-Mod).
$marker = Join-Path $script:ModsDir "$script:ModName\debug_draw"
New-Item -ItemType File -Force $marker | Out-Null
Write-Step "debug drawing enabled ($marker)"
