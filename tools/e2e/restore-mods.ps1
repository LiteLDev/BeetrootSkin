<#
.SYNOPSIS
  Manual recovery after an interrupted (killed) e2e run.

.DESCRIPTION
  Puts back what run.ps1 changes outside the mod folder and normally restores in `finally`:
  - every mod folder in mods-disabled\ (e.g. BEI) is moved back into mods\ (a copy that was
    deployed again meanwhile wins; the older one is kept as mods-disabled\<name>.stale-<time>);
  - options.txt keys changed by Set-GameOption (e.g. game_language for controls_zh) get their
    original values back (recorded in options.txt.bs-e2e-orig).
  Other projects' folders (e.g. mods.abp-e2e-aside) are not touched.

  Takes the shared e2e lock first so it never interferes with a running scenario (waits up to
  -WaitMinutes). The next run.ps1 does the same automatically before it starts.

.EXAMPLE
  pwsh -NoProfile -File tools/e2e/restore-mods.ps1
#>
param([int]$WaitMinutes = 10)

. "$PSScriptRoot/lib.ps1"
$ErrorActionPreference = 'Stop'

Enter-E2ELock -TimeoutMinutes $WaitMinutes
try {
    if (Get-GameProcess) { throw "Minecraft is running; close it first (it rewrites options.txt on exit)" }
    Restore-GameOptions
    if (Restore-OtherMods) { Write-Step "nothing left in $script:DisabledModsDir" }
    else { exit 1 }
} finally {
    Exit-E2ELock
}
