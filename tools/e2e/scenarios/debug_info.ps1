# Food debug info (AppleSkin's F3 line) and its F6 toggle hotkey.
# Screenshots: out/debug_info/
#   on.png           right after F6: the line and the "AppleSkin debug info: on" action bar text
#   exhaustion.png   after sprinting/jumping: exh value grew
#   coordinates.png  with /gamerule showcoordinates + showdaysplayed: vanilla text and chat stay below the line
#   hide_gui.png     F1 (hide GUI) hides the line too
#   off.png          after F6 again: line gone, "off" message
#   chat_f6.png      F6 with the chat screen open: ignored
# The key's presence in the game's keyboard layout (what Settings > Keyboard & Mouse lists) is
# asserted through the "hotkey mapped to key code 117" log line. The settings screens are
# OreUI and are neither captured by PrintWindow nor reachable with ESC from this harness, so
# the menu entry itself is not screenshotted.
$u = 'debug_info'
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"

function Press-VK([int]$vk, [int]$holdMs = 60) {
    Assert-GameForeground
    [BsE2E.Native]::Key([uint16]$vk, $false); Start-Sleep -Milliseconds $holdMs; [BsE2E.Native]::Key([uint16]$vk, $true)
    Start-Sleep -Milliseconds 300
}
function Get-ConfigFlag { (Get-Content $cfg -Raw | ConvertFrom-Json).showFoodDebugInfo }
$VK_F1 = 0x70; $VK_F6 = 0x75; $VK_W = 0x57; $VK_CTRL = 0x11; $VK_SPACE = 0x20

Wait-LogLine 'Food state: ' 30 | Out-Null
Wait-LogLine 'Food debug info hotkey mapped to key code 117' 10 | Out-Null
Reset-Player
Invoke-Chat '/gamerule showcoordinates false'
Invoke-Chat '/gamerule showdaysplayed false'
Start-Sleep -Seconds 1
if (Get-ConfigFlag) { throw "showFoodDebugInfo should start false" }

# --- F6: on --------------------------------------------------------------------------------
$before = @(Get-LeviLog).Count
Press-VK $VK_F6
Wait-NewLogLine 'Food debug info shown' $before 10 | Out-Null
Start-Sleep -Milliseconds 300
Save-Screenshot 'on' $u | Out-Null
if (-not (Get-ConfigFlag)) { throw "config.json was not saved with showFoodDebugInfo=true" }
Write-Step "config.json: showFoodDebugInfo=true"

# --- Exhaustion: sprint and jump ------------------------------------------------------------
Assert-GameForeground
[BsE2E.Native]::Key([uint16]$VK_CTRL, $false)
[BsE2E.Native]::Key([uint16]$VK_W, $false)
for ($i = 0; $i -lt 6; $i++) {
    Start-Sleep -Milliseconds 400
    [BsE2E.Native]::Key([uint16]$VK_SPACE, $false); Start-Sleep -Milliseconds 80; [BsE2E.Native]::Key([uint16]$VK_SPACE, $true)
}
[BsE2E.Native]::Key([uint16]$VK_W, $true)
[BsE2E.Native]::Key([uint16]$VK_CTRL, $true)
Start-Sleep -Milliseconds 800
Save-Screenshot 'exhaustion' $u | Out-Null

# --- Vanilla position/day text and chat start below our line ------------------------------
Invoke-Chat '/gamerule showcoordinates true'
Invoke-Chat '/gamerule showdaysplayed true'
Start-Sleep -Seconds 1
Save-Screenshot 'coordinates' $u | Out-Null
Invoke-Chat '/gamerule showdaysplayed false'
Start-Sleep -Seconds 1
Save-Screenshot 'coordinates_only' $u | Out-Null
Start-Sleep -Seconds 8 # let the chat messages fade
Save-Screenshot 'coordinates_clean' $u | Out-Null

# --- F1 hides the HUD, including our line ---------------------------------------------------
Press-VK $VK_F1
Start-Sleep -Milliseconds 500
Save-Screenshot 'hide_gui' $u | Out-Null
Press-VK $VK_F1
Start-Sleep -Milliseconds 500

# --- F6: off ---------------------------------------------------------------------------------
$before = @(Get-LeviLog).Count
Press-VK $VK_F6
Wait-NewLogLine 'Food debug info hidden' $before 10 | Out-Null
Start-Sleep -Milliseconds 300
Save-Screenshot 'off' $u | Out-Null
if (Get-ConfigFlag) { throw "config.json was not saved with showFoodDebugInfo=false" }
Write-Step "config.json: showFoodDebugInfo=false"

# --- The hotkey is ignored while a screen (chat) is open ---------------------------------------
Focus-Game
Press-Key T; Start-Sleep -Milliseconds 700
$before = @(Get-LeviLog).Count
Press-VK $VK_F6
Start-Sleep -Seconds 1
Save-Screenshot 'chat_f6' $u | Out-Null
Press-Key ESC; Start-Sleep -Milliseconds 1500
if (@(Get-LeviLog | Select-Object -Skip $before | Select-String 'Food debug info (shown|hidden)').Count -gt 0) {
    throw "F6 toggled the debug info while the chat screen was open"
}
