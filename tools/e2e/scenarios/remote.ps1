# Real remote server: the official Bedrock Dedicated Server (remote.prelaunch.ps1 starts it with a
# copy of the fixture world; run.ps1 joins it through Play > Servers > Add Server). Everything
# BeetrootSkin shows must come from the attributes, game rules and difficulty the server syncs to
# the client (source=client, exact=false), and match what the server's commands did. All
# commands go through the BDS console (Invoke-BdsCommand), not the client's chat.
# Screenshots: out/remote/*.png; server console: out/remote/bds.log.
$u = 'remote'

function ConvertFrom-FoodState([string]$line) {
    if ($line -notmatch 'hunger=(\d+) saturation=([\d.]+) exhaustion=([\d.]+) health=([\d.]+)/([\d.]+) peaceful=(\w+) naturalRegen=(\w+) creative=\w+ source=([\w-]+)') {
        throw "Not a food state line: $line"
    }
    @{ hunger = [int]$Matches[1]; saturation = [double]$Matches[2]; exhaustion = [double]$Matches[3]; health = [double]$Matches[4]
       maxHealth = [double]$Matches[5]; peaceful = $Matches[6]; naturalRegen = $Matches[7]; source = $Matches[8] }
}
function Get-State { ConvertFrom-FoodState (Get-FoodStateLog) }
function Assert-That([bool]$cond, [string]$what) { if (-not $cond) { throw "Assertion failed: $what" }; Write-Step "ok: $what" }
function Bds([string]$cmd, [int]$waitMs = 700) { Invoke-BdsCommand $cmd $waitMs | Out-Null }
function Shoot([string]$name, [int]$frames = 1) {
    # Previews pulse (~1.6 s period): several frames so at least one is bright.
    for ($i = 1; $i -le $frames; ++$i) {
        $n = if ($frames -gt 1) { "$name-$i" } else { $name }
        Save-Screenshot $n $u | Out-Null
        if ($i -lt $frames) { Start-Sleep -Milliseconds 330 }
    }
}
# Right mouse held long enough to eat one item (1.6 s on Bedrock).
function Eat-Held { Focus-Game; [BsE2E.Native]::MouseButton(1, $false); Start-Sleep -Milliseconds 2200; [BsE2E.Native]::MouseButton(1, $true); Start-Sleep -Seconds 2 }
# Burn hunger slowly (Hunger effect amplifier 19: 0.1 exhaustion per tick) down to $target.
function Lower-Hunger([int]$target, [int]$TimeoutSeconds = 120) {
    if ((Get-State).hunger -le $target) { return }
    Bds 'effect {player} hunger 200 19 true'
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-State).hunger -gt $target -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    Bds 'effect {player} hunger 0'   # removes only the Hunger effect (keeps Health Boost)
    Start-Sleep -Seconds 1
}

# --- Joined: the client attribute path, fed by the server --------------------------------------
$s = Get-State
Assert-That ($s.source -eq 'client') "on a dedicated server the food state comes from the client attributes (exact=false)"
Assert-That (-not (Get-LeviLog | Select-String 'source=integrated-server')) "no integrated-server food state in this session"
Wait-LogLine 'Sync: food state source none -> client$' 10 | Out-Null
# The first packets arrive before ClientJoinLevelEvent; the sync session must keep them.
Assert-That (@(Get-LeviLog | Select-String 'Sync: food state source ').Count -eq 1) "one food state source change (none -> client) for the whole join"
foreach ($a in 'player.hunger', 'player.saturation', 'player.exhaustion', 'health') {
    Wait-LogLine "Sync: server syncs minecraft:$([regex]::Escape($a)) to the client" 20 | Out-Null
}
Write-Step "server syncs hunger, saturation, exhaustion and health"
Save-Screenshot 'joined' $u | Out-Null

# --- Setup: a stone platform in the air (the fixture spawn has water), known state ----------------
Bds 'gamerule naturalregeneration true'
Bds 'gamerule showcoordinates false'
Bds 'difficulty normal'
Bds 'gamemode survival {player}'
Bds 'clear {player}'
Bds 'effect {player} clear'
Bds 'execute as {player} at @s run fill ~-20 119 ~-20 ~20 119 ~20 stone'
Bds 'execute as {player} at @s run tp @s ~ 120 ~ 0 0'
Bds 'effect {player} instant_health 1 10 true'
Bds 'effect {player} saturation 1 10 true'
Start-Sleep -Seconds 2
Bds 'effect {player} clear'
Start-Sleep -Seconds 1
$s = Get-State
Assert-That ($s.hunger -eq 20 -and $s.health -eq 20 -and $s.maxHealth -eq 20) "server reset: hunger 20, health 20/20 ($(Get-FoodStateLog))"
Assert-That ($s.peaceful -eq 'false' -and $s.naturalRegen -eq 'true') "client level: normal difficulty, natural regeneration on"

# --- Hunger drain on the server shows on the client ---------------------------------------------
Bds 'effect {player} hunger 2 255 true'
Start-Sleep -Seconds 3
Bds 'effect {player} clear'
Start-Sleep -Seconds 2
$before = Get-State
Assert-That ($before.hunger -le 16) "server-side Hunger effect drained hunger ($($before.hunger), saturation $($before.saturation))"

# --- Eat cooked beef: +8 hunger, +12.8 saturation (capped at the new hunger) ---------------------
Bds 'give {player} cooked_beef 8'
Bds 'give {player} golden_apple 4'
Bds 'give {player} bread 4'
Select-Hotbar 1
Start-Sleep -Seconds 1
$hp = Get-LeviLog | Select-String 'Hunger preview: ' | Select-Object -Last 1
Assert-That ($null -ne $hp) "hunger preview while holding beef: $($hp.Line)"
Shoot 'hold-beef' 4
Eat-Held
$after = Get-State
Assert-That ($after.hunger -eq [Math]::Min(20, $before.hunger + 8)) "eating cooked beef adds 8 hunger ($($before.hunger) -> $($after.hunger))"
$expectedSat = [Math]::Min([double]$after.hunger, $before.saturation + 12.8)
Assert-That ([Math]::Abs($after.saturation - $expectedSat) -lt 0.05) "saturation after eating is $expectedSat ($($after.saturation))"
Wait-LogLine 'Saturation overlay: saturation=12\.80 ' 10 | Out-Null
Write-Step "saturation overlay shows the synced 12.8 saturation"
Shoot 'eaten'

# --- Sprint-jump: exhaustion grows (underlay follows, from the client attributes) ----------------
$logAt = @(Get-LeviLog).Count
$ex0 = (Get-State).exhaustion
Select-Hotbar 9
Focus-Game
Click 0.5 0.5 -Relative
$keys = @([int][char]'W', $script:VK.CTRL, $script:VK.SPACE)
try {
    foreach ($k in $keys) { [BsE2E.Native]::Key([uint16]$k, $false); Start-Sleep -Milliseconds 30 }
    Start-Sleep -Milliseconds 1200
    Shoot 'sprint'
    Start-Sleep -Milliseconds 400
} finally { foreach ($k in $keys) { [BsE2E.Native]::Key([uint16]$k, $true) } }
Start-Sleep -Seconds 1
$under = @(Get-LeviLog | Select-Object -Skip $logAt | Select-String 'Exhaustion underlay: width=\d+ exhaustion=[\d.]+ source=client')
$s = Get-State
# (Food state lines are logged on hunger/saturation changes only; the underlay logs exhaustion.)
$ex1 = if ($under) { $under[-1].Line -replace '.*exhaustion=([\d.]+).*', '$1' } else { $ex0 }
Assert-That ($under.Count -ge 1 -or $s.saturation -lt $after.saturation) "sprint-jumping moved the exhaustion underlay ($($under.Count) changes, exhaustion $ex0 -> $ex1, saturation $($s.saturation))"
Shoot 'sprinted'
Bds 'execute as {player} at @s run tp @s ~ 120 ~ 0 0'

# --- Damage, then the health preview with natural regeneration on and off -------------------------
Lower-Hunger 12
$logAt = @(Get-LeviLog).Count
Bds 'damage {player} 8'
Start-Sleep -Seconds 1
$s = Get-State
Assert-That ($s.health -le 12.5 -and $s.health -ge 11) "server damage shows on the client (health $($s.health))"
Assert-That ($s.hunger -ge 10 -and $s.hunger -le 12) "hunger 10..12 for the previews ($($s.hunger))"
Select-Hotbar 1
$line = Wait-NewLogLine 'Health preview: health=1[12] -> ' $logAt 15
Write-Step "beef, natural regen on: $line"
Shoot 'health-preview' 4

$logAt = @(Get-LeviLog).Count
Bds 'gamerule naturalregeneration false'
Wait-NewLogLine 'Sync: server sent game rules: naturalRegeneration=false|Sync: server sent game rules: .*naturalregeneration=false' $logAt 10 | Out-Null
Wait-NewLogLine 'Food state: .*naturalRegen=false .*source=client' $logAt 10 | Out-Null
Wait-NewLogLine 'Health preview: none' $logAt 10 | Out-Null
Write-Step "naturalregeneration false: health preview hidden"
Shoot 'no-natural-regen' 2
# Golden apple: Regeneration II still heals without natural regeneration.
$logAt = @(Get-LeviLog).Count
Select-Hotbar 2
$line = Wait-NewLogLine 'Health preview: health=1[12] -> ' $logAt 10
Write-Step "golden apple, natural regen off: $line"
Shoot 'golden-apple' 4
Bds 'gamerule naturalregeneration true'
Wait-NewLogLine 'Food state: .*naturalRegen=true .*source=client' $logAt 10 | Out-Null

# --- Saturation overlay -----------------------------------------------------------------------
Assert-That (@(Get-LeviLog | Select-String 'Saturation preview: saturation=0\.00 gain=12\.80 ').Count -gt 0) "saturation preview of cooked beef at saturation 0: gain 12.8"

# --- Peaceful: no health preview, client level follows SetDifficultyPacket -----------------------
$logAt = @(Get-LeviLog).Count
Select-Hotbar 1
Bds 'difficulty peaceful'
Wait-NewLogLine 'Sync: server sent difficulty 0' $logAt 10 | Out-Null
Wait-NewLogLine 'Food state: .*peaceful=true .*source=client' $logAt 10 | Out-Null
Wait-NewLogLine 'Health preview: none' $logAt 10 | Out-Null
Write-Step "peaceful: health preview hidden"
Shoot 'peaceful' 2
Bds 'difficulty normal'
Wait-NewLogLine 'Food state: .*peaceful=false .*source=client' $logAt 15 | Out-Null

# --- Health Boost: two rows of hearts ------------------------------------------------------------
Select-Hotbar 9
Bds 'effect {player} health_boost 600 4 true'
Bds 'effect {player} instant_health 1 5 true'
Start-Sleep -Seconds 2
Bds 'damage {player} 18'
Start-Sleep -Seconds 1
$s = Get-State
Assert-That ($s.maxHealth -eq 40) "Health Boost V: max health 40 on the client ($($s.maxHealth))"
Assert-That ($s.health -ge 21 -and $s.health -le 23) "health about 22 ($($s.health))"
Lower-Hunger 12
$logAt = @(Get-LeviLog).Count
Select-Hotbar 1
$line = Wait-NewLogLine 'Health preview: health=2[123] -> .* hearts=1[01]\.\..* max=40' $logAt 15
Write-Step "beef, two rows: $line"
Shoot 'health-boost' 4

# --- Tooltip food values in the inventory --------------------------------------------------------
Open-Inventory
Move-Mouse 0.50 0.72 -Relative   # hotbar slot 1: cooked beef
Start-Sleep -Milliseconds 800
Save-Screenshot 'tooltip-beef' $u | Out-Null
[BsE2E.Native]::Key([uint16]0x10, $false)
try {
    Start-Sleep -Milliseconds 400
    Move-Mouse 0.534 0.72 -Relative   # slot 2: golden apple, Shift held
    Start-Sleep -Milliseconds 600
    Save-Screenshot 'tooltip-golden-apple-shift' $u | Out-Null
} finally { [BsE2E.Native]::Key([uint16]0x10, $true) }
Close-Screen

# --- F6: food debug line --------------------------------------------------------------------------
$logAt = @(Get-LeviLog).Count
Focus-Game
[BsE2E.Native]::Key([uint16]0x75, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::Key([uint16]0x75, $true)
Wait-NewLogLine 'Food debug info shown' $logAt 10 | Out-Null
Start-Sleep -Milliseconds 500
Shoot 'debug-line'
$logAt = @(Get-LeviLog).Count
[BsE2E.Native]::Key([uint16]0x75, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::Key([uint16]0x75, $true)
Wait-NewLogLine 'Food debug info hidden' $logAt 10 | Out-Null

# --- No client/server disagreement was reported, the server saw no errors -------------------------
Assert-That (-not (Get-LeviLog | Select-String 'Sync: client level rules differ')) "no rule mismatch warnings"
# (The console's own "ERROR" lines are command feedback such as "no effects to take".)
Assert-That (-not (Get-BdsLog | Select-String 'Player disconnected')) "the client stayed connected to the server"
Assert-That (-not $script:Bds.HasExited) "the server is still running"
Write-Step "final: $(Get-FoodStateLog)"
