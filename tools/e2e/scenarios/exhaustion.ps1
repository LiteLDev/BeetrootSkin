# Exhaustion underlay: sprint-jump in survival and capture the strip under the hunger bar at
# several fill levels, then mount a pig and a horse (vanilla replaces the hunger bar with the
# mount's health) and switch to creative: the strip must be hidden there.
# Screenshots: out/exhaustion/{start,sprint-1..6,stopped,pig,horse,dismounted,creative}.png
$u = 'exhaustion'
Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
Start-Sleep -Seconds 1
Save-Screenshot 'start' $u | Out-Null

function Hold-Keys([int[]]$vks) { Assert-GameForeground; foreach ($k in $vks) { [BsE2E.Native]::Key([uint16]$k, $false); Start-Sleep -Milliseconds 30 } }
function Release-Keys([int[]]$vks) { foreach ($k in $vks) { [BsE2E.Native]::Key([uint16]$k, $true) } }
function Get-LogSince([int]$after) { Get-LeviLog | Select-Object -Skip $after }

# Sprint-jump: W + CTRL (sprint) + SPACE (jump) held together. Exhaustion: 0.1 per metre
# sprinted plus 0.2 per sprint jump; it wraps at 4 (one saturation point consumed).
$keys = @([int][char]'W', $script:VK.CTRL, $script:VK.SPACE)
$after = (Get-LeviLog).Count
try {
    Hold-Keys $keys
    for ($i = 1; $i -le 6; $i++) {
        Start-Sleep -Milliseconds 900
        Save-Screenshot "sprint-$i" $u | Out-Null
    }
} finally {
    Release-Keys $keys
}
Start-Sleep -Milliseconds 500
$lines = @(Get-LogSince $after | Select-String 'Exhaustion underlay: width=')
Write-Step "underlay width changes while sprinting: $($lines.Count)"
$lines | Select-Object -First 40 | ForEach-Object { Write-Step "  $($_.Line)" }
if ($lines.Count -lt 3) { throw 'exhaustion underlay did not animate while sprint-jumping' }
Save-Screenshot 'stopped' $u | Out-Null
Write-Step "food state: $(Get-FoodStateLog)"

# Riding a living mount: vanilla hides the hunger bar, so must the underlay.
foreach ($mount in @('pig', 'horse')) {
    $after = (Get-LeviLog).Count
    Invoke-Chat "/summon $mount ~2 ~ ~"
    Start-Sleep -Milliseconds 500
    Invoke-Chat "/ride @s start_riding @e[type=$mount,c=1]"
    Start-Sleep -Seconds 2
    Save-Screenshot $mount $u | Out-Null
    $hid = @(Get-LogSince $after | Select-String 'Hunger bar shown: false|Exhaustion underlay: hidden')
    Write-Step "$mount -> $($hid | ForEach-Object { $_.Line })"
    if ($hid.Count -eq 0) { throw "hunger bar / underlay not hidden while riding a $mount" }
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
