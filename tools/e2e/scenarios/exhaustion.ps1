# Exhaustion underlay: sprint-jump in survival, then sweep exhaustion with the Hunger effect, and
# capture the strip under the hunger bar at several fill levels. Then ride a horse: unlike Java,
# Bedrock keeps the hunger bar while riding (the mount's hearts go in rows above it), so the
# underlay must follow vanilla and stay. Finally creative: no hunger bar, no underlay.
# Screenshots: out/exhaustion/{start,sprint-1..3,hunger-1..6,stopped,horse,dismounted,creative}.png
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

# Riding a living mount: the underlay follows vanilla's hunger bar (HudHungerRenderer's
# mShouldRender). Bedrock keeps the bar while riding, so neither may be reported hidden.
$after = (Get-LeviLog).Count
Invoke-Chat '/summon horse ~2 ~ ~ 0 0 minecraft:on_tame'
Start-Sleep -Milliseconds 500
Invoke-Chat '/ride @s start_riding @e[type=horse,c=1]'
Start-Sleep -Seconds 2
Save-Screenshot 'horse' $u | Out-Null
$hid = @(Get-LogSince $after | Select-String 'Hunger bar shown: false|Exhaustion underlay: hidden')
Write-Step "horse -> hidden lines: $($hid.Count) $($hid | ForEach-Object { $_.Line })"
if ($hid.Count -ne 0) { throw 'underlay hidden while vanilla still shows the hunger bar (riding)' }
Invoke-Chat '/ride @s stop_riding'
Invoke-Chat '/kill @e[type=horse]'
Start-Sleep -Seconds 1
Save-Screenshot 'dismounted' $u | Out-Null

$after = (Get-LeviLog).Count
Set-Creative
Start-Sleep -Seconds 1
Save-Screenshot 'creative' $u | Out-Null
# (Vanilla may not call the hunger renderer at all in creative; the screenshot is the evidence.)
Write-Step "creative: $(Get-LogSince $after | Select-String 'Exhaustion underlay: |Hunger bar shown' | ForEach-Object { $_.Line })"
Set-Survival
