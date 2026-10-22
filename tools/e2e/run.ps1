<#
.SYNOPSIS
  Run a BeetrootSkin end-to-end scenario against the real game.

.DESCRIPTION
  Takes the machine-wide e2e lock (shared with BEI), moves other mods (BEI) aside, deploys
  bin\BeetrootSkin, restores the BeetrootSkin-Test world, launches Minecraft through
  LeviLauncher, enters the world, runs tools\e2e\scenarios\<Scenario>.ps1, checks the
  LeviLamina log for errors and always closes the game, restores the other mods and releases
  the lock.

  Screenshots land in tools\e2e\out\<Scenario>\. Review them (they are the evidence).

.EXAMPLE
  pwsh tools/e2e/run.ps1 -Scenario smoke
  pwsh tools/e2e/run.ps1 -Scenario smoke -KeepOpen   # leave the game running for manual poking
#>
param(
    [Parameter(Mandatory)] [string]$Scenario,
    [switch]$NoDeploy,
    [switch]$KeepOpen
)

. "$PSScriptRoot/lib.ps1"
$ErrorActionPreference = 'Stop'

$scenarioFile = Join-Path $PSScriptRoot "scenarios/$Scenario.ps1"
if (-not (Test-Path $scenarioFile)) { throw "Unknown scenario: $scenarioFile" }

# A locked workstation delivers no input: wait for an unlock instead of failing (before taking the
# lock, so other projects are not blocked meanwhile, and again right before launching).
Wait-SessionUnlocked | Out-Null
Enter-E2ELock
$failed = $null
try {
    if (Get-GameProcess) { Write-Step "closing a running game first"; Stop-Game }
    # Leftovers of a killed run: options.txt backup (Disable-OtherMods puts mods back itself).
    Restore-GameOptions
    Disable-OtherMods
    if (-not $NoDeploy) { Deploy-Mod }
    Restore-TestWorld
    $script:Unit = $Scenario
    # Optional scenarios\<name>.prelaunch.ps1, e.g. to seed mods\BeetrootSkin\config before the mod loads.
    $prelaunch = Join-Path $PSScriptRoot "scenarios/$Scenario.prelaunch.ps1"
    if (Test-Path $prelaunch) { . $prelaunch }
    Wait-SessionUnlocked | Out-Null
    Start-Game | Out-Null
    Wait-GameReady
    # A prelaunch script may replace the world entry, e.g. remote.prelaunch.ps1:
    # Start-Bds; $script:EnterWorld = { Join-Bds }.
    if ($script:EnterWorld) { . $script:EnterWorld } else { Enter-TestWorld }
    . $scenarioFile
    Assert-NoErrors
    Write-Step "scenario '$Scenario' PASSED"
} catch {
    $failed = $_
    Write-Host "[e2e] scenario '$Scenario' FAILED: $_" -ForegroundColor Red
    try { Save-Screenshot 'failure' $Scenario | Out-Null } catch {}
    Get-LeviLog | Select-Object -Last 40 | ForEach-Object { Write-Host "  | $_" }
} finally {
    # Every cleanup step runs even when an earlier one fails.
    if (-not $KeepOpen) {
        try { Stop-Game } catch { Write-Host "[e2e] stopping the game failed: $_" -ForegroundColor Red }
    }
    # The dedicated server (if the scenario started one) always stops: its console pipes belong to
    # this process, so it could not outlive it usefully, not even with -KeepOpen.
    try { Stop-Bds } catch { Write-Host "[e2e] stopping BDS failed: $_" -ForegroundColor Red }
    if (-not $KeepOpen) {
        # Optional scenarios\<name>.restore.ps1 undoes what the prelaunch script changed.
        $restore = Join-Path $PSScriptRoot "scenarios/$Scenario.restore.ps1"
        if (Test-Path $restore) {
            try {
                & pwsh -NoProfile -File $restore
                if ($LASTEXITCODE -ne 0) { throw "exit code $LASTEXITCODE" }
            } catch { Write-Host "[e2e] $restore failed: $_" -ForegroundColor Red }
        }
        # The game writes options.txt when it exits: restore it only afterwards. (With -KeepOpen
        # the next run, or tools\e2e\restore-mods.ps1, puts it back.)
        try { Restore-GameOptions } catch { Write-Host "[e2e] Restore-GameOptions failed: $_" -ForegroundColor Red }
    }
    # Mods are loaded at game start, so BEI can go back right away even with -KeepOpen.
    try { Restore-OtherMods | Out-Null } catch { Write-Host "[e2e] Restore-OtherMods failed: $_" -ForegroundColor Red }
    Exit-E2ELock
}
if ($failed) { exit 1 }
