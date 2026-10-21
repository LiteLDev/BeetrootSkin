# Runs before the game starts: English game language for the controls scenario (the original
# options.txt is backed up by Set-GameOption and restored by run.ps1 after the game exits).
Set-GameOption 'game_language' 'en_US'
$script:ControlsLocale = 'en_US'
