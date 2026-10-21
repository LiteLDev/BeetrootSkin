# Hunger-restored HUD preview (AppleSkin HUDOverlayHandler HungerRestored).
# Holds several foods at ~6-10 hunger and screenshots the flashing shanks over a full flash
# cycle (32 ticks = 1.6 s), then checks the offhand, a non-food item, creative mode and a full
# hunger bar. The layer logs "Hunger preview: ..." on every change of what it shows.
# Screenshots: out/hunger_preview/*.png (+ *-zoom.png crops of the hunger bar, 3x).
$u = 'hunger_preview'

Add-Type -AssemblyName System.Drawing
# Crop the hunger bar region of a 1280x720 screenshot and upscale it 3x for review.
function Save-HungerZoom([string]$path) {
    $src = [System.Drawing.Bitmap]::FromFile($path)
    try {
        $rect = New-Object System.Drawing.Rectangle 400, 590, 480, 70
        $dst = New-Object System.Drawing.Bitmap ($rect.Width * 3), ($rect.Height * 3)
        $g = [System.Drawing.Graphics]::FromImage($dst)
        $g.InterpolationMode = 'NearestNeighbor'
        $g.PixelOffsetMode = 'Half'
        $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $dst.Width, $dst.Height), $rect, 'Pixel')
        $g.Dispose()
        $zoom = $path -replace '\.png$', '-zoom.png'
        $dst.Save($zoom, [System.Drawing.Imaging.ImageFormat]::Png)
        $dst.Dispose()
    } finally { $src.Dispose() }
}
function Shot([string]$name) { Save-HungerZoom (Save-Screenshot $name $u) }
# Several frames across one flash cycle.
function Shot-Cycle([string]$prefix, [int]$frames = 6, [int]$intervalMs = 270) {
    for ($i = 0; $i -lt $frames; $i++) { Shot "$prefix-$i"; Start-Sleep -Milliseconds $intervalMs }
}
function Get-Hunger {
    $line = Get-FoodStateLog
    if ($line -match 'hunger=(\d+)') { return [int]$Matches[1] }
    return 20
}
function LogCount { @(Get-LeviLog).Count }
function Expect-Preview([string]$pattern, [int]$after) {
    $line = Wait-NewLogLine "Hunger preview: $pattern" $after 15
    Write-Step "  $line"
}

Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
Give-Item cooked_beef 4    # slot 1
Give-Item golden_apple 4   # slot 2
Give-Item rotten_flesh 4   # slot 3
Give-Item stick 1          # slot 4 (not food)
Select-Hotbar 4

# Bring hunger to ~6-10.
# Drop-Hunger (amplifier 255) drains ~6 points/s once saturation is gone, too fast to stop in
# range; amplifier 40 adds ~4 exhaustion/s (~1 point/s), stopped once the log shows <= 11.
Invoke-Chat '/effect @s hunger 90 40 true'
$deadline = (Get-Date).AddSeconds(80)
while ((Get-Hunger) -gt 11 -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 300 }
Clear-Effects
Start-Sleep -Seconds 1
$h = Get-Hunger
Write-Step "food state: $(Get-FoodStateLog)"
if ($h -gt 10 -or $h -lt 1) { throw "could not bring hunger into range (hunger=$h)" }

# Cooked beef (8): shanks hunger/2 .. ceil((hunger+8)/2).
$n = LogCount; Select-Hotbar 1
Expect-Preview "hunger=$h food=8 icons=" $n
Shot-Cycle 'beef'

# Golden apple (4).
$n = LogCount; Select-Hotbar 2
Expect-Preview "hunger=$h food=4 icons=" $n
Shot-Cycle 'golden_apple'

# Rotten flesh (4): green "hunger effect" shanks.
$n = LogCount; Select-Hotbar 3
Expect-Preview "hunger=$h food=4 icons=.* rotten" $n
Shot-Cycle 'rotten_flesh'

# Non-food item: the overlay stops.
$n = LogCount; Select-Hotbar 4
Expect-Preview 'hidden' $n
Start-Sleep -Milliseconds 500
Shot 'stick-0'; Start-Sleep -Milliseconds 700; Shot 'stick-1'

# Bread in the offhand with an empty main hand.
Select-Hotbar 5
$n = LogCount
Invoke-Chat '/replaceitem entity @s slot.weapon.offhand 0 bread'
Expect-Preview "hunger=$h food=5 icons=.* offhand" $n
Shot-Cycle 'offhand_bread'
Invoke-Chat '/replaceitem entity @s slot.weapon.offhand 0 air'

# Creative: hidden (AppleSkin: !canHurtPlayer).
Select-Hotbar 1
Start-Sleep -Milliseconds 500
$n = LogCount
Set-Creative
Expect-Preview 'hidden' $n
Shot 'creative'
Set-Survival
Start-Sleep -Seconds 1

# Full hunger: beef cannot be eaten -> hidden; golden apple can always be eaten -> shown, but
# there is nothing to fill (range 10..10), so only the saturation preview would show.
Select-Hotbar 2
Start-Sleep -Milliseconds 500
$n = LogCount
Invoke-Chat '/effect @s saturation 1 10 true'
Start-Sleep -Seconds 2
Clear-Effects
Expect-Preview 'none' $n
Write-Step "food state: $(Get-FoodStateLog)"
Shot 'full_golden_apple'
$n = LogCount; Select-Hotbar 1
Expect-Preview 'hidden' $n
Shot 'full_beef'
