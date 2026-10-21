# Never leave the dedicated server running: kill any bedrock_server.exe started from
# %LOCALAPPDATA%\BeetrootSkin-bds (run.ps1 already stops the one it started; this also covers
# runs that were interrupted). Then drop the server-list entry Join-Bds added and put the
# showFoodDebugInfo value remote.prelaunch.ps1 replaced back.
. "$PSScriptRoot/../lib.ps1"
Stop-Bds
Remove-BdsServerEntries
$saved = Join-Path $script:OutDir 'remote\showFoodDebugInfo.orig'
$cfg = Join-Path $script:ModsDir "$script:ModName\config\config.json"
if ((Test-Path $saved) -and (Test-Path $cfg)) {
    $value = (Get-Content $saved -Raw).Trim() -eq 'True'
    $json = Get-Content $cfg -Raw | ConvertFrom-Json
    $json | Add-Member -Force -NotePropertyName showFoodDebugInfo -NotePropertyValue $value
    $json | ConvertTo-Json -Depth 8 | Set-Content $cfg -Encoding utf8
    Remove-Item $saved -Force
    Write-Step "showFoodDebugInfo restored to $value"
}
