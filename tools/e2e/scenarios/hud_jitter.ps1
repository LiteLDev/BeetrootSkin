# Vanilla HUD jitter vs. BeetrootSkin overlays (HudContext::hungerJitterY / heartJitterY).
# Saturation 0 (every shank shakes now and then) and health 4 (low-health heart shake) with
# cooked beef held: the hunger preview, saturation gain preview and health preview are drawn
# over shaking icons. Many screenshots are taken in a burst; each is cropped around both bars
# (<name>-hud.png, 4x).
#   hud_jitter        showVanillaAnimationsOverlay = true:  overlays move with the vanilla icons
#   hud_jitter_still  showVanillaAnimationsOverlay = false: overlays stay on the resting line
# Both prelaunch scripts set maxHudOverlayFlashAlpha = 1 so the overlays are easy to see.
$u = $script:Unit

function Save-BarsCrop([string]$path) {
    $src  = [System.Drawing.Bitmap]::FromFile($path)
    $rect = New-Object System.Drawing.Rectangle 410, 605, 460, 50   # hearts + hunger bar
    $out  = New-Object System.Drawing.Bitmap ($rect.Width * 4), ($rect.Height * 4)
    $g    = [System.Drawing.Graphics]::FromImage($out)
    $g.InterpolationMode = 'NearestNeighbor'
    $g.PixelOffsetMode   = 'Half'
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $out.Width, $out.Height), $rect, 'Pixel')
    $crop = $path -replace '\.png$', '-hud.png'
    $out.Save($crop, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $out.Dispose(); $src.Dispose()
}

function Get-Food {
    $line = Get-FoodStateLog
    if ($line -match 'hunger=(\d+) saturation=([\d.\-]+)') {
        return [pscustomobject]@{ Hunger = [int]$Matches[1]; Saturation = [double]$Matches[2] }
    }
    throw "no food state in log"
}

function Drain-Until([scriptblock]$Until, [int]$Amp = 30, [int]$MaxSteps = 40) {
    for ($i = 0; $i -lt $MaxSteps; $i++) {
        if (& $Until (Get-Food)) { Clear-Effects; return }
        Invoke-Chat "/effect @s hunger 1 $Amp true"
        Start-Sleep -Milliseconds 300
    }
    Clear-Effects
    throw "drain did not reach the wanted state: $(Get-FoodStateLog)"
}

Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
Give-Item cooked_beef 4   # slot 1
Select-Hotbar 9
Start-Sleep -Seconds 1

# A still frame first (full bars) so the resting lines are known.
Save-BarsCrop (Save-Screenshot 'rest' $u)

Drain-Until { param($f) $f.Hunger -le 14 } -Amp 60
Drain-Until { param($f) $f.Saturation -le 0 } -Amp 30
$f = Get-Food
if ($f.Hunger -lt 8) { throw "hunger $($f.Hunger) overshot the 8..14 window" }
Hurt-Player 16            # 20 -> 4 HP
Start-Sleep -Seconds 1
Write-Step "food state: $(Get-FoodStateLog)"

$mark = @(Get-LeviLog).Count
Select-Hotbar 1
Wait-NewLogLine 'Hunger preview: hunger=' $mark 20 | Out-Null
Wait-NewLogLine 'Health preview: health=4 ' $mark 20 | Out-Null
for ($i = 0; $i -lt 16; $i++) {
    Save-BarsCrop (Save-Screenshot ('shake-{0:D2}' -f $i) $u)
    Start-Sleep -Milliseconds 120
}
Get-LeviLog | Select-String 'jitter seen' | ForEach-Object { Write-Step $_.Line }
Wait-LogLine 'Hunger jitter seen' 20 | Out-Null
Wait-LogLine 'Heart jitter seen' 20 | Out-Null
