# Food-state accuracy / "sync" (features/sync): the food state is right after eating, sprinting,
# damage, /gamerule naturalregeneration, /difficulty, death + respawn and leave + rejoin, both
# from the integrated server and from the client's synced attributes (the remote-server path,
# forced with the force_client_sync marker), and the two agree.
# Screenshots: out/sync/{eaten,forced-peaceful,respawned,pause,title,rejoined}.png
$u = 'sync'
$marker = Join-Path $script:ModsDir "$script:ModName\force_client_sync"
if (Test-Path $marker) { Remove-Item $marker -Force }

# Food state line -> hashtable (hunger, saturation, exhaustion, health, peaceful, naturalRegen, source).
function ConvertFrom-FoodState([string]$line) {
    if ($line -notmatch 'hunger=(\d+) saturation=([\d.]+) exhaustion=([\d.]+) health=([\d.]+)/[\d.]+ peaceful=(\w+) naturalRegen=(\w+) creative=\w+ source=([\w-]+)') {
        throw "Not a food state line: $line"
    }
    @{ hunger = [int]$Matches[1]; saturation = [double]$Matches[2]; exhaustion = [double]$Matches[3]; health = [double]$Matches[4]
       peaceful = $Matches[5]; naturalRegen = $Matches[6]; source = $Matches[7] }
}
function Get-State { ConvertFrom-FoodState (Get-FoodStateLog) }
function Assert-That([bool]$cond, [string]$what) { if (-not $cond) { throw "Assertion failed: $what" }; Write-Step "ok: $what" }
# The last "Sync compare" line logged after $after (values settled) must show the same hunger,
# saturation and health on the client as on the integrated server, and exhaustion within 0.1:
# the server syncs exhaustion, but not every tiny step (e.g. 0.050 -> 0.051) right away.
# Returns the server's exhaustion.
function Assert-CompareConsistent([int]$after) {
    $lines = @(Get-LeviLog | Select-Object -Skip $after | Select-String 'Sync compare: ' | ForEach-Object { $_.Line })
    Assert-That ($lines.Count -gt 0) "client/server compare lines were logged ($($lines.Count))"
    $re = 'hunger=(\d+) saturation=([\d.]+) exhaustion=([\d.]+) health=([\d.]+)'
    $null = $lines[-1] -match "client $re \| server $re"
    $m = $Matches
    $ok = $m[1] -eq $m[5] -and $m[2] -eq $m[6] -and $m[4] -eq $m[8] -and [Math]::Abs([double]$m[3] - [double]$m[7]) -le 0.1
    Assert-That $ok "client attributes match the integrated server's: $($lines[-1] -replace '.*Sync compare: ', '')"
    return [double]$m[7]
}

# --- Setup: survival, normal, a 41x41 stone platform in the air (the fixture spawn has water) --
Reset-Player
Invoke-Chat '/gamerule naturalregeneration true'
Invoke-Chat '/gamerule doimmediaterespawn true'
Invoke-Chat '/fill ~-20 119 ~-20 ~20 119 ~20 stone'
Invoke-Chat '/tp @s ~ 120 ~ 0 0'
Start-Sleep -Seconds 1
Invoke-Chat '/spawnpoint @s ~ ~ ~'
Start-Sleep -Seconds 2
$s = Get-State
Assert-That ($s.source -eq 'integrated-server') "food state comes from the integrated server in single-player"
Assert-That ($s.naturalRegen -eq 'true' -and $s.peaceful -eq 'false') "normal difficulty, natural regeneration on"

# --- Eat: hunger and saturation rise by the food's values -------------------------------------
Drop-Hunger 2
Start-Sleep -Seconds 2
$before = Get-State
Assert-That ($before.hunger -le 14) "hunger dropped ($($before.hunger))"
Give-Item cooked_beef 4
Select-Hotbar 1
Focus-Game
[BsE2E.Native]::MouseButton(1, $false); Start-Sleep -Milliseconds 2200; [BsE2E.Native]::MouseButton(1, $true)
Start-Sleep -Seconds 2
$after = Get-State
# Cooked beef: 8 hunger, 12.8 saturation (capped at the new hunger).
Assert-That ($after.hunger -eq [Math]::Min(20, $before.hunger + 8)) "eating cooked beef adds 8 hunger ($($before.hunger) -> $($after.hunger))"
$expectedSat = [Math]::Min([double]$after.hunger, $before.saturation + 12.8)
Assert-That ([Math]::Abs($after.saturation - $expectedSat) -lt 0.05) "saturation after eating is $expectedSat ($($after.saturation))"
Save-Screenshot 'eaten' $u | Out-Null

# --- Remote-server path: client attributes vs the integrated server ---------------------------
$logAt = @(Get-LeviLog).Count
New-Item -ItemType File -Force $marker | Out-Null
Wait-NewLogLine 'Sync: food state source integrated-server -> client \(forced' $logAt 10 | Out-Null
$ex0 = Assert-CompareConsistent $logAt
# Sprint-jump forward for ~1.5 s (about 10 m; the platform reaches 20 m ahead).
Focus-Game
[BsE2E.Native]::Key(0x11, $false); [BsE2E.Native]::Key([uint16][char]'W', $false)
for ($i = 0; $i -lt 3; $i++) { Start-Sleep -Milliseconds 400; [BsE2E.Native]::Key(0x20, $false); Start-Sleep -Milliseconds 100; [BsE2E.Native]::Key(0x20, $true) }
[BsE2E.Native]::Key([uint16][char]'W', $true); [BsE2E.Native]::Key(0x11, $true)
Start-Sleep -Seconds 2
$ex1 = Assert-CompareConsistent $logAt
$sat1 = (Get-State).saturation
Assert-That ($ex1 - $ex0 -ge 0.3 -or $sat1 -lt $after.saturation) "sprint-jumping added exhaustion (server $ex0 -> $ex1, saturation $sat1)"
Hurt-Player 4
Start-Sleep -Seconds 2
$s = Get-State
Assert-That ($s.source -eq 'client') "forced: food state from client attributes"
Assert-That ($s.health -le 16) "damage shows on the client copy (health $($s.health))"
Assert-CompareConsistent $logAt | Out-Null

Invoke-Chat '/gamerule naturalregeneration false'
Wait-NewLogLine 'Sync: server sent game rules: naturalRegeneration=false' $logAt 10 | Out-Null
Wait-NewLogLine 'Sync: client level peaceful=false naturalRegen=false' $logAt 10 | Out-Null
Invoke-Chat '/difficulty peaceful'
Wait-NewLogLine 'Sync: client level peaceful=true naturalRegen=false' $logAt 10 | Out-Null
Wait-NewLogLine 'Food state: .*peaceful=true naturalRegen=false .*source=client' $logAt 10 | Out-Null
Start-Sleep -Seconds 2
Assert-CompareConsistent $logAt | Out-Null
Save-Screenshot 'forced-peaceful' $u | Out-Null
if (Get-LeviLog | Select-Object -Skip $logAt | Select-String 'Sync: client level rules differ') { throw "client level rules disagreed with the server" }

Remove-Item $marker -Force
Wait-NewLogLine 'Sync: food state source client \(forced by force_client_sync\) -> integrated-server' $logAt 10 | Out-Null
Wait-NewLogLine 'Food state: .*peaceful=true naturalRegen=false .*source=integrated-server' $logAt 10 | Out-Null

# --- Death and respawn: fresh values, no stale state -----------------------------------------
Invoke-Chat '/difficulty normal'
Drop-Hunger 1
$logAt = @(Get-LeviLog).Count
Invoke-Chat '/kill @s'
Start-Sleep -Seconds 4
Focus-Game
$line = Wait-NewLogLine 'Food state: hunger=20 saturation=[\d.]+ exhaustion=[\d.]+ health=20.0/20.0 peaceful=false naturalRegen=false creative=false source=integrated-server' $logAt 20
Write-Step "after respawn: $line"
Save-Screenshot 'respawned' $u | Out-Null

# --- Leave and rejoin: state cleared on exit, rules re-read from the saved world ---------------
$logAt = @(Get-LeviLog).Count
# The rejoin must see what "Save & Quit" wrote. Under load (several games at once) the save can
# still be in flight when the title screen shows; rejoining then loads the previous save (old game
# rules, pre-death player). So wait until level.dat has been rewritten after the quit.
$levelDat   = Join-Path (Join-Path (Get-WorldsDir) $script:WorldName) 'level.dat'
$savedAfter = Get-Date
Focus-Game
Press-Key ESC; Start-Sleep -Seconds 2
Save-Screenshot 'pause' $u | Out-Null
Click 0.294 0.667 -Relative  # "Save & Quit" on the pause screen at 1600x900 (left column)
Wait-NewLogLine 'Sync: left level' $logAt 20 | Out-Null
$deadline = (Get-Date).AddSeconds(60)
while ((Get-Item $levelDat).LastWriteTime -lt $savedAfter) {
    if ((Get-Date) -gt $deadline) { throw "level.dat was not saved within 60 s of Save & Quit ($levelDat)" }
    Start-Sleep -Milliseconds 500
}
# level.dat is not enough: LevelDB flushes db\ separately and holds db\LOCK until the integrated
# server has closed the database. Reopening the world before that loads the previous save.
$dbLock = Join-Path (Split-Path $levelDat) 'db\LOCK'
while ($true) {
    try { [IO.File]::Open($dbLock, 'Open', 'ReadWrite', 'None').Dispose(); break }
    catch [IO.FileNotFoundException] { break }
    catch [IO.IOException] {
        if ((Get-Date) -gt $deadline) { throw "the world's LevelDB was still open 60 s after Save & Quit ($dbLock)" }
        Start-Sleep -Milliseconds 250
    }
}
Write-Step "world saved and closed (level.dat $((Get-Item $levelDat).LastWriteTime.ToString('HH:mm:ss.fff')), db closed $((Get-Date).ToString('HH:mm:ss.fff')))"
Write-WorldSaveState (Split-Path $levelDat) 'naturalregeneration'
Start-Sleep -Seconds 5
Save-Screenshot 'title' $u | Out-Null
Enter-TestWorld
Wait-NewLogLine 'Sync: food state source \w+ -> integrated-server' $logAt 30 | Out-Null
# What BeetrootSkin must get right on a rejoin: nothing from the previous session leaks through,
# and its view equals what the (re)loaded world says. Whether the game reopens the save it just
# wrote is the game's business: on a quick quit-and-rejoin it sometimes reopens an older state
# (verified: level.dat on disk had the new rule, the reopened world had the old one, on any slot).
$rules = Wait-NewLogLine 'Sync: server sent game rules: .*naturalRegeneration=(true|false)' $logAt 30
$serverRegen = [regex]::Match($rules, 'naturalRegeneration=(true|false)').Groups[1].Value
# An older save can also have the earlier Peaceful difficulty (difficulty 0).
$difficulty = Wait-NewLogLine 'Sync: server sent difficulty \d+' $logAt 30
$serverPeaceful = if ([regex]::Match($difficulty, 'difficulty (\d+)').Groups[1].Value -eq '0') { 'true' } else { 'false' }
Wait-NewLogLine "Sync: client level peaceful=$serverPeaceful naturalRegen=$serverRegen" $logAt 30 | Out-Null
Start-Sleep -Seconds 2
$s = Get-State
Assert-That ($s.source -eq 'integrated-server' -and $s.naturalRegen -eq $serverRegen -and $s.peaceful -eq $serverPeaceful) "after rejoin: integrated server source, peaceful=$serverPeaceful naturalRegen=$serverRegen as the reloaded world says"
if ($serverRegen -ne 'false' -or $serverPeaceful -ne 'false') {
    Write-Step "note: the game reopened an older save of the world (peaceful=$serverPeaceful, naturalRegeneration=$serverRegen, hunger $($s.hunger)); BeetrootSkin followed it"
} else {
    Assert-That ($s.hunger -eq 20 -and $s.health -eq 20) "after rejoin: values of the respawned player (hunger $($s.hunger), health $($s.health))"
}
Save-Screenshot 'rejoined' $u | Out-Null
