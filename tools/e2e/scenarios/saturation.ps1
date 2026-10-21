# Saturation HUD overlay (golden outlines over the shanks) and the flashing saturation-gain
# preview of the held food.
# Screenshots: out/saturation/<name>.png plus <name>-hud.png (the hunger bar, cropped and 4x).
#
# On Bedrock the Saturation effect only restores hunger, so saturation is raised by eating:
# golden carrot (6 hunger, 14.4 saturation), golden apple (4, 9.6), cooked beef (8, 12.8).
$u = 'saturation'

# Crop the hunger bar out of a saved screenshot (1280x720) and enlarge it 4x (nearest neighbour)
# so the outlines can be compared with the shanks pixel by pixel.
function Save-HudCrop([string]$path) {
    $src  = [System.Drawing.Bitmap]::FromFile($path)
    $rect = New-Object System.Drawing.Rectangle 660, 615, 280, 40   # hunger bar
    $out  = New-Object System.Drawing.Bitmap ($rect.Width * 4), ($rect.Height * 4)
    $g    = [System.Drawing.Graphics]::FromImage($out)
    $g.InterpolationMode = 'NearestNeighbor'
    $g.PixelOffsetMode   = 'Half'
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $out.Width, $out.Height), $rect, 'Pixel')
    $crop = $path -replace '\.png$', '-hud.png'
    $out.Save($crop, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $out.Dispose(); $src.Dispose()
}

function Shot([string]$name) {
    Write-Step "$name -> $(Get-FoodStateLog)"
    Save-HudCrop (Save-Screenshot $name $u)
}

function Get-Food {
    $line = Get-FoodStateLog
    if ($line -match 'hunger=(\d+) saturation=([\d.\-]+)') {
        return [pscustomobject]@{ Hunger = [int]$Matches[1]; Saturation = [double]$Matches[2] }
    }
    throw "no food state in log"
}

# Apply short Hunger effects (amplifier $Amp) until $Until (scriptblock on the food state) holds.
function Drain-Until([scriptblock]$Until, [int]$Amp = 30, [int]$MaxSteps = 40) {
    for ($i = 0; $i -lt $MaxSteps; $i++) {
        if (& $Until (Get-Food)) { Clear-Effects; return }
        Invoke-Chat "/effect @s hunger 1 $Amp true"
        Start-Sleep -Milliseconds 300
    }
    Clear-Effects
    throw "drain did not reach the wanted state: $(Get-FoodStateLog)"
}

# Hold right click long enough to eat the held food (1.6 s on Bedrock).
function Eat-Held {
    Assert-GameForeground
    [BsE2E.Native]::MouseButton(1, $false)
    Start-Sleep -Milliseconds 2600
    [BsE2E.Native]::MouseButton(1, $true)
    Start-Sleep -Milliseconds 800
}

# Hold slot $Slot, wait for its preview log line and take two screenshots half a flash apart.
function Shot-Preview([int]$Slot, [string]$name) {
    $mark = @(Get-LeviLog).Count
    Select-Hotbar $Slot
    $line = Wait-NewLogLine 'Saturation preview: saturation=' $mark 20
    Write-Step "preview: $line"
    Start-Sleep -Milliseconds 300
    Shot "$name-a"
    Start-Sleep -Milliseconds 800
    Shot "$name-b"
}

function Eat-Slot([int]$Slot, [string]$name) {
    Select-Hotbar $Slot
    Eat-Held
    Select-Hotbar 9            # empty hand: base overlay only
    Start-Sleep -Seconds 1
    Shot $name
    Write-Step "overlay: $(Get-LeviLog | Select-String 'Saturation overlay: ' | Select-Object -Last 1)"
}

Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
Give-Item golden_apple 4   # slot 1
Give-Item golden_carrot 4  # slot 2
Give-Item cooked_beef 4    # slot 3
Select-Hotbar 9
Start-Sleep -Seconds 1

# 1. Whatever the world starts with (5 saturation: two full outlines and a half one).
Shot 'initial'

# 2. Hunger 9..14 and no saturation, then golden carrot: preview of 14.4 saturation, and after
#    eating 7.2 icons (quarter sprite at the tail).
Drain-Until { param($f) $f.Hunger -le 14 } -Amp 60
Drain-Until { param($f) $f.Saturation -le 0 } -Amp 30
$f = Get-Food
if ($f.Hunger -lt 9) { throw "hunger $($f.Hunger) overshot the 9..14 window" }
Shot 'drained'
Shot-Preview 2 'preview-carrot'
Eat-Slot 2 'carrot'
Wait-LogLine 'Saturation overlay: saturation=14\.40 gain=0\.00 icons=8' 10 | Out-Null

# 3. Golden apple on top (always edible): preview from the tail to the cap, then a full bar.
Shot-Preview 1 'preview-apple-full'
Eat-Slot 1 'full'
# (20, or 19 when the carrot only brought hunger to 15: saturation is capped by hunger.)
Wait-LogLine 'Saturation overlay: saturation=(19|20)\.00 gain=0\.00 icons=10' 10 | Out-Null

# 4. Drain to an odd value: half outline at the tail.
Drain-Until { param($f) $f.Saturation -le 11 -and ([int]$f.Saturation % 2) -eq 1 } -Amp 20
Start-Sleep -Milliseconds 500
Shot 'half'

# 5. No saturation, then golden apple: 9.6 -> 4.8 icons (three-quarter sprite at the tail).
Drain-Until { param($f) $f.Saturation -le 0 } -Amp 30
Start-Sleep -Milliseconds 500
Shot 'empty'
Eat-Slot 1 'three-quarter'
Wait-LogLine 'Saturation overlay: saturation=9\.60 gain=0\.00 icons=5' 10 | Out-Null

# 6. Low hunger, cooked beef: flashing preview (gain capped at the new hunger level when needed).
Drop-Hunger 3
Start-Sleep -Seconds 1
Shot-Preview 3 'preview-beef'
Start-Sleep -Milliseconds 400
Shot 'preview-beef-c'
Eat-Slot 3 'beef'
