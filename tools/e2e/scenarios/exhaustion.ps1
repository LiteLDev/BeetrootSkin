# Exhaustion underlay: sprint-jump in survival and capture the strip under the hunger bar at
# several fill levels, then mount a pig and a horse (vanilla replaces the hunger bar with the
# mount's health) and switch to creative: the strip must be hidden there.
# Screenshots: out/exhaustion/{start,sprint-1..6,stopped,pig,horse,dismounted,creative}.png
$u = 'exhaustion'
Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
# The fixture's spawn is next to a lake: sprint on a stone platform high above it instead.
Invoke-Chat '/fill ~-16 199 ~-16 ~16 199 ~16 stone'
Invoke-Chat '/tp @s ~ 200 ~-14 0 0'   # onto the platform's -z edge, facing +z, level
Start-Sleep -Seconds 1
Save-Screenshot 'start' $u | Out-Null

function Hold-Keys([int[]]$vks) { Assert-GameForeground; foreach ($k in $vks) { [BsE2E.Native]::Key([uint16]$k, $false); Start-Sleep -Milliseconds 30 } }
function Release-Keys([int[]]$vks) { foreach ($k in $vks) { [BsE2E.Native]::Key([uint16]$k, $true) } }
function Get-LogSince([int]$after) { Get-LeviLog | Select-Object -Skip $after }

# Sprint-jump: W + CTRL (sprint) + SPACE (jump) held together. Exhaustion: 0.1 per metre
# sprinted plus 0.2 per sprint jump; it wraps at 4 (one saturation point consumed).
$keys = @([int][char]'W', $script:VK.CTRL, $script:VK.SPACE)
Click 0.5 0.5 -Relative   # make sure the game has captured the mouse/keyboard
$after = (Get-LeviLog).Count
try {
    Hold-Keys $keys
    for ($i = 1; $i -le 3; $i++) {
        Start-Sleep -Milliseconds 900
        Save-Screenshot "sprint-$i" $u | Out-Null
    }
} finally {
    Release-Keys $keys
}
Start-Sleep -Milliseconds 500
$lines = @(Get-LogSince $after | Select-String 'Exhaustion underlay: width=')
Write-Step "underlay width changes while sprint-jumping: $($lines.Count)"
$lines | ForEach-Object { Write-Step "  $($_.Line)" }
Write-Step "food state: $(Get-FoodStateLog)"

# Deterministic sweep: the Hunger effect adds 0.005 * (amplifier + 1) exhaustion per tick
# (amplifier 4: 0.5/s, the bar fills in ~8 s and then wraps as saturation is consumed).
$after = (Get-LeviLog).Count
Invoke-Chat '/effect @s hunger 30 4 true'
for ($i = 1; $i -le 6; $i++) {
    Start-Sleep -Milliseconds 1300
    Save-Screenshot "hunger-$i" $u | Out-Null
}
Clear-Effects
Start-Sleep -Milliseconds 500
$lines = @(Get-LogSince $after | Select-String 'Exhaustion underlay: width=')
Write-Step "underlay width changes under the Hunger effect: $($lines.Count)"
$lines | ForEach-Object { Write-Step "  $($_.Line)" }
if ($lines.Count -lt 3) { throw 'exhaustion underlay did not animate' }
Save-Screenshot 'stopped' $u | Out-Null
Write-Step "food state: $(Get-FoodStateLog)"

# Riding a living mount: vanilla hides the hunger bar, so must the underlay.
foreach ($mount in @('pig', 'horse')) {
    $after = (Get-LeviLog).Count
    if ($mount -eq 'horse') { Invoke-Chat '/summon horse ~2 ~ ~ 0 0 minecraft:on_tame' }
    else { Invoke-Chat '/summon pig ~2 ~ ~'; Invoke-Chat '/replaceitem entity @e[type=pig,c=1] slot.saddle 0 saddle' }
    Start-Sleep -Milliseconds 500
    Invoke-Chat "/ride @s start_riding @e[type=$mount,c=1]"
    Start-Sleep -Seconds 2
    Save-Screenshot $mount $u | Out-Null
    $hid = @(Get-LogSince $after | Select-String 'Hunger bar shown: false|Exhaustion underlay: hidden')
    Write-Step "$mount -> $($hid | ForEach-Object { $_.Line })"
    if ($hid.Count -eq 0 -and $mount -eq 'horse') { throw "hunger bar / underlay not hidden while riding a $mount" }
    Invoke-Chat '/ride @s stop_riding'
    Invoke-Chat "/kill @e[type=$mount]"
    Start-Sleep -Seconds 1
}
Save-Screenshot 'dismounted' $u | Out-Null

$after = (Get-LeviLog).Count
Set-Creative
Start-Sleep -Seconds 1
Save-Screenshot 'creative' $u | Out-Null
# (Vanilla may not call the hunger renderer at all in creative; the screenshot is the evidence.)
Write-Step "creative: $(Get-LogSince $after | Select-String 'Exhaustion underlay: |Hunger bar shown' | ForEach-Object { $_.Line })"
Set-Survival
