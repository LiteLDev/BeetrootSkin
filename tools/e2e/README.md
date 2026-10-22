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
   difficulty, cheats on, always noon; mob spawning is switched off in its `level.dat`, since
   zombies spawn in the shade of its trees) and marks it as just played so it is the first world.
5. Runs the optional `scenarios\<name>.prelaunch.ps1`.
6. Launches the game through LeviLauncher and waits for `BeetrootSkin enabled`.
7. Enters the world (or runs `$script:EnterWorld` when the prelaunch script set it, e.g. `remote`
   joins a dedicated server, see below) and waits for `BeetrootSkin: joined level` plus a `Food state: ... source=integrated-server`
   line (proves the local server ticks; a half-joined world is retried).
8. Runs `scenarios\<name>.ps1`.
9. Checks the log for `ERR`/`FTL` from BeetrootSkin, LeviLamina or CrashLogger.
10. Always closes the game (and a dedicated server started by the scenario), runs
    `scenarios\<name>.restore.ps1`, restores `options.txt` and the other mods and releases the lock
    (each cleanup step runs even if an earlier one fails).

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

Test-only companion mods (listed in `$script:TestOnlyMods`, built from this repository into
`bin\<name>`) are deployed by a scenario's `.prelaunch.ps1` with `Deploy-ExtraMod <name>` and
removed by its `.restore.ps1` with `Remove-ExtraMod <name>`; a leftover from an interrupted run is
deleted by the next run. The `api` scenario uses `beetroot-api-sample`
(`xmake f --api_sample=y; xmake`).

## Remote server (`remote` scenario)

`remote` tests the multiplayer path (food state from the attributes, game rules and difficulty
the server syncs to the client: `source=client`, `exact=false`) against Mojang's official
**Bedrock Dedicated Server** (BDS) instead of the integrated server.

```powershell
pwsh -NoProfile -File tools/e2e/get-bds.ps1                 # once: BDS 1.26.51.1 -> %LOCALAPPDATA%\BeetrootSkin-bds\1.26.51.1
pwsh -NoProfile -File tools/e2e/run.ps1 -Scenario remote
```

- `get-bds.ps1 [-Version x.y.z.w | -Latest]` downloads
  `https://www.minecraft.net/bedrockdedicatedserver/bin-win/bedrock-server-<version>.zip` (Mojang's
  official link; `-Latest` asks Mojang's download-links API) and extracts it to
  `%LOCALAPPDATA%\BeetrootSkin-bds\<version>`, outside the repository. Downloading it means
  accepting the Minecraft EULA and privacy policy, as on minecraft.net. The server must speak the
  client's protocol: client 1.26.51.01 pairs with BDS **1.26.51.1** (still downloadable; 1.26.52.3 is
  the current release). From a packaged (MSIX) shell such as the Claude desktop app,
  `%LOCALAPPDATA%` is transparently redirected to the package's `LocalCache\Local`; that works the
  same, but the files are not where Explorer shows `%LOCALAPPDATA%`.
- `remote.prelaunch.ps1` runs `Start-Bds`: it writes `server.properties` (survival, normal,
  cheats, `level-name=BeetrootSkin-Remote`, `server-ip=127.0.0.1`, port 19132, no allow list, LAN
  visibility off, every joining player operator), copies the `BeetrootSkin-Test` fixture world in as
  the server's world (fresh every run, so the player starts fresh), starts `bedrock_server.exe` with
  its console on pipes (log: `out\remote\bds.log`) and waits for `Server started.`. It then sets
  `$script:EnterWorld = { Join-Bds }`, so run.ps1 joins the server instead of the local world.
- `Join-Bds` drives Play > Servers > "+ Add server" (name `BeetrootSkin e2e`, 127.0.0.1) > "Add and
  play" > "Proceed" on the "Third-party online play" caution, then waits for BDS's `Player
  Spawned: <name>` (kept in `$script:BdsPlayer`) and a `Food state: ... source=client` line. BDS
  1.26.51 only supports the NetherNet transport (`transport=raknet` is rejected with "TRANSPORT
  TYPE ERROR"), which uses Minecraft's online signaling service even for 127.0.0.1: the machine
  needs internet access, and the client sometimes fails with "Multiplayer Connection Failed ...
  NetherNet". `Join-Bds` then goes "Back to menu" (which returns to the Servers tab with the new
  entry selected) and presses its PLAY again (3 connects in all). When the client's signaling
  is down for longer (seen for an hour on 2026-10-22 while BDS reported "Signed in to signaling
  service successfully"; binding BDS to 0.0.0.0 did not help), the scenario cannot run. The
  `minecraft://connect/?serverUrl=...` deep link is **not usable**: on this machine it is handled
  by the Microsoft Store install of Minecraft (a different, unmodded version), not by the
  LeviLauncher instance. The added server-list entry is removed again by `Remove-BdsServerEntries`
  (prelaunch and restore, while the game is closed; it edits `minecraftpe\external_servers.txt`).
- Commands go to the **server console**, not the client chat: `Invoke-BdsCommand 'effect {player}
  hunger 2 255 true'` (`{player}` = the joined player's quoted name; returns the console lines it
  printed). `Get-BdsLog [after]` / `Wait-BdsLine 'regex' [after] [seconds]` read the console.
  Bedrock console notes: `effect <player> <effect> 0` removes one effect, `effect <player> clear`
  all of them; use `execute as <player> at @s run ...` for relative coordinates; "ERROR" console
  lines such as "no items to remove" are just command feedback.
- `Stop-Bds` sends `stop` (kills after 20 s) and also kills any `bedrock_server.exe` running from
  a `BeetrootSkin-bds` folder; run.ps1 calls it after closing the game and `remote.restore.ps1`
  calls it again, so no server is left behind. The server's console pipes belong to the run.ps1
  process, so it is stopped even with `-KeepOpen` (the game stays open, disconnected).
- **Firewall:** the server listens on 127.0.0.1, which needs no rule, but it also opens other UDP
  sockets (NetherNet signaling), so the first start of a new `bedrock_server.exe` path can still
  raise a Windows Firewall "allow access" dialog. Close it with **Cancel**; never allow it or change
  firewall rules. While the dialog is up, input to the game is blocked (see below).

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
