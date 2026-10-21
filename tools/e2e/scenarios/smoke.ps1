# Smoke test: the mod loads, reads the player's food state, draws a mod-owned texture at the
# hunger bar (debug layer) and reserves space in an item tooltip (debug extension).
# Screenshots: out/smoke/{hud,inventory,tooltip}.png
$u = 'smoke'
Wait-LogLine 'Food state: ' 30 | Out-Null
Reset-Player
Give-Item golden_apple 4      # hotbar slot 1
Give-Item rotten_flesh 8      # hotbar slot 2
Drop-Hunger 3
Hurt-Player 6
Select-Hotbar 1
Start-Sleep -Seconds 2
Write-Step "food state: $(Get-FoodStateLog)"
Wait-LogLine 'Debug: mod texture icons.png is ready' 20 | Out-Null
Wait-LogLine 'Debug: held food hunger=4 ' 20 | Out-Null
Save-Screenshot 'hud' $u | Out-Null
Open-Inventory
Save-Screenshot 'inventory' $u | Out-Null
Move-Mouse 0.50 0.72 -Relative   # hotbar slot 1 (golden apple) in the survival inventory at 1600x900
Start-Sleep -Milliseconds 800
Save-Screenshot 'tooltip' $u | Out-Null
Wait-LogLine 'Debug: hovered minecraft:golden_apple' 10 | Out-Null
Wait-LogLine 'Tooltip box: ' 10 | Out-Null
Move-Mouse 0.534 0.72 -Relative  # hotbar slot 2 (rotten flesh)
Start-Sleep -Milliseconds 800
Save-Screenshot 'tooltip-rotten' $u | Out-Null
Wait-LogLine 'Debug: hovered minecraft:rotten_flesh -> .*rotten=true' 10 | Out-Null
Close-Screen
