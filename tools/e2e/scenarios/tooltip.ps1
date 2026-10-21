# Food values in item tooltips (AppleSkin TooltipOverlayHandler), with
# showFoodValuesInTooltipAlways = false (see tooltip.prelaunch.ps1): every food is hovered
# without and with Shift held (raw key down), in the survival inventory, the creative inventory
# and a chest.
# Screenshots: out/tooltip/<n>-<item>-{plain,shift}.png, creative-*.png, chest-*.png
$u = 'tooltip'
$foods = @('golden_apple', 'rotten_flesh', 'cooked_beef', 'cake', 'suspicious_stew', 'enchanted_golden_apple', 'dried_kelp')
$SlotX0 = 0.50; $SlotDX = 0.034; $SlotY = 0.72   # survival inventory hotbar at 1600x900
$script:ChestSlotX = 0.36; $script:ChestSlotY = 0.30        # first chest slot
$script:CreativeSlotX = 0.50; $script:CreativeSlotY = 0.80  # creative hotbar slot 1

function Hover-WithShift([double]$x, [double]$y, [string]$name) {
    Move-Mouse $x $y -Relative
    Start-Sleep -Milliseconds 700
    Save-Screenshot "$name-plain" $u | Out-Null
    [BsE2E.Native]::Key([uint16]0x10, $false)       # raw SHIFT down
    try {
        Start-Sleep -Milliseconds 400
        Move-Mouse ($x + 0.002) $y -Relative
        Start-Sleep -Milliseconds 500
        Save-Screenshot "$name-shift" $u | Out-Null
    } finally {
        [BsE2E.Native]::Key([uint16]0x10, $true)    # SHIFT up
    }
    Start-Sleep -Milliseconds 200
}

Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
foreach ($f in $foods) { Give-Item $f 1 }
Start-Sleep -Seconds 1

# Survival inventory.
Open-Inventory
for ($i = 0; $i -lt $foods.Count; $i++) {
    Hover-WithShift ($SlotX0 + $SlotDX * $i) $SlotY ("{0}-{1}" -f ($i + 1), $foods[$i])
}
Close-Screen

# Chest: golden apple in the first chest slot, cooked beef in the second.
# The player looks south (+z) and 60 degrees down, at the chest's front face.
Invoke-Chat '/setblock ~ ~ ~1 chest'
Invoke-Chat '/replaceitem block ~ ~ ~1 slot.container 0 golden_apple 1'
Invoke-Chat '/replaceitem block ~ ~ ~1 slot.container 1 cooked_beef 1'
Invoke-Chat '/tp @s ~ ~ ~ 0 60'
Start-Sleep -Seconds 1
Click 0.5 0.5 -Relative -Right
Start-Sleep -Milliseconds 1500
Save-Screenshot 'chest-screen' $u | Out-Null
Hover-WithShift $script:ChestSlotX $script:ChestSlotY 'chest-golden_apple'
Close-Screen

# Creative inventory.
Set-Creative
Start-Sleep -Seconds 1
Open-Inventory
Save-Screenshot 'creative-screen' $u | Out-Null
Hover-WithShift $script:CreativeSlotX $script:CreativeSlotY 'creative-golden_apple'
Close-Screen
Set-Survival
