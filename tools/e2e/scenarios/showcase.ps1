# Showcase media for the README and the LeviMC Mods gallery (docs/showcase/).
# Stages a pleasant spot of the fixture world (grass bank by the river, looking at the mountains)
# and captures native 1600x900 frames (no downscaling, so the pixel art stays crisp):
#   tooltip-<food>.png      hovering each food in the survival inventory
#   burst-<food>\NNNN.bmp   ~3.6 s of frames while holding a food (the HUD previews flash);
#                           frames.csv lists each frame's capture time in ms
#   video-eat\NNNN.bmp      ~10 s: empty hand -> cooked beef (preview flashes) -> eat it
#   saturation.png          after eating: golden saturation outlines
#   debug.png               F6 debug line (and the exhaustion bar behind the shanks)
# Output: out/showcase/. tools/e2e/showcase-media.py turns it into docs/showcase/.
$u = 'showcase'
$out = Get-UnitOutDir $u
New-Item -ItemType Directory -Force $out | Out-Null
$VK_F6 = 0x75

function Grab([string]$name) {
    $bmp = Get-ClientBitmap
    try { $bmp.Save((Join-Path $out "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png) } finally { $bmp.Dispose() }
    Write-Step "grabbed $name"
}

# Capture frames for $Seconds; $Events = @{ ms = { action } } run between frames once their time has come.
function Record([string]$name, [double]$Seconds, [hashtable]$Events = @{}) {
    $dir = Join-Path $out $name
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    New-Item -ItemType Directory -Force $dir | Out-Null
    $pending = [System.Collections.Generic.List[int]]::new([int[]]@($Events.Keys | Sort-Object))
    $times = [System.Collections.Generic.List[string]]::new()
    $sw = [Diagnostics.Stopwatch]::StartNew(); $i = 0
    while ($sw.ElapsedMilliseconds -lt $Seconds * 1000) {
        while ($pending.Count -gt 0 -and $sw.ElapsedMilliseconds -ge $pending[0]) { & $Events[$pending[0]]; $pending.RemoveAt(0) }
        $t = $sw.ElapsedMilliseconds
        $bmp = Get-ClientBitmap
        try { $bmp.Save((Join-Path $dir ('{0:D4}.bmp' -f $i)), [System.Drawing.Imaging.ImageFormat]::Bmp) } finally { $bmp.Dispose() }
        $times.Add("$i,$t"); $i++
    }
    $times | Set-Content (Join-Path $dir 'frames.csv')
    Write-Step ("recorded {0}: {1} frames, {2:N1} fps" -f $name, $i, ($i * 1000.0 / $sw.ElapsedMilliseconds))
}

function Get-Hunger {
    $line = Get-FoodStateLog
    if ($line -match 'hunger=(\d+) ') { return [int]$Matches[1] }
    return 20
}

Wait-LogLine 'Food state: ' 30 | Out-Null
Invoke-Chat '/gamerule sendcommandfeedback false'
Invoke-Chat '/weather clear'
Invoke-Chat '/time set noon'
Reset-Player
# The grass bank west of the spawn (scouted), looking north along the river.
Invoke-Chat '/tp @s ~-8 ~3 ~'
Start-Sleep -Seconds 2
Invoke-Chat '/tp @s ~ ~ ~ 0 5'
$foods = @('cooked_beef', 'golden_apple', 'rotten_flesh', 'golden_carrot', 'bread', 'pumpkin_pie', 'cookie', 'cake')
Give-Item cooked_beef 16; Give-Item golden_apple 4; Give-Item rotten_flesh 4; Give-Item golden_carrot 8
Give-Item bread 4; Give-Item pumpkin_pie 4; Give-Item cookie 8; Give-Item cake 1
Select-Hotbar 9

# Tooltips in the survival inventory (hotbar row: slot 1 at 0.50 0.72, +0.034 per slot).
Open-Inventory
for ($i = 0; $i -lt $foods.Count; $i++) {
    Move-Mouse (0.50 + 0.034 * $i) 0.72 -Relative
    Start-Sleep -Milliseconds 700
    Grab "tooltip-$($foods[$i])"
}
Close-Screen

# Hunger 11, no saturation, 9 health: cooked beef then lifts hunger to 18+, so its preview also
# flashes the hearts natural regeneration would give back.
Invoke-Chat '/effect @s hunger 120 19 true'
$deadline = (Get-Date).AddSeconds(90)
while ((Get-Hunger) -gt 11 -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
Clear-Effects
Hurt-Player 11
Invoke-Chat '/tp @s ~ ~ ~ 0 5'
Write-Step "food state: $(Get-FoodStateLog)"
Start-Sleep -Seconds 6   # let the chat fade

# Each food for a little over two flash cycles (1.6 s each), once the item name above the
# hotbar has faded.
foreach ($hb in 1, 2, 3) {   # not $slot: run.ps1 has a typed $Slot
    Select-Hotbar $hb
    Start-Sleep -Milliseconds 3500
    Record "burst-$($foods[$hb - 1])" 3.6
}
Select-Hotbar 9; Start-Sleep -Seconds 1

# The eating clip: empty hand, then cooked beef (preview flashes), then eat it.
Record 'video-eat' 10.5 @{
    1000 = { Press-Key '1' }
    4300 = { [BsE2E.Native]::MouseButton(1, $false) }
    6300 = { [BsE2E.Native]::MouseButton(1, $true) }
}
Write-Step "food state: $(Get-FoodStateLog)"
Select-Hotbar 9
Start-Sleep -Seconds 1
Grab 'saturation'

# Debug line.
# (Eating has left some exhaustion. No commands here: even failed ones print to the chat.)
Assert-GameForeground
[BsE2E.Native]::Key([uint16]$VK_F6, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::Key([uint16]$VK_F6, $true)
Start-Sleep -Seconds 4   # the "debug info: on" message fades
Grab 'debug'
[BsE2E.Native]::Key([uint16]$VK_F6, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::Key([uint16]$VK_F6, $true)
Start-Sleep -Milliseconds 500
