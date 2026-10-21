# BeetrootSkin end-to-end tests

These tests drive the **real game** on this machine (adapted from BEI's harness):
- the LeviLauncher instance `1.26.51.01` at `%APPDATA%\levilauncher.exe\versions\1.26.51.01`;
- **real input** (`SendInput`);
- **window screenshots** (`PrintWindow`).

There is one game instance per machine, so `run.ps1` holds a machine-wide lock for the whole run:
`%APPDATA%\levilauncher.exe\bei-e2e.lock`, **the same file BEI's harness uses**, so runs of both
projects (and parallel worktrees) simply queue.

```powershell
xmake -y                                                   # build bin\BeetrootSkin first
pwsh -NoProfile -File tools/e2e/run.ps1 -Scenario smoke
pwsh -NoProfile -File tools/e2e/run.ps1 -Scenario smoke -KeepOpen   # leave the game open afterwards
```

`run.ps1` performs these steps:
1. Takes the lock.
2. Puts back leftovers of an interrupted run (`mods-disabled\*`, options set by `Set-GameOption`), then
   moves every other mod folder (e.g. `mods\BEI`) to `mods-disabled\` (always moved back in `finally`).
3. Deploys `bin\BeetrootSkin` to `mods\BeetrootSkin` (keeps `config\`).
4. Restores the `BeetrootSkin-Test` world from `fixtures\BeetrootSkin-Test.zip` (survival, normal
   difficulty, cheats on, always day, flat) and marks it as just played so it is the first world.
5. Runs the optional `scenarios\<name>.prelaunch.ps1`.
6. Launches the game through LeviLauncher and waits for `BeetrootSkin enabled`.
7. Enters the world and waits for `BeetrootSkin: joined level` plus a `Food state: ... source=integrated-server`
   line (proves the local server ticks; a half-joined world is retried).
8. Runs `scenarios\<name>.ps1`.
9. Checks the log for `ERR`/`FTL` from BeetrootSkin, LeviLamina or CrashLogger.
10. Always closes the game, runs `scenarios\<name>.restore.ps1`, restores `options.txt` and the
    other mods and releases the lock (each cleanup step runs even if an earlier one fails).

If a run was killed (so its `finally` never ran), the next run cleans up first; to do it right
away run `pwsh -NoProfile -File tools/e2e/restore-mods.ps1` (takes the lock, moves
`mods-disabled\*` back into `mods\`, restores `options.txt`). A mod that was deployed again in the
meantime wins; the older aside copy is kept as `mods-disabled\<name>.stale-<time>` (delete those by
hand). Other projects' aside folders (e.g. `mods.abp-e2e-aside`) are never touched.

**Look at the screenshots** in `tools\e2e\out\<scenario>\` (e.g. with the Read tool). They are the
evidence that a feature works.

## Writing a scenario

A scenario is a PowerShell file dot-sourced after the player is in the world, with `lib.ps1`
loaded. Start food scenarios with `Reset-Player`: the game can keep player data between runs.

| Helper | What it does |
| ------ | ------------ |
| `Reset-Player` | Survival, normal difficulty, empty inventory, no effects, full health and hunger |
| `Set-Survival` / `Set-Creative` / `Set-Difficulty normal` | Game mode / difficulty via chat |
| `Give-Item golden_apple 4` | `/give @s ...`; items fill the hotbar from slot 1 |
| `Select-Hotbar 2` | Presses the number key |
| `Clear-Inventory` / `Clear-Effects` | `/clear @s`, `/effect @s clear` |
| `Drop-Hunger 3` | Hunger effect (amplifier 255) for N seconds, then clears it: about 2.5 hunger points per second after saturation is gone |
| `Hurt-Player 6` | `/damage @s 6` |
| `Feed-Player [level] [seconds]` | Saturation effect |
| `Get-FoodStateLog` / `Wait-Hunger 6` | BeetrootSkin logs `Food state: hunger=.. saturation=.. exhaustion=.. health=../.. ... source=..` once after joining and on every change of hunger, saturation, health, source, peaceful or naturalRegen |
| `Open-Inventory` / `Close-Screen` | `E` / `Esc` with waits |
| `Invoke-Chat '/give @s oak_log 16'` | Opens chat, types, presses Enter. Cheats are on. |
| `Move-Mouse x y -Relative`, `Click x y -Relative [-Right]`, `Scroll n` | Coordinates are fractions of the 1600x900 client area |
| `Press-Key R` / `Press-Key ESC` / `Type-Text 'oak'` | Keyboard (`SHIFT` is a key name too) |
| `Save-Screenshot 'name' $u [-FromScreen]` | Saves to `out\$u\name.png`, downscaled to 1280x720 (PrintWindow; `-FromScreen` copies the visible desktop instead) |
| `Open-PauseMenu`, then `Open-Settings`, then `Open-KeyboardSettings`; `Close-Settings` | ESC from the HUD (checked on screen, retried: the game sometimes ignores the first ESC after joining), the pause menu's Settings button, Settings > Keyboard & Mouse (OreUI; PrintWindow captures them); `Close-Settings` = ESC back to the HUD |
| `Scroll-SettingsToEnd` / `Scroll-Notches 20` | Scrolls the right-hand settings pane to its end (until the picture stops changing) / by single wheel notches (40 px each at 1600x900; negative = down) |
| `Set-GameOption game_language zh_CN` | In a prelaunch script: edits `options.txt` (backed up once, restored by `run.ps1`) |
| `Wait-LogLine 'regex' [timeoutSeconds]` / `Wait-NewLogLine 'regex' $after` | Waits for a line in this session's log |
| `Get-LeviLog` | This session's log lines |

To find coordinates, take a screenshot. The screenshot is 1280x720, so a point `(px, py)` in it
corresponds to `-Relative (px/1280) (py/720)`.

Reference positions at 1600x900 (GUI scale 3, 1 GUI unit = 2.4 screenshot pixels):
- HUD hunger bar: GUI x 277..357, y 259..268 (10 icons, 8 apart, rightmost at x 349); hearts:
  x 176..256 at the same y;
- survival inventory, hotbar slot 1: `0.50 0.72`; each further slot `+0.034` in x.

`smoke.prelaunch.ps1` shows how to enable the debug drawing (`debug_draw` marker in the deployed
mod folder): outlines of every HUD icon rect, AppleSkin sprites from the mod-owned `icons.png`, and
a magenta area reserved in item tooltips, plus `Debug: held food ...` / `Debug: hovered ...` log
lines with the parsed food values.

## Notes and pitfalls

- **Elevated windows block injected input.** While a UAC prompt or a Windows Firewall
  "allow access" dialog (`PickerHost.exe ... FirewallNotificationDialogServer`, sometimes an
  invisible 0x0 window) is in the foreground, every click and key is silently dropped. Every input
  helper goes through `Assert-GameForeground`, which logs the window holding the foreground
  (`foreground is <process> (pid) '<title>' [<class>]`) and re-focuses the game several times with
  growing waits before it fails with "Minecraft is not the foreground window ... refusing to send
  input". It never clicks into the other window (`Focus-Game` skips its title-bar click when another
  window covers that point), so security dialogs must be closed by hand (Cancel firewall prompts).
  Other projects' servers on this machine can trigger them at any time.
- **A locked workstation** (lock screen `LockApp` in the foreground, or LogonUI with no foreground
  window, or a non-`Default` input desktop) delivers no input at all. `run.ps1` waits for an unlock
  before taking the lock and again before launching the game, and `Assert-GameForeground` waits it
  out mid-scenario without counting re-focus attempts; each wait logs `SESSION LOCKED: <why>` once
  and polls every 30 s (up to 4 hours).
- **Another project may hold the lock** (any harness using `bei-e2e.lock`); `run.ps1` waits up to
  two hours.
- **LeviLamina appends to `logs\latest.log` across sessions.** `Get-LeviLog` only returns lines
  written after this session's `Start-Game`. BeetrootSkin flushes every message.
- **The harness is DPI-aware and forces a 1600x900 client area**, so coordinates are stable.
- **Bedrock pauses when it loses focus.** `Focus-Game` only clicks the title bar if the game is
  not already in the foreground.
