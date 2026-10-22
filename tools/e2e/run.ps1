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

  By default the run is headless: the LeviHeadlessInput mod (sibling project, see
  $script:HeadlessInputDir in lib.ps1) is deployed next to BeetrootSkin and every click, key and
  screenshot goes through its control pipe, so the game stays in the background and never takes
  the real mouse, keyboard or foreground. -Foreground uses real input (SendInput) as before.

.EXAMPLE
  pwsh tools/e2e/run.ps1 -Scenario smoke
  pwsh tools/e2e/run.ps1 -Scenario smoke -KeepOpen   # leave the game running for manual poking
  pwsh tools/e2e/run.ps1 -Scenario smoke -Foreground # real mouse/keyboard (takes over the PC)
  pwsh tools/e2e/run.ps1 -Scenario smoke -Slot 2     # a specific game instance (see -Slot)

.PARAMETER Slot
  Game instances the run may use (headless only; foreground runs always use slot 1, the
  LeviLauncher instance). The first free one is taken. Default: $env:LEVI_E2E_SLOTS ("1,2,3") or
  1. Slots 2.. are clones (versions\<base>-hi<N>) created on first use, so several runs (of this
  and other projects) can play at the same time. Only use them once every harness on the machine
  touches its own instance's game only (older BEI/AlleviatedBackpacks harnesses focus, feed and
  close every Minecraft.Windows process).
#>
param(
    [Parameter(Mandatory)] [string]$Scenario,
    [switch]$NoDeploy,
    [switch]$KeepOpen,
    [switch]$Foreground,
    [int[]]$Slot
)

. "$PSScriptRoot/lib.ps1"
$ErrorActionPreference = 'Stop'

if (-not $Foreground) {
    $shim = Join-Path $script:HeadlessInputDir 'harness-shim.ps1'
    if (-not (Test-Path $shim)) {
        throw "LeviHeadlessInput is not built ($shim missing). Build it (xmake in its project folder), set `$env:LEVI_HEADLESS_INPUT_DIR, or pass -Foreground to use the real mouse and keyboard."
    }
    # Replaces Click/Press-Key/Type-Text/Scroll/Move-Mouse/Focus-Game/Assert-GameForeground/
    # Set-GameWindowSize/Get-ClientBitmap and routes [BsE2E.Native]::Key/MouseButton/... used by
    # scenarios through the mod's pipe.
    . $shim
    Register-HeadlessNative ([BsE2E.Native])
}

$scenarioFile = Join-Path $PSScriptRoot "scenarios/$Scenario.ps1"
if (-not (Test-Path $scenarioFile)) { throw "Unknown scenario: $scenarioFile" }

# A locked workstation delivers no input: wait for an unlock instead of failing (before taking the
# lock, so other projects are not blocked meanwhile, and again right before launching).
Wait-SessionUnlocked | Out-Null
$slots = if ($Foreground) { @(1) } elseif ($Slot) { $Slot } elseif ($env:LEVI_E2E_SLOTS) {
    @($env:LEVI_E2E_SLOTS -split '[,; ]+' | Where-Object { $_ } | ForEach-Object { [int]$_ }) } else { @(1) }
Enter-E2ELock -Slots $slots
$failed = $null
try {
    # A clone instance is made on first use (cheap: hard links); it keeps its own mods and data.
    if ($script:InstanceSlot -gt 1) { & (Join-Path $script:HeadlessInputDir 'clone-instance.ps1') -Slot $script:InstanceSlot -Source $script:BaseVersion }
    if (Get-GameProcess) { Write-Step "closing a running game first"; Stop-Game }
    # Leftovers of a killed run: options.txt backup (Disable-OtherMods puts mods back itself).
    Restore-GameOptions
    Disable-OtherMods
    if (-not $NoDeploy) { Deploy-Mod }
    Remove-HeadlessInputMod   # leftover of a killed or -KeepOpen run
    if ($script:Headless) { Install-HeadlessInputMod -ModsDir $script:ModsDir -PipeName $script:HeadlessPipe }
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
        try { Remove-HeadlessInputMod } catch { Write-Host "[e2e] removing LeviHeadlessInput failed: $_" -ForegroundColor Red }
    } elseif ($script:Headless) {
        # Hand the open game to the human (real input again) and keep a later normal start passive;
        # the next run removes the mod.
        try {
            Set-HeadlessBackground $false | Out-Null
            $cfg = Join-Path $script:ModsDir 'LeviHeadlessInput\config\config.json'
            if (Test-Path $cfg) { '{ "background": false }' | Set-Content -Encoding utf8 $cfg }
            Write-Step "headless input off: the game takes real input again"
        } catch { Write-Host "[e2e] switching headless input off failed: $_" -ForegroundColor Red }
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
