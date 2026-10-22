# Live config reload: config.json is rewritten while the game runs and every change must apply
# within ~1 s (log "Config reloaded: ..."), without restarting. Then invalid JSON (one warning,
# file untouched, settings kept), and F6 (the mod's own save must not trigger a reload).
# Starts from the defaults (config_reload.prelaunch.ps1); the original config is put back by
# config_reload.restore.ps1.
# Screenshots: out/config_reload/
#   hud-defaults         holding cooked beef: saturation outline, exhaustion strip, flashing previews
#   hud-sat-exh-off      showSaturationHudOverlay + showFoodExhaustionHudUnderlay off
#   hud-alpha-0 / -1     maxHudOverlayFlashAlpha 0 (no flash) / 1 (opaque flash)
#   tooltip-always-off   inventory, beef hovered without Shift, showFoodValuesInTooltipAlways off (edited while open)
#   tooltip-always-on    ... switched back on while the inventory stays open
#   debug-on             showFoodDebugInfo turned on from the file
#   invalid              broken JSON: HUD unchanged (settings kept)
#   restored             valid file again
#   f6-off               F6 hid the debug line and saved; no reload follows
# (*-zoom.png: the HUD bars cropped and upscaled 3x.)
$u = 'config_reload'
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"
$bak = "$cfg.bak"
$bakBefore = if (Test-Path $bak) { (Get-Item $bak).LastWriteTimeUtc } else { $null }

Add-Type -AssemblyName System.Drawing
function Save-HudZoom([string]$path) {
    $src = [System.Drawing.Bitmap]::FromFile($path)
    try {
        $rect = New-Object System.Drawing.Rectangle 400, 590, 480, 70
        $dst = New-Object System.Drawing.Bitmap ($rect.Width * 3), ($rect.Height * 3)
        $g = [System.Drawing.Graphics]::FromImage($dst)
        $g.InterpolationMode = 'NearestNeighbor'
        $g.PixelOffsetMode = 'Half'
        $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $dst.Width, $dst.Height), $rect, 'Pixel')
        $g.Dispose()
        $dst.Save(($path -replace '\.png$', '-zoom.png'), [System.Drawing.Imaging.ImageFormat]::Png)
        $dst.Dispose()
    } finally { $src.Dispose() }
}
function Shot([string]$name) { Save-HudZoom (Save-Screenshot $name $u) }
function LogCount { @(Get-LeviLog).Count }
function Read-Config { Get-Content $cfg -Raw | ConvertFrom-Json }

# Rewrite config.json with some options changed (like a user saving it in an editor), then wait
# for the reload line naming them. Returns the seconds it took.
function Set-ConfigOptions([hashtable]$changes, [string]$expect) {
    $json = Read-Config
    foreach ($k in $changes.Keys) { $json.$k = $changes[$k] }
    $after = LogCount
    $t0 = Get-Date
    $json | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8NoBOM $cfg
    $deadline = $t0.AddSeconds(5)
    while ((Get-Date) -lt $deadline) {
        $hit = @(Get-LeviLog | Select-Object -Skip $after | Select-String 'Config reloaded: ' | Select-Object -Last 1)
        if ($hit) { break }
        Start-Sleep -Milliseconds 100
    }
    if (-not $hit) { throw "no reload within 5 s after setting $($changes.Keys -join ', ')" }
    $secs = ((Get-Date) - $t0).TotalSeconds
    Write-Step ("reload after {0:N2} s: {1}" -f $secs, $hit[0].Line)
    if ($hit[0].Line -notmatch $expect) { throw "reload line does not match '$expect'" }
    if ($secs -gt 2.5) { throw "reload took $secs s (expected about 1 s)" }
    Start-Sleep -Milliseconds 400 # let a few frames render with the new settings
}

Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
Give-Item cooked_beef 4
Select-Hotbar 1
# Some hunger and saturation missing (previews visible), a little exhaustion, some damage.
Drop-Hunger 2
Feed-Player 2
Invoke-Chat '/effect @s hunger 2 3 true'
Start-Sleep -Seconds 2
Clear-Effects
Hurt-Player 6
# Visible exhaustion strip: Hunger IV adds 0.025/tick, ~1.5 in 3 s (below the wrap at 4).
Invoke-Chat '/effect @s hunger 3 4 true'
Start-Sleep -Seconds 4
Clear-Effects
Start-Sleep -Seconds 1
Write-Step "food state: $(Get-FoodStateLog)"
Shot 'hud-defaults'

# --- HUD overlays off ------------------------------------------------------------------------
$after = LogCount
Set-ConfigOptions @{ showSaturationHudOverlay = $false; showFoodExhaustionHudUnderlay = $false } 'showSaturationHudOverlay=false, showFoodExhaustionHudUnderlay=false'
Wait-NewLogLine 'Exhaustion underlay: hidden \(enabled=false' $after 5 | Out-Null
Shot 'hud-sat-exh-off'

# --- Flash alpha ----------------------------------------------------------------------------
Set-ConfigOptions @{ maxHudOverlayFlashAlpha = 0 } 'maxHudOverlayFlashAlpha=0'
Shot 'hud-alpha-0'
Set-ConfigOptions @{ maxHudOverlayFlashAlpha = 1.0; showSaturationHudOverlay = $true; showFoodExhaustionHudUnderlay = $true } 'showSaturationHudOverlay=true, showFoodExhaustionHudUnderlay=true, maxHudOverlayFlashAlpha=1'
Shot 'hud-alpha-1'

# --- Tooltip, edited while the inventory is open --------------------------------------------
Open-Inventory
Move-Mouse 0.50 0.72 -Relative
Start-Sleep -Milliseconds 700
Set-ConfigOptions @{ showFoodValuesInTooltipAlways = $false } 'showFoodValuesInTooltipAlways=false'
Move-Mouse 0.502 0.72 -Relative
Start-Sleep -Milliseconds 500
Save-Screenshot 'tooltip-always-off' $u | Out-Null
Set-ConfigOptions @{ showFoodValuesInTooltipAlways = $true } 'showFoodValuesInTooltipAlways=true'
Move-Mouse 0.50 0.72 -Relative
Start-Sleep -Milliseconds 500
Save-Screenshot 'tooltip-always-on' $u | Out-Null
Close-Screen

# --- Debug line from the file ----------------------------------------------------------------
Set-ConfigOptions @{ showFoodDebugInfo = $true } 'showFoodDebugInfo=true'
Save-Screenshot 'debug-on' $u | Out-Null

# --- Invalid JSON: one warning, settings kept, file untouched -----------------------------------
$valid = Get-Content $cfg -Raw
$broken = $valid -replace '"showSaturationHudOverlay": true', '"showSaturationHudOverlay": fals'
if ($broken -eq $valid) { throw 'could not break the config text' }
$after = LogCount
$broken | Set-Content -Encoding utf8NoBOM -NoNewline $cfg
Wait-NewLogLine 'is invalid, keeping the current settings' $after 5 | Out-Null
Start-Sleep -Seconds 4 # several more polls
$warns = @(Get-LeviLog | Select-Object -Skip $after | Select-String 'is invalid, keeping the current settings')
$warns | ForEach-Object { Write-Step "  $($_.Line)" }
if ($warns.Count -ne 1) { throw "expected exactly one invalid-config warning, got $($warns.Count)" }
if (@(Get-LeviLog | Select-Object -Skip $after | Select-String 'Config reloaded: ').Count -ne 0) { throw 'an invalid file was applied' }
if ((Get-Content $cfg -Raw) -ne $broken) { throw 'the broken config file was modified by the mod' }
if (-not (Get-GameProcess)) { throw 'game exited after invalid config' }
Shot 'invalid'

# Fix it: the saturation option comes back as false (the user's intended edit).
$fixed = $valid -replace '"showSaturationHudOverlay": true', '"showSaturationHudOverlay": false'
$after = LogCount
$fixed | Set-Content -Encoding utf8NoBOM -NoNewline $cfg
$line = Wait-NewLogLine 'Config reloaded: ' $after 5
Write-Step "  $line"
if ($line -notmatch 'showSaturationHudOverlay=false') { throw 'fixed file not applied' }
Set-ConfigOptions @{ showSaturationHudOverlay = $true } 'showSaturationHudOverlay=true'
Shot 'restored'

# --- F6: the mod's own save must not be reloaded -----------------------------------------------
Start-Sleep -Seconds 2 # nothing pending
$after = LogCount
Focus-Game
Assert-GameForeground
[BsE2E.Native]::Key([uint16]0x75, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::Key([uint16]0x75, $true)
Wait-NewLogLine 'Food debug info hidden' $after 10 | Out-Null
Start-Sleep -Seconds 4 # several polls
Shot 'f6-off'
$noise = @(Get-LeviLog | Select-Object -Skip $after | Select-String 'Config reloaded|changed on disk|is invalid|was removed|overwriting it')
$noise | ForEach-Object { Write-Step "  unexpected: $($_.Line)" }
if ($noise.Count -ne 0) { throw 'F6 save triggered a reload or warning' }
if ((Read-Config).showFoodDebugInfo -ne $false) { throw 'F6 did not save showFoodDebugInfo=false' }
Write-Step 'F6 saved without a reload'

# --- An edit and F6 at the same moment: neither change is lost ------------------------------
# F6 saves before the poll picked the edit up (merged, "Config reloaded before saving"), or
# right after it (plain reload); either way the file must end up with both changes.
$json = Read-Config
$json.showFoodHealthHudOverlay = $false
$after = LogCount
$json | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8NoBOM $cfg
[BsE2E.Native]::Key([uint16]0x75, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::Key([uint16]0x75, $true)
Wait-NewLogLine 'Food debug info shown' $after 10 | Out-Null
Start-Sleep -Seconds 3
Get-LeviLog | Select-Object -Skip $after | Select-String 'Config |Food debug info' | ForEach-Object { Write-Step "  $($_.Line)" }
$j = Read-Config
if ($j.showFoodHealthHudOverlay -ne $false -or $j.showFoodDebugInfo -ne $true) {
    throw "edit + F6 lost a change: showFoodHealthHudOverlay=$($j.showFoodHealthHudOverlay) showFoodDebugInfo=$($j.showFoodDebugInfo)"
}
Write-Step 'edit + F6: both changes kept'
Save-Screenshot 'edit-and-f6' $u | Out-Null

# The mod never needed a backup (no unseen edit was overwritten).
$bakNow = if (Test-Path $bak) { (Get-Item $bak).LastWriteTimeUtc } else { $null }
if ($bakNow -ne $bakBefore) { throw "unexpected $bak written" }

$reloads = @(Get-LeviLog | Select-String 'Config reloaded: ')
Write-Step "reloads this session: $($reloads.Count)"
