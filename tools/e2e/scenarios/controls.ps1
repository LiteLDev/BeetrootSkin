# The food debug info hotkey in Settings > Keyboard & Mouse: listed under its localized name
# (app/KeyNames.cpp, lang key beetroot.key.toggle_food_debug_info) with its default key F6.
# The game language comes from the prelaunch script (controls: en_US, controls_zh: zh_CN; the
# original options.txt is restored by run.ps1).
# Screenshots: out/<scenario>/
#   pause.png      the pause menu (ESC from the HUD)
#   keyboard.png   Settings > Keyboard & Mouse
#   key.png        the BeetrootSkin key row ("<name>  F6"), right above the CHORDS section
#   hud.png        back in the world after closing the settings and the pause menu
# The name the menu got is asserted through BeetrootSkin's "Key name requested: <id> -> '<text>'"
# log line; the screenshot shows it on screen.
$u = $script:Unit
$locale = if ($script:ControlsLocale) { $script:ControlsLocale } else { 'en_US' }
$langFile = Join-Path $script:RepoRoot "assets\lang\$locale\debug_info.json"
$expected = (Get-Content $langFile -Raw -Encoding utf8 | ConvertFrom-Json).'beetroot.key.toggle_food_debug_info'
if (-not $expected) { throw "beetroot.key.toggle_food_debug_info missing from $langFile" }
Write-Step "expecting the key name '$expected' ($locale)"

Wait-LogLine 'Food state: ' 30 | Out-Null
Wait-LogLine 'Food debug info hotkey mapped to key code 117' 10 | Out-Null
Start-Sleep -Seconds 1

# The name is only requested by the controls menu (logged once per name and language).
$beforeMenu = @(Get-LeviLog).Count
Open-PauseMenu
Save-Screenshot 'pause' $u | Out-Null
Open-Settings
Open-KeyboardSettings
Save-Screenshot 'keyboard' $u | Out-Null

# The mod key is the last entry of the mapping list, right above the CHORDS section; from the
# end of the pane that is 20 wheel notches up (one notch = 40 px at 1600x900).
Scroll-SettingsToEnd
Save-Screenshot 'end' $u | Out-Null
Scroll-Notches 20
Save-Screenshot 'key' $u | Out-Null

$line = Wait-NewLogLine "Key name requested: key\.BeetrootSkin\.toggle_food_debug_info(\.name)? -> '" $beforeMenu 10
Write-Step $line
if (-not $line.EndsWith("-> '$expected'")) { throw "The controls menu got the wrong key name: $line (expected '$expected')" }

Close-Settings
Save-Screenshot 'hud' $u | Out-Null
# Back in the world: F6 still toggles (the settings screens did not swallow the key mapping).
$before = @(Get-LeviLog).Count
Assert-GameForeground
[BsE2E.Native]::Key([uint16]0x75, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::Key([uint16]0x75, $true)
Wait-NewLogLine 'Food debug info (shown|hidden)' $before 10 | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'f6' $u | Out-Null
# Toggle back so the saved showFoodDebugInfo is unchanged.
$before = @(Get-LeviLog).Count
Assert-GameForeground
[BsE2E.Native]::Key([uint16]0x75, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::Key([uint16]0x75, $true)
Wait-NewLogLine 'Food debug info (shown|hidden)' $before 10 | Out-Null
