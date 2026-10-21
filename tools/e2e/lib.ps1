# BeetrootSkin end-to-end test helpers: deploy the mod, drive Minecraft Bedrock through LeviLauncher,
# send real input (SendInput) and capture screenshots.
#
# Dot-source this file:  . "$PSScriptRoot/lib.ps1"
# All coordinates passed to Click/Move are *client-area pixels* of the game window, or
# fractions (0..1) of the client area when -Relative is used.


$script:RepoRoot     = (Resolve-Path "$PSScriptRoot/../..").Path
$script:Launcher     = Join-Path $env:USERPROFILE 'Desktop\LeviLauncher.exe'
$script:LauncherData = Join-Path $env:APPDATA 'levilauncher.exe'
$script:VersionName  = '1.26.51.01'
$script:VersionDir   = Join-Path $script:LauncherData "versions\$script:VersionName"
$script:ModsDir      = Join-Path $script:VersionDir 'mods'
$script:GameExe      = Join-Path $script:VersionDir 'Minecraft.Windows.exe'
# Shared with BEI's harness on purpose: both projects drive the same game instance, so their
# runs queue on the same lock.
$script:LockFile     = Join-Path $script:LauncherData 'bei-e2e.lock'
$script:OutDir       = Join-Path $script:RepoRoot 'tools\e2e\out'
$script:ModName      = 'BeetrootSkin'
$script:WorldName    = 'BeetrootSkin-Test'
# Every other mod folder (BEI, other projects sharing this game instance) is moved to
# mods-disabled\ for the run and moved back by Restore-OtherMods, so their hooks and UI don't
# interfere. mods-disabled\ belongs to this harness: whatever is in it gets restored.
$script:KeepMods         = @('LeviLamina', $script:ModName)
$script:DisabledModsDir  = Join-Path $script:VersionDir 'mods-disabled'

if (-not ('BsE2E.Native' -as [type])) {
    Add-Type -AssemblyName System.Drawing
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace BsE2E {
public static class Native {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Explicit)] struct UNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)] struct INPUT { public uint type; public UNION u; }

    [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
    [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint flags);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", SetLastError = true)] static extern IntPtr OpenInputDesktop(uint flags, bool inherit, uint access);
    [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr h);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern bool GetUserObjectInformation(IntPtr h, int index, System.Text.StringBuilder info, int len, out int needed);
    // Name of the desktop that receives input: "Default" normally, "Winlogon" (or not openable at
    // all, then "") while the workstation is locked, at the sign-in screen or on a UAC prompt.
    public static string InputDesktopName() {
        IntPtr d = OpenInputDesktop(0, false, 0x0001 /* DESKTOP_READOBJECTS */);
        if (d == IntPtr.Zero) return "";
        try {
            var sb = new System.Text.StringBuilder(256); int needed;
            return GetUserObjectInformation(d, 2 /* UOI_NAME */, sb, sb.Capacity * 2, out needed) ? sb.ToString() : "";
        } finally { CloseDesktop(d); }
    }
    // "process (pid) 'title' [class]" of the foreground window, for diagnostics.
    public static string DescribeForeground() {
        IntPtr fg = GetForegroundWindow();
        if (fg == IntPtr.Zero) return "<no foreground window>";
        uint pid; GetWindowThreadProcessId(fg, out pid);
        var t = new System.Text.StringBuilder(256); GetWindowText(fg, t, 256);
        var c = new System.Text.StringBuilder(256); GetClassName(fg, c, 256);
        string name = "?";
        try { name = System.Diagnostics.Process.GetProcessById((int)pid).ProcessName; } catch {}
        return name + " (pid " + pid + ") '" + t + "' [" + c + "] hwnd=0x" + fg.ToString("X");
    }
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    public static void ForceForeground(IntPtr h) {
        IntPtr fg = GetForegroundWindow();
        uint fgThread = GetWindowThreadProcessId(fg, IntPtr.Zero);
        uint me = GetCurrentThreadId();
        AttachThreadInput(me, fgThread, true);
        keybd_event(0x12, 0, 0, IntPtr.Zero); keybd_event(0x12, 0, 2, IntPtr.Zero);
        ShowWindow(h, 9);
        BringWindowToTop(h);
        SetForegroundWindow(h);
        AttachThreadInput(me, fgThread, false);
    }

    const uint INPUT_MOUSE = 0, INPUT_KEYBOARD = 1;
    const uint KEYEVENTF_KEYUP = 2, KEYEVENTF_SCANCODE = 8, KEYEVENTF_UNICODE = 4, KEYEVENTF_EXTENDED = 1;
    const uint MOUSEEVENTF_MOVE = 1, MOUSEEVENTF_LEFTDOWN = 2, MOUSEEVENTF_LEFTUP = 4,
               MOUSEEVENTF_RIGHTDOWN = 8, MOUSEEVENTF_RIGHTUP = 16, MOUSEEVENTF_WHEEL = 0x800;

    static void Send(INPUT i) { SendInput(1, new[] { i }, Marshal.SizeOf(typeof(INPUT))); }

    public static void Key(ushort vk, bool up) {
        var i = new INPUT { type = INPUT_KEYBOARD };
        i.u.ki.wVk = vk;
        i.u.ki.wScan = (ushort)MapVirtualKey(vk, 0);
        i.u.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0);
        if (vk == 0x25 || vk == 0x26 || vk == 0x27 || vk == 0x28 || vk == 0x2E || vk == 0x24 || vk == 0x23) i.u.ki.dwFlags |= KEYEVENTF_EXTENDED;
        Send(i);
    }
    public static void Unicode(char c) {
        var down = new INPUT { type = INPUT_KEYBOARD }; down.u.ki.wScan = c; down.u.ki.dwFlags = KEYEVENTF_UNICODE;
        var up = new INPUT { type = INPUT_KEYBOARD }; up.u.ki.wScan = c; up.u.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        Send(down); Send(up);
    }
    public static void MouseButton(int button, bool up) {
        var i = new INPUT { type = INPUT_MOUSE };
        if (button == 0) i.u.mi.dwFlags = up ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
        else i.u.mi.dwFlags = up ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
        Send(i);
    }
    public static void Wheel(int delta) {
        var i = new INPUT { type = INPUT_MOUSE }; i.u.mi.dwFlags = MOUSEEVENTF_WHEEL; i.u.mi.mouseData = (uint)delta; Send(i);
    }
    public static void MoveRelative(int dx, int dy) {
        var i = new INPUT { type = INPUT_MOUSE }; i.u.mi.dwFlags = MOUSEEVENTF_MOVE; i.u.mi.dx = dx; i.u.mi.dy = dy; Send(i);
    }
}
}
'@
}

# Physical pixels everywhere (the desktop uses display scaling).
[BsE2E.Native]::SetProcessDpiAwarenessContext([IntPtr]::new(-4)) | Out-Null

function Write-Step([string]$msg) { Write-Host "[e2e] $msg" }

# ---------------------------------------------------------------------------------------
# Lock (the game instance is shared by every worktree on this machine)
# ---------------------------------------------------------------------------------------
function Enter-E2ELock([int]$TimeoutMinutes = 120) {
    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    while ($true) {
        try {
            $script:LockStream = [System.IO.File]::Open($script:LockFile, 'OpenOrCreate', 'ReadWrite', 'None')
            $bytes = [Text.Encoding]::UTF8.GetBytes("pid=$PID repo=$script:RepoRoot at=$(Get-Date -Format o)")
            $script:LockStream.SetLength(0); $script:LockStream.Write($bytes, 0, $bytes.Length); $script:LockStream.Flush()
            Write-Step "lock acquired"
            return
        } catch [System.IO.IOException] {
            if ((Get-Date) -gt $deadline) { throw "Timed out waiting for e2e lock $script:LockFile" }
            Start-Sleep -Seconds 15
        }
    }
}

function Exit-E2ELock {
    if ($script:LockStream) { $script:LockStream.Dispose(); $script:LockStream = $null; Write-Step "lock released" }
}

# ---------------------------------------------------------------------------------------
# Deploy / world fixture
# ---------------------------------------------------------------------------------------
function Get-GameDataDir {
    # LeviLauncher isolation: per-version data under the version folder.
    $iso = Join-Path $script:VersionDir 'Minecraft Bedrock'
    if (Test-Path $iso) { return $iso }
    return Join-Path $env:APPDATA 'Minecraft Bedrock'
}

function Get-WorldsDir {
    $data = Get-GameDataDir
    $dirs = @(Get-ChildItem (Join-Path $data 'Users') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName 'games\com.mojang\minecraftWorlds' } |
        Where-Object { Test-Path $_ })
    # Prefer a real user folder over "Shared".
    $user = $dirs | Where-Object { $_ -notmatch '\\Shared\\' } | Select-Object -First 1
    if ($user) { return $user }
    return $dirs | Select-Object -First 1
}

function Deploy-Mod([string]$BinDir = (Join-Path $script:RepoRoot "bin\$script:ModName")) {
    if (-not (Test-Path (Join-Path $BinDir "$script:ModName.dll"))) { throw "Build output not found: $BinDir (run xmake first)" }
    $dest = Join-Path $script:ModsDir $script:ModName
    New-Item -ItemType Directory -Force $dest | Out-Null
    # Keep the user's config/ and data/ directories; replace binaries, manifest, lang, textures.
    Get-ChildItem $dest -File | Remove-Item -Force
    foreach ($d in 'lang', 'textures') {
        if (Test-Path (Join-Path $dest $d)) { Remove-Item -Recurse -Force (Join-Path $dest $d) }
    }
    Copy-Item -Recurse -Force (Join-Path $BinDir '*') $dest
    Write-Step "deployed $BinDir -> $dest"
}

# Move a directory, retrying for a while: the game (or an antivirus scan) may still hold files
# open for a moment after it exits. Returns $true on success.
function Move-DirWithRetry([string]$Src, [string]$Dst, [int]$Attempts = 10) {
    for ($i = 0; $i -lt $Attempts; $i++) {
        try { Move-Item -LiteralPath $Src -Destination $Dst -ErrorAction Stop; return $true }
        catch { Start-Sleep -Seconds 2 }
    }
    return $false
}

# Move every mod except KeepMods (e.g. BEI) out of mods\ for the run. Always pair with
# Restore-OtherMods (run.ps1 does it in `finally`). Leftovers of an interrupted (killed) run are
# put back first, so mods-disabled\ only ever holds what this run moved.
function Disable-OtherMods {
    Restore-OtherMods | Out-Null
    New-Item -ItemType Directory -Force $script:DisabledModsDir | Out-Null
    foreach ($dir in @(Get-ChildItem $script:ModsDir -Directory)) {
        if ($script:KeepMods -contains $dir.Name) { continue }
        $dst = Join-Path $script:DisabledModsDir $dir.Name
        if (Test-Path $dst) {
            # Restore-OtherMods could not move it back (see its warnings): keep it aside.
            $stale = "$dst.stale-$(Get-Date -Format yyyyMMddHHmmss)"
            Move-Item -LiteralPath $dst $stale
            Write-Step "kept stale $dst as $stale"
        }
        if (-not (Move-DirWithRetry $dir.FullName $dst)) { throw "Could not move mod $($dir.FullName) to $dst" }
        Write-Step "disabled mod $($dir.Name) for this run"
    }
}

# Move everything Disable-OtherMods put aside back into mods\. Idempotent and safe to call with
# nothing to restore: run.ps1 calls it before a run (leftovers of a killed run) and in `finally`,
# tools\e2e\restore-mods.ps1 by hand. Returns $true when nothing is left aside.
function Restore-OtherMods {
    if (-not (Test-Path $script:DisabledModsDir)) { return $true }
    $ok = $true
    $dirs = @(Get-ChildItem $script:DisabledModsDir -Directory | Where-Object { $_.Name -notmatch '\.stale-\d+$' })
    foreach ($dir in $dirs) {
        $dst = Join-Path $script:ModsDir $dir.Name
        if (Test-Path $dst) {
            # The mod was deployed again meanwhile (by its own project or by hand): that copy is
            # newer. Keep ours under a name that is never restored, so nothing is lost.
            $stale = "$($dir.FullName).stale-$(Get-Date -Format yyyyMMddHHmmss)"
            Move-Item -LiteralPath $dir.FullName $stale
            Write-Step "warning: $dst was deployed again; kept the older copy as $stale"
            continue
        }
        if (Move-DirWithRetry $dir.FullName $dst) { Write-Step "restored mod $($dir.Name)" }
        else {
            $ok = $false
            Write-Host "[e2e] FAILED to restore $($dir.FullName) -> $dst; run tools\e2e\restore-mods.ps1 or move it back by hand" -ForegroundColor Red
        }
    }
    return $ok
}

# ---------------------------------------------------------------------------------------
# Game options (options.txt). A scenario that changes an option (e.g. game_language) calls
# Set-GameOption in its prelaunch; the original value is recorded in options.txt.bs-e2e-orig
# ("key:value" per line, or "key" alone when the key was absent) and put back by
# Restore-GameOptions (run.ps1 after the game exits, and before the next run if a run was killed).
# Only the keys the harness changed are touched, and only while they still hold the harness's
# value, so settings changed in between (e.g. by playing after a killed run) are never lost.
# ---------------------------------------------------------------------------------------
function Get-GameOptionsFile {
    $all = @(Get-ChildItem (Get-GameDataDir) -Recurse -Filter options.txt -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\minecraftpe\\options\.txt$' })
    # Like Get-WorldsDir: prefer a real user folder over "Shared".
    $user = $all | Where-Object { $_.FullName -notmatch '\\Users\\Shared\\' } | Select-Object -First 1
    if ($user) { return $user.FullName }
    return $all | Select-Object -First 1 -ExpandProperty FullName
}

function Get-OptionLine([string[]]$Lines, [string]$Key) {
    $Lines | Where-Object { $_ -match "^$([regex]::Escape($Key)):" } | Select-Object -First 1
}

function Set-OptionLine([string]$File, [string]$Key, $Line) {
    # $Line = $null removes the key.
    $found = $false
    $lines = @(Get-Content $File | ForEach-Object {
            if ($_ -match "^$([regex]::Escape($Key)):") { if (-not $found -and $null -ne $Line) { $Line }; $found = $true } else { $_ }
        })
    if (-not $found -and $null -ne $Line) { $lines += $Line }
    $lines | Set-Content $File -Encoding utf8NoBOM
}

function Set-GameOption([string]$Key, [string]$Value) {
    $f = Get-GameOptionsFile
    if (-not $f) { throw "options.txt not found under $(Get-GameDataDir)" }
    $orig = "$f.bs-e2e-orig"
    $recorded = @(if (Test-Path $orig) { Get-Content $orig })
    if (-not ($recorded | Where-Object { $_ -eq $Key -or $_ -match "^$([regex]::Escape($Key)):" })) {
        $cur = Get-OptionLine @(Get-Content $f) $Key
        Add-Content $orig $(if ($cur) { $cur } else { $Key }) -Encoding utf8NoBOM
    }
    Add-Content "$f.bs-e2e-set" "${Key}:$Value" -Encoding utf8NoBOM
    Set-OptionLine $f $Key "${Key}:$Value"
    Write-Step "options.txt: $Key = $Value"
}

function Restore-GameOptions {
    foreach ($o in @(Get-ChildItem (Get-GameDataDir) -Recurse -Filter 'options.txt.bs-e2e-orig' -ErrorAction SilentlyContinue)) {
        $f = $o.FullName -replace '\.bs-e2e-orig$', ''
        $setFile = "$f.bs-e2e-set"
        $set = @(if (Test-Path $setFile) { Get-Content $setFile })
        if (Test-Path $f) {
            foreach ($entry in @(Get-Content $o.FullName)) {
                $key = ($entry -split ':', 2)[0]
                $now = Get-OptionLine @(Get-Content $f) $key
                if ($now -and -not ($set -contains $now)) {
                    Write-Step "options.txt: $key was changed since the e2e run ($now); leaving it"
                    continue
                }
                Set-OptionLine $f $key $(if ($entry -eq $key) { $null } else { $entry })
                Write-Step "options.txt: restored $(if ($entry -eq $key) { "$key (removed)" } else { $entry })"
            }
        }
        Remove-Item -LiteralPath $o.FullName -Force
        if (Test-Path $setFile) { Remove-Item -LiteralPath $setFile -Force }
    }
}

# Little-endian NBT (Bedrock level.dat: 8-byte header, then the root compound) tag value
# offset, or -1. Good enough for unique top-level tag names.
function Find-NbtTag([System.Collections.Generic.List[byte]]$Bytes, [byte]$Type, [string]$Name) {
    $nb = [Text.Encoding]::UTF8.GetBytes($Name)
    $pat = [byte[]](@($Type, [byte]($nb.Length -band 0xFF), [byte]($nb.Length -shr 8)) + $nb)
    $arr = $Bytes.ToArray()
    for ($i = 8; $i -le $arr.Length - $pat.Length; $i++) {
        if ($arr[$i] -ne $pat[0]) { continue }
        $ok = $true
        for ($j = 1; $j -lt $pat.Length; $j++) { if ($arr[$i + $j] -ne $pat[$j]) { $ok = $false; break } }
        if ($ok) { return $i + $pat.Length }
    }
    return -1
}

# Mark the world as just played so it is the first entry of the world list.
function Set-WorldLastPlayed([string]$WorldDir) {
    foreach ($f in 'level.dat', 'level.dat_old') {
        $path = Join-Path $WorldDir $f
        if (-not (Test-Path $path)) { continue }
        $b = [System.Collections.Generic.List[byte]]::new([IO.File]::ReadAllBytes($path))
        $p = Find-NbtTag $b 4 'LastPlayed'
        if ($p -lt 0) { continue }
        $v = [BitConverter]::GetBytes([long][DateTimeOffset]::UtcNow.ToUnixTimeSeconds())
        for ($k = 0; $k -lt 8; $k++) { $b[$p + $k] = $v[$k] }
        [IO.File]::WriteAllBytes($path, $b.ToArray())
    }
}

function Restore-TestWorld {
    $zip = Join-Path $script:RepoRoot "tools\e2e\fixtures\$script:WorldName.zip"
    if (-not (Test-Path $zip)) { Write-Step "no world fixture, skipping restore"; return }
    $worlds = Get-WorldsDir
    if (-not $worlds) { Write-Step "worlds dir not found (game never started?), skipping restore"; return }
    # Remove every world with our name (any folder id), then extract a fresh copy.
    Get-ChildItem $worlds -Directory | Where-Object {
        (Get-Content (Join-Path $_.FullName 'levelname.txt') -ErrorAction SilentlyContinue) -eq $script:WorldName
    } | Remove-Item -Recurse -Force
    $target = Join-Path $worlds $script:WorldName
    Expand-Archive -Path $zip -DestinationPath $target -Force
    Set-WorldLastPlayed $target
    Write-Step "restored world fixture to $target"
}

# ---------------------------------------------------------------------------------------
# Game process / window
# ---------------------------------------------------------------------------------------
function Get-GameProcess { Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1 }

function Start-Game([int]$TimeoutSeconds = 180) {
    if (Get-GameProcess) { throw "Minecraft is already running; close it first" }
    # latest.log is appended across sessions: remember where this session starts.
    $script:LogOffset = @(Get-LeviLog -All).Count
    # A leftover launcher instance swallows --launch (single-instance hand-off) without starting
    # the game, so close any running launcher first.
    Get-Process -Name 'LeviLauncher' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    Write-Step "launching $script:VersionName via LeviLauncher"
    Start-Process -FilePath $script:Launcher -ArgumentList "--launch=`"$script:VersionName`""
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $p = Get-GameProcess
        if ($p) { $script:Game = $p; Write-Step "game window found (pid $($p.Id))"; return $p }
        Start-Sleep -Seconds 2
    }
    throw "Game window did not appear within $TimeoutSeconds s"
}

function Stop-Game {
    Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue | ForEach-Object {
        $_.CloseMainWindow() | Out-Null
        if (-not $_.WaitForExit(10000)) { $_.Kill() }
    }
    Get-Process -Name 'LeviLauncher' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Write-Step "game stopped"
}

function Get-GameWindow {
    $p = Get-GameProcess
    if (-not $p) { throw "Minecraft is not running" }
    return $p.MainWindowHandle
}

# Fixed window size so GUI scale and click coordinates are reproducible across runs.
$script:WindowW = 1600
$script:WindowH = 900

function Focus-Game {
    $h = Get-GameWindow
    if ([BsE2E.Native]::GetForegroundWindow() -eq $h) { return }
    # Keep the game above other windows so synthesized clicks land on it, then click its
    # title bar: a (synthesized) user click is allowed to change the foreground window even
    # when SetForegroundWindow is blocked by the focus-stealing rules.
    [BsE2E.Native]::SetWindowPos($h, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
    $wr = New-Object BsE2E.Native+RECT; [BsE2E.Native]::GetWindowRect($h, [ref]$wr) | Out-Null
    $c = Get-ClientRect
    if ($c.Y - $wr.Top -gt 4) {
        $pt = New-Object BsE2E.Native+POINT
        $pt.X = [int](($wr.Left + $wr.Right) / 2); $pt.Y = [int](($wr.Top + $c.Y) / 2)
        # Only click when the title bar point really is the game's (a topmost dialog, e.g. a
        # firewall prompt, may cover it: never click into other windows).
        $under = [BsE2E.Native]::WindowFromPoint($pt)
        if ($under -ne [IntPtr]::Zero -and [BsE2E.Native]::GetAncestor($under, 2) -eq $h) {
            [BsE2E.Native]::SetCursorPos($pt.X, $pt.Y) | Out-Null
            [BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 50; [BsE2E.Native]::MouseButton(0, $true)
            Start-Sleep -Milliseconds 300
        }
    }
    for ($i = 0; $i -lt 5; $i++) {
        [BsE2E.Native]::ForceForeground($h)
        Start-Sleep -Milliseconds 300
        if ([BsE2E.Native]::GetForegroundWindow() -eq $h) { return }
    }
    Write-Step "warning: could not bring the game window to the foreground"
}

function Set-GameWindowSize([int]$ClientW = $script:WindowW, [int]$ClientH = $script:WindowH) {
    $h = Get-GameWindow
    [BsE2E.Native]::ShowWindow($h, 9) | Out-Null  # restore (un-maximize)
    $wr = New-Object BsE2E.Native+RECT; [BsE2E.Native]::GetWindowRect($h, [ref]$wr) | Out-Null
    $c = Get-ClientRect
    $extraW = ($wr.Right - $wr.Left) - $c.W; $extraH = ($wr.Bottom - $wr.Top) - $c.H
    [BsE2E.Native]::SetWindowPos($h, [IntPtr]::Zero, 40, 40, $ClientW + $extraW, $ClientH + $extraH, 0x0040) | Out-Null
    Start-Sleep -Milliseconds 800
}

function Get-ClientRect {
    $h = Get-GameWindow
    $r = New-Object BsE2E.Native+RECT
    [BsE2E.Native]::GetClientRect($h, [ref]$r) | Out-Null
    $pt = New-Object BsE2E.Native+POINT
    [BsE2E.Native]::ClientToScreen($h, [ref]$pt) | Out-Null
    [pscustomobject]@{ X = $pt.X; Y = $pt.Y; W = $r.Right - $r.Left; H = $r.Bottom - $r.Top }
}

function ConvertTo-Screen([double]$x, [double]$y, [switch]$Relative) {
    $c = Get-ClientRect
    if ($Relative) { $x = $x * $c.W; $y = $y * $c.H }
    [pscustomobject]@{ X = [int]($c.X + $x); Y = [int]($c.Y + $y) }
}

# ---------------------------------------------------------------------------------------
# Input
# ---------------------------------------------------------------------------------------
# Safety: never send synthesized input unless Minecraft is the foreground window, so keys and
# clicks can never land in another application (e.g. a chat window the user is typing in).
#
# Another window can take the foreground at any time (a Windows Firewall or UAC prompt, a
# notification, another app). Re-focus the game a few times with short waits before giving up,
# and log which window holds the foreground. Never interact with that other window: security
# dialogs (UAC, firewall) must be answered by a human; the game is only re-focused.
#
# A locked workstation (or the sign-in / UAC secure desktop) is a distinct condition: no window
# can be focused and no input is delivered until a human unlocks it, so it is waited out
# (Wait-SessionUnlocked) instead of burning re-focus attempts.
function Test-SessionLocked {
    $desk = [BsE2E.Native]::InputDesktopName()
    # '' = the input desktop could not be opened (no access from this context): not a lock signal.
    if ($desk -and $desk -ne 'Default') { return "input desktop is '$desk' (locked, sign-in screen or UAC prompt)" }
    $fg = [BsE2E.Native]::DescribeForeground()
    if ($fg -match '^LockApp ') { return "the lock screen is in the foreground: $fg" }
    # LogonUI.exe can outlive the lock screen for a moment: only together with no foreground window.
    if ([BsE2E.Native]::GetForegroundWindow() -eq [IntPtr]::Zero -and (Get-Process -Name 'LogonUI' -ErrorAction SilentlyContinue)) {
        return "LogonUI.exe is running and no window has the foreground (workstation locked)"
    }
    return $null
}

# Wait (polling every $PollSeconds) until the session is unlocked; logs once. Returns $true if it
# had to wait. Throws after $TimeoutMinutes.
function Wait-SessionUnlocked([int]$TimeoutMinutes = 240, [int]$PollSeconds = 30) {
    $why = Test-SessionLocked
    if (-not $why) { return $false }
    Write-Host "[e2e] SESSION LOCKED: $why; waiting for someone to unlock this machine (checking every $PollSeconds s, up to $TimeoutMinutes min)" -ForegroundColor Yellow
    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    while (Test-SessionLocked) {
        if ((Get-Date) -gt $deadline) { throw "The Windows session stayed locked for $TimeoutMinutes minutes ($why)" }
        Start-Sleep -Seconds $PollSeconds
    }
    Write-Step "session unlocked; continuing"
    Start-Sleep -Seconds 3
    return $true
}

function Assert-GameForeground([int]$Attempts = 6) {
    $h = Get-GameWindow
    if ([BsE2E.Native]::GetForegroundWindow() -eq $h) { return }
    Wait-SessionUnlocked | Out-Null
    $holder = $null
    for ($i = 1; $i -le $Attempts; $i++) {
        # Locked meanwhile: wait it out without counting the attempt.
        if (Wait-SessionUnlocked) { $i = 1 }
        $now = [BsE2E.Native]::DescribeForeground()
        if ($now -ne $holder) { Write-Step "foreground is $now, not Minecraft; re-focusing (attempt $i/$Attempts)"; $holder = $now }
        Focus-Game
        if ([BsE2E.Native]::GetForegroundWindow() -eq $h) {
            Write-Step "Minecraft is the foreground window again"
            Start-Sleep -Milliseconds 300   # Bedrock resumes from its focus-loss pause
            return
        }
        Start-Sleep -Milliseconds (500 * $i)
    }
    throw "Minecraft is not the foreground window ($([BsE2E.Native]::DescribeForeground()) is, after $Attempts attempts); refusing to send input"
}

function Move-Mouse([double]$x, [double]$y, [switch]$Relative) {
    Assert-GameForeground
    $p = ConvertTo-Screen $x $y -Relative:$Relative
    [BsE2E.Native]::SetCursorPos($p.X, $p.Y) | Out-Null
    # A tiny relative move makes the game register the new position.
    [BsE2E.Native]::MoveRelative(1, 0); [BsE2E.Native]::MoveRelative(-1, 0)
    Start-Sleep -Milliseconds 120
}

function Click([double]$x, [double]$y, [switch]$Relative, [switch]$Right) {
    Move-Mouse $x $y -Relative:$Relative
    $b = if ($Right) { 1 } else { 0 }
    [BsE2E.Native]::MouseButton($b, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::MouseButton($b, $true)
    Start-Sleep -Milliseconds 250
}

function Scroll([int]$notches) { Assert-GameForeground; [BsE2E.Native]::Wheel(120 * $notches); Start-Sleep -Milliseconds 200 }

$script:VK = @{
    ESC = 0x1B; ENTER = 0x0D; BACK = 0x08; TAB = 0x09; SPACE = 0x20; SHIFT = 0x10; CTRL = 0x11
    LEFT = 0x25; UP = 0x26; RIGHT = 0x27; DOWN = 0x28; F1 = 0x70; F2 = 0x71; F3 = 0x72
}

function Press-Key([string]$key, [int]$holdMs = 60) {
    Assert-GameForeground
    $vk = if ($script:VK.ContainsKey($key.ToUpper())) { $script:VK[$key.ToUpper()] } else { [int][char]$key.ToUpper() }
    [BsE2E.Native]::Key([uint16]$vk, $false); Start-Sleep -Milliseconds $holdMs; [BsE2E.Native]::Key([uint16]$vk, $true)
    Start-Sleep -Milliseconds 200
}

function Type-Text([string]$text) {
    Assert-GameForeground
    $h = Get-GameWindow
    foreach ($c in $text.ToCharArray()) {
        # A window that pops up mid-text must not receive the rest of it.
        if ([BsE2E.Native]::GetForegroundWindow() -ne $h) { Assert-GameForeground }
        [BsE2E.Native]::Unicode($c); Start-Sleep -Milliseconds 40
    }
    Start-Sleep -Milliseconds 200
}

# ---------------------------------------------------------------------------------------
# Screenshots and logs
# ---------------------------------------------------------------------------------------
#
# The client area as a Bitmap (dispose it). PrintWindow captures the window itself, even when
# covered by other windows, including the OreUI screens (pause menu, settings). -FromScreen copies
# the visible desktop instead (needs the game to be the visible foreground window).
function Get-ClientBitmap([switch]$FromScreen) {
    $c = Get-ClientRect
    $h = Get-GameWindow
    $bmp = New-Object System.Drawing.Bitmap $c.W, $c.H
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        if ($FromScreen) {
            Assert-GameForeground
            $g.CopyFromScreen($c.X, $c.Y, 0, 0, (New-Object System.Drawing.Size $c.W, $c.H))
        } else {
            $wr = New-Object BsE2E.Native+RECT; [BsE2E.Native]::GetWindowRect($h, [ref]$wr) | Out-Null
            $full = New-Object System.Drawing.Bitmap ($wr.Right - $wr.Left), ($wr.Bottom - $wr.Top)
            $gf = [System.Drawing.Graphics]::FromImage($full)
            $hdc = $gf.GetHdc()
            [BsE2E.Native]::PrintWindow($h, $hdc, 2) | Out-Null   # PW_RENDERFULLCONTENT
            $gf.ReleaseHdc($hdc); $gf.Dispose()
            $g.DrawImage($full, (New-Object System.Drawing.Rectangle 0, 0, $c.W, $c.H), (New-Object System.Drawing.Rectangle ($c.X - $wr.Left), ($c.Y - $wr.Top), $c.W, $c.H), 'Pixel')
            $full.Dispose()
        }
    } finally { $g.Dispose() }
    return $bmp
}

function Save-Screenshot([string]$name, [string]$unit = 'scratch', [switch]$FromScreen) {
    $dir = Join-Path $script:OutDir $unit
    New-Item -ItemType Directory -Force $dir | Out-Null
    $bmp = Get-ClientBitmap -FromScreen:$FromScreen
    # Downscale for review (keeps files small; 1280 wide is plenty to read the UI).
    $scale = [Math]::Min(1.0, 1280.0 / $bmp.Width)
    $out = New-Object System.Drawing.Bitmap ([int]($bmp.Width * $scale)), ([int]($bmp.Height * $scale))
    $g2 = [System.Drawing.Graphics]::FromImage($out)
    $g2.InterpolationMode = 'HighQualityBicubic'
    $g2.DrawImage($bmp, 0, 0, $out.Width, $out.Height)
    $path = Join-Path $dir "$name.png"
    $out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g2.Dispose(); $bmp.Dispose(); $out.Dispose()
    Write-Step "screenshot $path"
    return $path
}

# Hash of the pixels of a region of the client area (fractions; default: all of it), to detect
# when the picture stops changing.
function Get-ClientHash([double]$X0 = 0, [double]$Y0 = 0, [double]$X1 = 1, [double]$Y1 = 1) {
    $bmp = Get-ClientBitmap
    try {
        $r = New-Object System.Drawing.Rectangle ([int]($X0 * $bmp.Width)), ([int]($Y0 * $bmp.Height)), ([int](($X1 - $X0) * $bmp.Width)), ([int](($Y1 - $Y0) * $bmp.Height))
        $crop = $bmp.Clone($r, $bmp.PixelFormat)
        $ms = New-Object System.IO.MemoryStream
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            $crop.Save($ms, [System.Drawing.Imaging.ImageFormat]::Bmp)
            return [Convert]::ToHexString($sha.ComputeHash($ms.ToArray()))
        } finally { $sha.Dispose(); $ms.Dispose(); $crop.Dispose() }
    } finally { $bmp.Dispose() }
}

# Colors "r,g,b" at points (fractions of the client area).
function Get-PixelColors([double[][]]$Points) {
    $bmp = Get-ClientBitmap
    try {
        foreach ($p in $Points) {
            $px = $bmp.GetPixel([int]($p[0] * $bmp.Width), [int]($p[1] * $bmp.Height))
            "$($px.R),$($px.G),$($px.B)"
        }
    } finally { $bmp.Dispose() }
}

# True when every point is the light, unsaturated gray of a vanilla button face (the pause menu
# buttons). Pure white (clouds, snow) and darker grays (stone) do not match.
function Test-LightGray([double[][]]$Points) {
    foreach ($c in @(Get-PixelColors $Points)) {
        $v = @($c -split ',' | ForEach-Object { [int]$_ })
        $max = ($v | Measure-Object -Maximum).Maximum; $min = ($v | Measure-Object -Minimum).Minimum
        if ($min -lt 180 -or $max -gt 228 -or ($max - $min) -gt 12) { return $false }
    }
    return $true
}

$script:LogOffset = 0
# Lines of LeviLamina's latest.log written by the current game session (-All: whole file).
function Get-LeviLog([switch]$All) {
    $f = Join-Path $script:VersionDir 'logs\latest.log'
    if (-not (Test-Path $f)) { return @() }
    # The game keeps the file open for writing: read with FileShare.ReadWrite.
    $fs = [System.IO.File]::Open($f, 'Open', 'Read', 'ReadWrite')
    try { $text = (New-Object System.IO.StreamReader($fs, [Text.Encoding]::UTF8)).ReadToEnd() } finally { $fs.Dispose() }
    $lines = @($text -split "`r?`n" | Where-Object { $_ -ne '' })
    if ($All) { return $lines }
    # LeviLamina rotates latest.log (e.g. at midnight); a shorter file means a fresh one, so
    # everything in it belongs to this session.
    if ($lines.Count -lt $script:LogOffset) { $script:LogOffset = 0 }
    if ($script:LogOffset -ge $lines.Count) { return @() }
    return $lines[$script:LogOffset..($lines.Count - 1)]
}

function Wait-LogLine([string]$pattern, [int]$TimeoutSeconds = 120) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $hit = Get-LeviLog | Select-String -Pattern $pattern | Select-Object -Last 1
        if ($hit) { return $hit.Line }
        if (-not (Get-GameProcess)) { throw "Game exited while waiting for log '$pattern'" }
        Start-Sleep -Seconds 2
    }
    throw "Timed out waiting for log line '$pattern'"
}


# Like Wait-LogLine, but only matches lines after the first $After lines of this session.
function Wait-NewLogLine([string]$pattern, [int]$After, [int]$TimeoutSeconds = 120) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $hit = @(Get-LeviLog) | Select-Object -Skip $After | Select-String -Pattern $pattern | Select-Object -Last 1
        if ($hit) { return $hit.Line }
        if (-not (Get-GameProcess)) { throw "Game exited while waiting for log '$pattern'" }
        Start-Sleep -Seconds 2
    }
    throw "Timed out waiting for log line '$pattern'"
}

function Assert-NoErrors {
    $bad = Get-LeviLog | Select-String -Pattern ' (ERR|FTL)\]\[(BeetrootSkin|LeviLamina|CrashLogger)\]'
    if ($bad) { $bad | ForEach-Object { Write-Host "  $($_.Line)" }; throw "Errors found in LeviLamina log" }
    Write-Step "no BeetrootSkin errors in log"
}

# ---------------------------------------------------------------------------------------
# Menu navigation (calibrated for a 1600x900 client area; coordinates are fractions)
# ---------------------------------------------------------------------------------------
function Wait-GameReady([int]$TimeoutSeconds = 180) {
    Set-GameWindowSize
    Wait-LogLine "$script:ModName enabled|Failed to load mod $script:ModName" $TimeoutSeconds | Out-Null
    if (Get-LeviLog | Select-String "Failed to load mod $script:ModName") { throw "LeviLamina failed to load $script:ModName (see log)" }
    # The title screen takes a while after mods are enabled.
    Start-Sleep -Seconds 15
    Focus-Game
    # First-launch sign-in nag: "Maybe later" (harmless click if absent: empty area).
    Click 0.781 0.775 -Relative
    Start-Sleep -Seconds 2
}

function Enter-TestWorld([int]$LoadSeconds = 30, [int]$Attempts = 3) {
    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        $before = @(Get-LeviLog).Count
        Focus-Game
        Click 0.498 0.474 -Relative     # Play
        Start-Sleep -Seconds 4
        Click 0.173 0.43 -Relative      # first world thumbnail (BeetrootSkin-Test, LastPlayed set by Restore-TestWorld)
        Start-Sleep -Seconds $LoadSeconds
        try {
            # The join log alone is not enough: the game sometimes joins and then times out its own
            # local server ("Kelp"). A food state read from the integrated server proves it ticks.
            Wait-NewLogLine "$($script:ModName): joined level" $before 60 | Out-Null
            Wait-NewLogLine "Food state: .*source=integrated-server" $before 30 | Out-Null
            Start-Sleep -Seconds 2
            Focus-Game
            Write-Step "in world"
            return
        } catch {
            if ($attempt -eq $Attempts) { throw }
            # The game sometimes fails to join its own local world ("Connection timed out",
            # error "Kelp"). Leave the error dialog and try again.
            Write-Step "world join failed (attempt $attempt): $_; retrying"
            Save-Screenshot "join-failed-$attempt" 'e2e' | Out-Null
            # A half-joined world times out after a while and then shows the error dialog.
            Start-Sleep -Seconds 25
            Focus-Game
            Click 0.24 0.68 -Relative   # "Back to menu" on the connection error dialog
            Start-Sleep -Seconds 5
        }
    }
}

# Open the survival inventory (E) and wait for it to render.
function Open-Inventory { Focus-Game; Press-Key E; Start-Sleep -Milliseconds 1200 }
function Close-Screen { Press-Key ESC; Start-Sleep -Milliseconds 800 }

# Pause menu and Settings > Keyboard & Mouse (OreUI screens; PrintWindow captures them). Positions
# are fractions of the 1600x900 client area and do not depend on the game language.
# Open-PauseMenu: call from the HUD (no screen open). Shortly after joining a world the game
# sometimes ignores that ESC (a run then went on clicking into the world), so the menu is checked
# on screen (its gray Resume / Add-ons / Save & Quit button faces) and ESC is retried.
$script:PauseMenuProbe = @(@(0.16, 0.357), @(0.16, 0.565), @(0.16, 0.667))
function Test-PauseMenu { Test-LightGray $script:PauseMenuProbe }
function Open-PauseMenu([int]$Attempts = 4) {
    for ($i = 1; $i -le $Attempts; $i++) {
        Focus-Game
        Press-Key ESC
        $deadline = (Get-Date).AddSeconds(4)
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 500
            if (Test-PauseMenu) { Start-Sleep -Milliseconds 700; return }
        }
        Write-Step "pause menu did not open (attempt $i/$Attempts; probe colors $((Get-PixelColors $script:PauseMenuProbe) -join ' '))"
        Save-Screenshot "pause-attempt-$i" 'e2e' | Out-Null
    }
    throw "The pause menu did not open after $Attempts ESC presses"
}
# Pause menu -> Settings.
function Open-Settings { Click 0.293 0.461 -Relative; Start-Sleep -Milliseconds 2500 }
# Settings -> Keyboard & Mouse (left column, "Controls" section).
function Open-KeyboardSettings { Click 0.109 0.556 -Relative; Start-Sleep -Milliseconds 2000 }
# Scroll the right-hand settings pane to its end. The pane scrolls smoothly and a large wheel
# delta only moves it about a page, so keep scrolling until the pane stops changing (three equal
# pictures in a row, so one dropped wheel event does not stop it early).
function Scroll-SettingsToEnd([int]$MaxSteps = 80) {
    Move-Mouse 0.66 0.6 -Relative
    $prev = $null; $same = 0
    for ($k = 0; $k -lt $MaxSteps; $k++) {
        Scroll -30
        Start-Sleep -Milliseconds 700
        $hash = Get-ClientHash 0.34 0.1 0.97 0.97   # the pane only
        if ($hash -eq $prev) { $same++ } else { $same = 0 }
        if ($same -ge 2) { Write-Step "settings pane at its end after $k wheel steps"; return }
        $prev = $hash
    }
    throw "The settings pane was still scrolling after $MaxSteps wheel steps"
}
# Scroll by single wheel notches, slowly enough for the smooth scrolling to finish each one
# (one notch = 40 px at 1600x900).
function Scroll-Notches([int]$Notches) {
    $dir = [Math]::Sign($Notches)
    for ($k = 0; $k -lt [Math]::Abs($Notches); $k++) { Scroll $dir; Start-Sleep -Milliseconds 350 }
    Start-Sleep -Milliseconds 1000
}
# Settings screen -> pause menu -> HUD.
function Close-Settings {
    Press-Key ESC; Start-Sleep -Milliseconds 1500
    for ($i = 0; $i -lt 3; $i++) {
        Press-Key ESC; Start-Sleep -Milliseconds 1500
        if (-not (Test-PauseMenu)) { return }
    }
    throw "The pause menu did not close"
}

# Run a chat command, e.g. Invoke-Chat '/give @s oak_log 16'
function Invoke-Chat([string]$text) {
    Focus-Game
    Press-Key T; Start-Sleep -Milliseconds 700
    Type-Text $text
    Press-Key ENTER; Start-Sleep -Milliseconds 800
}

# ---------------------------------------------------------------------------------------
# Food / survival helpers (all via chat commands; the fixture has cheats on)
# ---------------------------------------------------------------------------------------
# Survival mode (the fixture is survival already; scenarios call this to be explicit).
function Set-Survival { Invoke-Chat '/gamemode s' }
function Set-Creative { Invoke-Chat '/gamemode c' }
# 'peaceful' | 'easy' | 'normal' | 'hard'
function Set-Difficulty([string]$Level = 'normal') { Invoke-Chat "/difficulty $Level" }
# Give-Item golden_apple 4   (namespace optional)
function Give-Item([string]$Item, [int]$Count = 1) { Invoke-Chat "/give @s $Item $Count" }
function Clear-Inventory { Invoke-Chat '/clear @s' }
function Clear-Effects { Invoke-Chat '/effect @s clear' }
# Select hotbar slot 1..9 (number key).
function Select-Hotbar([ValidateRange(1, 9)][int]$Slot) { Focus-Game; Press-Key ([string]$Slot); Start-Sleep -Milliseconds 300 }
# Burn hunger quickly: Hunger effect with a huge amplifier for $Seconds, then clear it.
# Roughly 1 hunger point (half a shank) per 4 exhaustion; amplifier 255 drains several points
# per second, so 2-3 s already empties a good part of the bar. Saturation goes first.
function Drop-Hunger([int]$Seconds = 3) {
    Invoke-Chat "/effect @s hunger $Seconds 255 true"
    Start-Sleep -Seconds ($Seconds + 1)
    Clear-Effects
}
# Hurt the player by $Amount half-hearts (no knockback).
function Hurt-Player([int]$Amount = 6) { Invoke-Chat "/damage @s $Amount" }
# Set the hunger bar exactly: empty it with Drop-Hunger first, then feed with /effect saturation.
# Saturation effect level L restores L+1 hunger (and 2(L+1) saturation) per tick on Bedrock.
function Feed-Player([int]$Level = 0, [int]$Seconds = 1) { Invoke-Chat "/effect @s saturation $Seconds $Level true" }
# The food state BeetrootSkin logged after joining (hunger/saturation/exhaustion/source).
function Get-FoodStateLog { Get-LeviLog | Select-String 'Food state: ' | Select-Object -Last 1 | ForEach-Object { $_.Line } }
# Wait until BeetrootSkin logs a food state whose hunger matches (it logs on every change of
# hunger or saturation). Returns the line.
function Wait-Hunger([int]$Hunger, [int]$TimeoutSeconds = 20) { Wait-LogLine "Food state: hunger=$Hunger " $TimeoutSeconds }
# Bring the player to a known state: survival, normal difficulty, empty inventory, no effects,
# full health and a full hunger bar. The game may keep player data across fixture restores,
# so every scenario that cares about food values should start with this.
function Reset-Player {
    Set-Survival
    Set-Difficulty normal
    Clear-Inventory
    Clear-Effects
    Invoke-Chat '/effect @s instant_health 1 10 true'
    Invoke-Chat '/effect @s saturation 1 10 true'
    Start-Sleep -Seconds 2
    Clear-Effects
}
