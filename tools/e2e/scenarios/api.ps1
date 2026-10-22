# Public API (include/beetrootskin/api) used from another mod's DLL. api.prelaunch.ps1 deploys
# the test-only beetroot-api-sample mod, which
# - doubles the nutrition of minecraft:bread (FoodValuesEvent),
# - cancels HUDOverlayEvent::Exhaustion (no exhaustion underlay),
# - cancels TooltipOverlayEvent::Pre for minecraft:apple (no food values in its tooltip).
# Checks: bread tooltip shows default 5 vs modified 10 (AppleSkin's outlines), apple tooltip has
# no food rows, cooked beef is unaffected; the HUD hunger/saturation/health previews use the
# modified bread values, beef's stay vanilla, and the underlay is never drawn.
# Screenshots: out/api/*.png (+ *-zoom.png crops of the HUD bars, 3x).
$u = 'api'

Add-Type -AssemblyName System.Drawing
# Crop the hearts + hunger bar region of a 1280x720 screenshot and upscale it 3x for review.
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
function Get-Hunger {
    $line = Get-FoodStateLog
    if ($line -match 'hunger=(\d+)') { return [int]$Matches[1] }
    return 20
}
function Expect-New([string]$pattern, [int]$after, [int]$timeout = 15) {
    $line = Wait-NewLogLine $pattern $after $timeout
    Write-Step "  $line"
}

# The sample mod loaded after BeetrootSkin, called its exported function and subscribed.
Write-Step "  $(Wait-LogLine 'API: events registered' 10)"
Write-Step "  $(Wait-LogLine 'API sample: BeetrootSkin API version 1' 10)"
Write-Step "  $(Wait-LogLine 'API sample: 3 of 3 listeners subscribed' 10)"

Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
Select-Hotbar 4           # empty hand while the hotbar fills
Give-Item bread 4        # slot 1
Give-Item apple 4         # slot 2
Give-Item cooked_beef 4   # slot 3
Start-Sleep -Seconds 1

# --- Tooltips (survival inventory hotbar) ------------------------------------------------
$SlotX0 = 0.50; $SlotDX = 0.034; $SlotY = 0.72
Open-Inventory
$n = LogCount
Move-Mouse $SlotX0 $SlotY -Relative
Start-Sleep -Milliseconds 800
Save-Screenshot 'tooltip-bread' $u | Out-Null
# (Logged once per change, maybe already for a held bread: search the whole session.)
Write-Step "  $(Wait-LogLine 'API: FoodValuesEvent minecraft:bread: default hunger=5 saturation=6\.00 -> modified hunger=10 saturation=6\.00' 10)"
Write-Step "  $(Wait-LogLine 'API sample: doubled minecraft:bread nutrition 5 -> 10' 10)"

$n = LogCount
Move-Mouse ($SlotX0 + $SlotDX) $SlotY -Relative
Start-Sleep -Milliseconds 800
Save-Screenshot 'tooltip-apple' $u | Out-Null
Expect-New 'API: beetroot::api::TooltipOverlayEvent::Pre cancelled by a listener' $n

$n = LogCount
Move-Mouse ($SlotX0 + 2 * $SlotDX) $SlotY -Relative
Start-Sleep -Milliseconds 800
Save-Screenshot 'tooltip-cooked_beef' $u | Out-Null
Expect-New 'API: beetroot::api::TooltipOverlayEvent::Pre no longer cancelled' $n
if (Get-LeviLog | Select-String 'FoodValuesEvent minecraft:cooked_beef') { throw 'cooked beef values were reported as modified' }
Close-Screen

# --- HUD -----------------------------------------------------------------------------------
# The sample cancels every Exhaustion event: logged once, and the underlay never drawn.
Write-Step "  $(Wait-LogLine 'Exhaustion underlay: hidden \(cancelled by HUDOverlayEvent::Exhaustion listener\)' 10)"
Write-Step "  $(Wait-LogLine 'API: beetroot::api::HUDOverlayEvent::Exhaustion cancelled by a listener' 10)"
Write-Step "  $(Wait-LogLine 'API sample: cancelled HUDOverlayEvent::Exhaustion' 10)"

# Bring hunger to <= 11 with the Hunger effect, which also builds up exhaustion (the underlay
# would grow; it must stay hidden). The saturation overlay is unaffected.
Select-Hotbar 4
Invoke-Chat '/effect @s hunger 90 40 true'
Start-Sleep -Seconds 2
Shot 'hud-exhaustion-cancelled'
$deadline = (Get-Date).AddSeconds(80)
while ((Get-Hunger) -gt 11 -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 300 }
Clear-Effects
Start-Sleep -Seconds 1
$h = Get-Hunger
Write-Step "food state: $(Get-FoodStateLog)"
if ($h -gt 11 -or $h -lt 1) { throw "could not bring hunger into range (hunger=$h)" }
Hurt-Player 6
Start-Sleep -Seconds 1
Write-Step "food state: $(Get-FoodStateLog)"

# Bread: previews use the modified values (food=10, not 5).
$n = LogCount; Select-Hotbar 1
Expect-New "Hunger preview: hunger=$h food=10 icons=" $n
Expect-New 'Saturation preview: saturation=[\d.]+ gain=[1-9]' $n
Expect-New 'Health preview: health=\d+ -> \d+' $n
Shot 'hud-bread-0'; Start-Sleep -Milliseconds 300; Shot 'hud-bread-1'; Start-Sleep -Milliseconds 300; Shot 'hud-bread-2'

# Cooked beef: unaffected by the sample (food=8).
$n = LogCount; Select-Hotbar 3
Expect-New "Hunger preview: hunger=$h food=8 icons=" $n
Shot 'hud-beef-0'; Start-Sleep -Milliseconds 300; Shot 'hud-beef-1'

$drawn = @(Get-LeviLog | Select-String 'Exhaustion underlay: width=')
if ($drawn.Count -ne 0) { throw "exhaustion underlay drawn despite the cancelled event: $($drawn[0].Line)" }
$sat = @(Get-LeviLog | Select-String 'Saturation overlay: saturation=')
Write-Step "saturation overlay lines (unaffected by the sample): $($sat.Count)"
if ($sat.Count -eq 0) { throw 'saturation overlay was not drawn' }
Select-Hotbar 4
