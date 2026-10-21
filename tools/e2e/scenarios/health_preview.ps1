# Health-restored HUD preview (AppleSkin HealthRestored overlay).
# Survival, normal difficulty, hunger ~11 (below 18), health 8:
#   cooked_beef  -> 11 + 8 >= 18, natural regen after eating: flashing hearts from 8 up
#   golden_apple -> 11 + 4 < 18, but Regeneration II 5 s heals 4 HP: two flashing hearts
#   bread        -> 11 + 5 < 18 and no effect: nothing
#   health_boost (max 40 HP, two rows), health 22, cooked_beef: hearts on the second row
#   peaceful     -> nothing
# Screenshots: out/health_preview/*.png (several per case, the overlay flashes).
$u = 'health_preview'
Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
Give-Item cooked_beef 4   # hotbar slot 1
Give-Item golden_apple 4  # slot 2
Give-Item bread 4         # slot 3
Select-Hotbar 4           # empty hand while preparing

function Get-Hunger {
    $line = Get-FoodStateLog
    if ($line -match 'hunger=(\d+) ') { return [int]$Matches[1] }
    return 20
}

# Burn saturation and hunger slowly (hunger effect amplifier 19 = 0.1 exhaustion per tick,
# about half a point per second) down to 11-12. Saturation is whatever the world had (the
# Bedrock Saturation effect used by Reset-Player only restores hunger), so no fast Drop-Hunger.
if ((Get-Hunger) -gt 12) {
    Invoke-Chat '/effect @s hunger 120 19 true'
    $deadline = (Get-Date).AddSeconds(90)
    while ((Get-Hunger) -gt 12 -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    Clear-Effects
}
Start-Sleep -Seconds 1
$hunger = Get-Hunger
Write-Step "hunger now $hunger"
if ($hunger -lt 10 -or $hunger -gt 12) { throw "could not bring hunger to 10..12 (got $hunger)" }

Hurt-Player 12            # 20 -> 8 HP
Start-Sleep -Seconds 1
Write-Step "food state: $(Get-FoodStateLog)"
# Damage adds a little exhaustion; beef (8) must still lift hunger to 18.
if ((Get-Hunger) -lt 10) { throw "hunger fell below 10 after the damage: $(Get-FoodStateLog)" }

function Shoot([string]$name) {
    # The overlay pulses (~1.6 s period); take a few frames so at least one is bright.
    for ($i = 1; $i -le 4; ++$i) { Save-Screenshot "$name-$i" $u | Out-Null; Start-Sleep -Milliseconds 330 }
}

$mark = @(Get-LeviLog).Count
Select-Hotbar 1
$line = Wait-NewLogLine 'Health preview: health=8 -> ' $mark 15
Write-Step "beef: $line"
Shoot 'beef'

$mark = @(Get-LeviLog).Count
Select-Hotbar 2
$line = Wait-NewLogLine 'Health preview: health=8 -> 12 hearts=4..5' $mark 15
Write-Step "golden apple: $line"
Shoot 'golden_apple'

$mark = @(Get-LeviLog).Count
Select-Hotbar 3
$line = Wait-NewLogLine 'Health preview: none' $mark 15
Write-Step "bread: $line"
Shoot 'bread'

# Two rows of hearts: Health Boost V (+20 max HP), heal up, then hurt to 22 HP.
Select-Hotbar 4
Invoke-Chat '/effect @s health_boost 600 4 true'
Invoke-Chat '/effect @s instant_health 1 5 true'
Start-Sleep -Seconds 2
Hurt-Player 18
Start-Sleep -Seconds 1
Write-Step "food state: $(Get-FoodStateLog)"
if ((Get-Hunger) -lt 10) { throw "hunger fell below 10 after the damage: $(Get-FoodStateLog)" }
$mark = @(Get-LeviLog).Count
Select-Hotbar 1
$line = Wait-NewLogLine 'Health preview: health=22 -> .* hearts=11\.\.' $mark 15
Write-Step "beef, two rows: $line"
Shoot 'health_boost'

# Peaceful: no preview.
$mark = @(Get-LeviLog).Count
Set-Difficulty peaceful
$line = Wait-NewLogLine 'Health preview: none' $mark 15
Write-Step "peaceful: $line"
Shoot 'peaceful'
Set-Difficulty normal
