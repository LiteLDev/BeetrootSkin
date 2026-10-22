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
# Test-only companion mods built from this repository (bin\<name>, e.g. beetroot-api-sample from
# `xmake f --api_sample=y`). Scenarios deploy them with Deploy-ExtraMod in their prelaunch script
# and remove them with Remove-ExtraMod in their restore script; Disable-OtherMods deletes a
# leftover of an interrupted run instead of parking it in mods-disabled\.
$script:TestOnlyMods     = @('beetroot-api-sample')

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
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
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

# Move every mod except KeepMods (e.g. BEI) out of mods\ for the run. Always pair with
# Restore-OtherMods (run.ps1 does it in `finally`).
function Disable-OtherMods {
    New-Item -ItemType Directory -Force $script:DisabledModsDir | Out-Null
    foreach ($dir in @(Get-ChildItem $script:ModsDir -Directory)) {
        if ($script:KeepMods -contains $dir.Name) { continue }
        if ($script:TestOnlyMods -contains $dir.Name) {
            Remove-Item -Recurse -Force $dir.FullName
            Write-Step "removed leftover test-only mod $($dir.Name)"
            continue
        }
        $dst = Join-Path $script:DisabledModsDir $dir.Name
        if (Test-Path $dst) {
            # Leftover from an interrupted run while the mod was redeployed: keep it aside.
            $stale = "$dst.stale-$(Get-Date -Format yyyyMMddHHmmss)"
            Move-Item $dst $stale
            Write-Step "kept stale $dst as $stale"
        }
        Move-Item $dir.FullName $dst
        Write-Step "disabled mod $($dir.Name) for this run"
    }
}

# Deploy a test-only companion mod (bin\<Name>, see $script:TestOnlyMods) to mods\<Name>. Call it
# from a scenario's .prelaunch.ps1 (after Disable-OtherMods) and pair it with Remove-ExtraMod in
# the scenario's .restore.ps1.
function Deploy-ExtraMod([Parameter(Mandatory)][string]$Name) {
    if ($script:TestOnlyMods -notcontains $Name) { throw "$Name is not listed in `$script:TestOnlyMods" }
    $bin = Join-Path $script:RepoRoot "bin\$Name"
    if (-not (Test-Path (Join-Path $bin 'manifest.json'))) {
        throw "Build output not found: $bin (xmake f --api_sample=y, then xmake)"
    }
    $dest = Join-Path $script:ModsDir $Name
    if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
    Copy-Item -Recurse -Force $bin $dest
    Write-Step "deployed test-only mod $bin -> $dest"
}

# Remove a test-only companion mod deployed by Deploy-ExtraMod (retries while the game still
# holds the DLL open for a moment after closing).
function Remove-ExtraMod([Parameter(Mandatory)][string]$Name) {
    if ($script:TestOnlyMods -notcontains $Name) { throw "$Name is not listed in `$script:TestOnlyMods" }
    $dest = Join-Path $script:ModsDir $Name
    for ($i = 0; $i -lt 10 -and (Test-Path $dest); $i++) {
        try { Remove-Item -Recurse -Force $dest -ErrorAction Stop } catch { Start-Sleep -Seconds 2 }
    }
    if (Test-Path $dest) { Write-Host "[e2e] FAILED to remove $dest; delete it by hand" -ForegroundColor Red }
    else { Write-Step "removed test-only mod $Name" }
}

# Move everything Disable-OtherMods put aside back into mods\.
function Restore-OtherMods {
    if (-not (Test-Path $script:DisabledModsDir)) { return }
    $dirs = @(Get-ChildItem $script:DisabledModsDir -Directory | Where-Object { $_.Name -notmatch '\.stale-\d+$' })
    foreach ($dir in $dirs) {
        $dst = Join-Path $script:ModsDir $dir.Name
        if (Test-Path $dst) { Write-Step "warning: $dst exists again; leaving $($dir.FullName) in place"; continue }
        for ($i = 0; $i -lt 10; $i++) {
            try { Move-Item $dir.FullName $dst -ErrorAction Stop; Write-Step "restored mod $($dir.Name)"; break }
            catch { Start-Sleep -Seconds 2 } # the game may still hold files open for a moment
        }
        if (-not (Test-Path $dst)) { Write-Host "[e2e] FAILED to restore $($dir.FullName) -> $dst; move it back by hand" -ForegroundColor Red }
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
        [BsE2E.Native]::SetCursorPos([int](($wr.Left + $wr.Right) / 2), [int](($wr.Top + $c.Y) / 2)) | Out-Null
        [BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 50; [BsE2E.Native]::MouseButton(0, $true)
        Start-Sleep -Milliseconds 300
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
function Assert-GameForeground {
    $h = Get-GameWindow
    if ([BsE2E.Native]::GetForegroundWindow() -ne $h) {
        Focus-Game
        if ([BsE2E.Native]::GetForegroundWindow() -ne $h) { throw "Minecraft is not the foreground window; refusing to send input" }
    }
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
    foreach ($c in $text.ToCharArray()) { [BsE2E.Native]::Unicode($c); Start-Sleep -Milliseconds 40 }
    Start-Sleep -Milliseconds 200
}

# ---------------------------------------------------------------------------------------
# Screenshots and logs
# ---------------------------------------------------------------------------------------
# The game's client area as a bitmap (the caller disposes it). Captures the window itself with
# PrintWindow (works even when covered by other windows), then crops to the client area.
function Get-GameCapture {
    $c = Get-ClientRect
    $h = Get-GameWindow
    $wr = New-Object BsE2E.Native+RECT; [BsE2E.Native]::GetWindowRect($h, [ref]$wr) | Out-Null
    $full = New-Object System.Drawing.Bitmap ($wr.Right - $wr.Left), ($wr.Bottom - $wr.Top)
    try {
        $gf = [System.Drawing.Graphics]::FromImage($full)
        try {
            $hdc = $gf.GetHdc()
            [BsE2E.Native]::PrintWindow($h, $hdc, 2) | Out-Null   # PW_RENDERFULLCONTENT
            $gf.ReleaseHdc($hdc)
        } finally { $gf.Dispose() }
        $bmp = New-Object System.Drawing.Bitmap $c.W, $c.H
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        try {
            $g.DrawImage($full, (New-Object System.Drawing.Rectangle 0, 0, $c.W, $c.H), (New-Object System.Drawing.Rectangle ($c.X - $wr.Left), ($c.Y - $wr.Top), $c.W, $c.H), 'Pixel')
        } finally { $g.Dispose() }
        return $bmp
    } finally { $full.Dispose() }
}

function Save-Screenshot([string]$name, [string]$unit = 'scratch') {
    $dir = Join-Path $script:OutDir $unit
    New-Item -ItemType Directory -Force $dir | Out-Null
    $bmp = Get-GameCapture
    try {
        # Downscale for review (keeps files small; 1280 wide is plenty to read the UI).
        $scale = [Math]::Min(1.0, 1280.0 / $bmp.Width)
        $out = New-Object System.Drawing.Bitmap ([int]($bmp.Width * $scale)), ([int]($bmp.Height * $scale))
        $g2 = [System.Drawing.Graphics]::FromImage($out)
        $g2.InterpolationMode = 'HighQualityBicubic'
        $g2.DrawImage($bmp, 0, 0, $out.Width, $out.Height)
        $path = Join-Path $dir "$name.png"
        $out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $g2.Dispose(); $out.Dispose()
    } finally { $bmp.Dispose() }
    Write-Step "screenshot $path"
    return $path
}

# Colour of the game's client-area pixel at fractions ($x, $y) of the client area, e.g. to tell
# whether a dialog button is showing.
function Get-GamePixel([double]$x, [double]$y) {
    $bmp = Get-GameCapture
    try { return $bmp.GetPixel([int]($x * ($bmp.Width - 1)), [int]($y * ($bmp.Height - 1))) } finally { $bmp.Dispose() }
}

# Vanilla's green action button (e.g. "Add and play", "Proceed"): strongly green, dark red/blue.
function Test-GreenButton([double]$x, [double]$y) {
    $p = Get-GamePixel $x $y
    return $p.G -gt 100 -and $p.G -gt $p.R + 40 -and $p.G -gt $p.B + 40
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
    $bad = Get-LeviLog | Select-String -Pattern ' (ERR|FTL)\]\[(BeetrootSkin|LeviLamina|CrashLogger|beetroot-api-sample)\]'
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

# ---------------------------------------------------------------------------------------
# Remote server: the official Bedrock Dedicated Server (BDS), fetched by get-bds.ps1
# ---------------------------------------------------------------------------------------
# BDS lives outside the repository. 1.26.51.1 is the server build of the client's 1.26.51.01.
$script:BdsRoot      = Join-Path $env:LOCALAPPDATA 'BeetrootSkin-bds'
$script:BdsVersion   = '1.26.51.1'
$script:BdsPort      = 19132
# BDS 1.26.51 accepts only NetherNet ("transport=raknet" logs "TRANSPORT TYPE ERROR ... NetherNet
# is the only supported transport type"), which needs Minecraft's online signaling service on both
# sides even for 127.0.0.1, so the client can fail with "Multiplayer Connection Failed".
$script:BdsTransport = 'nethernet'
$script:BdsLevelName = 'BeetrootSkin-Remote'
$script:BdsServerName = 'BeetrootSkin e2e'   # entry Join-Bds adds to the client's server list
$script:Bds          = $null   # BsE2E.BdsProcess while a server runs
$script:BdsPlayer    = $null   # the client's player name on the server (from "Player Spawned")
# run.ps1 dot-sources this instead of calling Enter-TestWorld when a prelaunch script sets it,
# e.g. $script:EnterWorld = { Join-Bds }.
$script:EnterWorld   = $null

if (-not ('BsE2E.BdsProcess' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
namespace BsE2E {
// bedrock_server.exe with its console on pipes: stdout is copied to a log file and kept in
// memory (read with Lines()), commands go to stdin. The pump runs on .NET threads, so it keeps
// working while the PowerShell script sleeps.
public sealed class BdsProcess {
    readonly Process p;
    readonly StreamWriter log;
    readonly List<string> lines = new List<string>();
    BdsProcess(Process p, StreamWriter log) { this.p = p; this.log = log; }
    public static BdsProcess Start(string exe, string logPath) {
        var psi = new ProcessStartInfo(exe) {
            WorkingDirectory = Path.GetDirectoryName(exe), UseShellExecute = false, CreateNoWindow = true,
            RedirectStandardInput = true, RedirectStandardOutput = true, RedirectStandardError = true,
        };
        var log = new StreamWriter(logPath, false) { AutoFlush = true };
        var p = new Process { StartInfo = psi };
        var b = new BdsProcess(p, log);
        p.OutputDataReceived += (s, e) => b.Add(e.Data);
        p.ErrorDataReceived += (s, e) => b.Add(e.Data);
        p.Start();
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        return b;
    }
    void Add(string line) {
        if (line == null) return;
        lock (lines) { lines.Add(line); try { log.WriteLine(line); } catch (ObjectDisposedException) { } }
    }
    public string[] Lines() { lock (lines) return lines.ToArray(); }
    public int Count { get { lock (lines) return lines.Count; } }
    public int Id { get { return p.Id; } }
    public bool HasExited { get { return p.HasExited; } }
    public void Send(string command) {
        lock (lines) { try { log.WriteLine("> " + command); } catch (ObjectDisposedException) { } }
        p.StandardInput.WriteLine(command);
        p.StandardInput.Flush();
    }
    public void Stop(int timeoutMs) {
        try { if (!p.HasExited) Send("stop"); } catch { }
        if (!p.WaitForExit(timeoutMs)) { try { p.Kill(true); } catch { } p.WaitForExit(5000); }
        lock (lines) log.Dispose();
    }
}
}
'@
}

function Get-BdsDir([string]$Version = $script:BdsVersion) { Join-Path $script:BdsRoot $Version }

# Rewrite key=value lines of server.properties (keys that are missing are appended).
function Set-BdsProperties([hashtable]$Properties, [string]$Version = $script:BdsVersion) {
    $file  = Join-Path (Get-BdsDir $Version) 'server.properties'
    $lines = [System.Collections.Generic.List[string]]::new([string[]](Get-Content $file))
    foreach ($key in $Properties.Keys) {
        $re = '^#?\s*' + [regex]::Escape($key) + '='
        $i  = -1
        for ($k = 0; $k -lt $lines.Count; $k++) { if ($lines[$k] -match $re) { $i = $k; break } }
        if ($i -ge 0) { $lines[$i] = "$key=$($Properties[$key])" } else { $lines.Add("$key=$($Properties[$key])") }
    }
    Set-Content $file $lines -Encoding utf8
}

# Start BDS with a fresh copy of the BeetrootSkin-Test fixture world (flat, always day) as
# $BdsLevelName: survival, normal, cheats, every joining player operator, listening on
# 127.0.0.1 only (loopback needs no firewall rule). The console log goes to out\<unit>\bds.log.
function Start-Bds([string]$Unit = 'bds', [string]$Version = $script:BdsVersion, [int]$TimeoutSeconds = 120) {
    Stop-Bds
    $dir = Get-BdsDir $Version
    $exe = Join-Path $dir 'bedrock_server.exe'
    if (-not (Test-Path $exe)) { throw "BDS $Version not found in $dir; run tools/e2e/get-bds.ps1 -Version $Version" }
    Set-BdsProperties @{
        'server-name' = $script:BdsServerName; 'gamemode' = 'survival'; 'difficulty' = 'normal'; 'allow-cheats' = 'true'
        'online-mode' = 'true'; 'allow-list' = 'false'; 'level-name' = $script:BdsLevelName
        'server-port' = $script:BdsPort; 'server-ip' = '127.0.0.1'; 'enable-lan-visibility' = 'false'
        'transport' = $script:BdsTransport
        'default-player-permission-level' = 'operator'; 'player-idle-timeout' = '0'
    } $Version
    $world = Join-Path $dir "worlds\$script:BdsLevelName"
    if (Test-Path $world) { Remove-Item -Recurse -Force $world }
    $zip = Join-Path $script:RepoRoot "tools\e2e\fixtures\$script:WorldName.zip"
    if (Test-Path $zip) {
        Expand-Archive -Path $zip -DestinationPath $world -Force
        Set-Content (Join-Path $world 'levelname.txt') $script:BdsLevelName -NoNewline
    }
    $logDir = Join-Path $script:OutDir $Unit
    New-Item -ItemType Directory -Force $logDir | Out-Null
    $script:Bds = [BsE2E.BdsProcess]::Start($exe, (Join-Path $logDir 'bds.log'))
    Write-Step "BDS $Version starting (pid $($script:Bds.Id), 127.0.0.1:$script:BdsPort)"
    Wait-BdsLine 'Server started\.' 0 $TimeoutSeconds | Out-Null
    Write-Step "BDS started"
}

# Stop the server started by Start-Bds ("stop" on the console, killed after 20 s), and any other
# bedrock_server.exe running from $BdsRoot (leftover of an interrupted run).
function Stop-Bds {
    if ($script:Bds) {
        $script:Bds.Stop(20000)
        $script:Bds = $null
        Write-Step "BDS stopped"
    }
    # Match on the folder name, not the full path: a packaged (MSIX) shell sees %LOCALAPPDATA%
    # redirected to its package's LocalCache\Local, so the process path differs from $BdsRoot.
    $leaf = '\' + (Split-Path $script:BdsRoot -Leaf) + '\'
    Get-Process -Name 'bedrock_server' -ErrorAction SilentlyContinue | Where-Object {
        $_.Path -and $_.Path.IndexOf($leaf, [StringComparison]::OrdinalIgnoreCase) -ge 0
    } | ForEach-Object { Write-Step "killing leftover BDS pid $($_.Id)"; $_.Kill($true); $_.WaitForExit(5000) | Out-Null }
}

# Join-Bds adds a server-list entry every run ("Add and play"); remove ours from the client's
# minecraftpe\external_servers.txt (lines "<n>:<name>:<address>:<port>:<time>") while the game
# is closed, so the list does not grow. Other entries are kept and renumbered.
function Remove-BdsServerEntries {
    if (Get-GameProcess) { Write-Step "game running: server list left as is"; return }
    $data = Get-GameDataDir
    foreach ($f in @(Get-ChildItem (Join-Path $data 'Users') -Recurse -Filter external_servers.txt -ErrorAction SilentlyContinue)) {
        $lines = @(Get-Content $f.FullName | Where-Object { $_ -ne '' })
        $keep  = @($lines | Where-Object { ($_ -split ':', 3)[1] -ne $script:BdsServerName })
        if ($keep.Count -eq $lines.Count) { continue }
        $n = 0
        $renumbered = @($keep | ForEach-Object { $n++; "${n}:" +($_ -split ':', 2)[1] })
        Set-Content $f.FullName $renumbered -Encoding utf8
        Write-Step "removed $($lines.Count - $keep.Count) '$script:BdsServerName' entries from $($f.FullName)"
    }
}

# BDS console lines (all, or after the first $After).
function Get-BdsLog([int]$After = 0) {
    if (-not $script:Bds) { return @() }
    @($script:Bds.Lines()) | Select-Object -Skip $After
}

function Wait-BdsLine([string]$pattern, [int]$After = 0, [int]$TimeoutSeconds = 30) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $hit = Get-BdsLog $After | Select-String -Pattern $pattern | Select-Object -Last 1
        if ($hit) { return $hit.Line }
        if (-not $script:Bds -or $script:Bds.HasExited) { throw "BDS is not running (waiting for '$pattern')" }
        Start-Sleep -Milliseconds 500
    }
    throw "Timed out waiting for BDS line '$pattern'"
}

# Run a command on the BDS console (no leading slash), e.g. Invoke-BdsCommand 'difficulty peaceful'.
# {player} is replaced by the joined player's quoted name. Returns the console lines it printed.
function Invoke-BdsCommand([string]$Command, [int]$WaitMs = 700) {
    if (-not $script:Bds -or $script:Bds.HasExited) { throw "BDS is not running" }
    if ($Command.Contains('{player}')) {
        if (-not $script:BdsPlayer) { throw "No player has joined the server yet" }
        $Command = $Command.Replace('{player}', "`"$script:BdsPlayer`"")
    }
    $at = $script:Bds.Count
    $script:Bds.Send($Command)
    Start-Sleep -Milliseconds $WaitMs
    $out = @(Get-BdsLog $at)
    Write-Step "bds> $Command$(if ($out) { ' -> ' + ($out -join ' | ') })"
    return $out
}

# From the title screen: Play -> Servers -> Add Server (127.0.0.1:$BdsPort) -> Play, then wait
# until BDS reports the player and BeetrootSkin logs a food state from the client attributes.
# Further attempts cover transient NetherNet signaling failures of the client.
function Join-Bds([int]$Attempts = 3) {
    if (-not $script:Bds) { throw "Start-Bds first" }
    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        $before = @(Get-LeviLog).Count
        $bdsAt  = $script:Bds.Count
        Focus-Game
        Click 0.498 0.474 -Relative      # Play
        Start-Sleep -Seconds 4
        Click 0.832 0.129 -Relative      # Servers tab (the right pane then shows a featured server: never click its Play)
        Start-Sleep -Seconds 4
        Click 0.176 0.253 -Relative      # "+ Add server"
        Start-Sleep -Seconds 3
        Click 0.5 0.199 -Relative        # Server name
        Type-Text $script:BdsServerName
        Click 0.5 0.365 -Relative        # Server address
        Type-Text '127.0.0.1'
        if ($script:BdsPort -ne 19132) { # the port field is prefilled with 19132
            Click 0.5 0.532 -Relative
            for ($i = 0; $i -lt 6; $i++) { Press-Key BACK 30 }
            Type-Text "$script:BdsPort"
        }
        Save-Screenshot "add-server-$attempt" $script:Unit | Out-Null
        # "Add and play", then "Proceed" on the "Caution: Third-party online play" dialog: both
        # are green buttons at about the same spot. Press whichever shows until the server sees
        # the player; these screens drop a click unless the pointer rested on the button first.
        # The colour is sampled left and right of the labels (x 0.65..0.74), where both buttons
        # (x 0.51..0.86, y 0.64..0.69) are plain green.
        $deadline = (Get-Date).AddSeconds(60)
        $n = 0
        while ((Get-Date) -lt $deadline -and -not (Get-BdsLog $bdsAt | Select-String 'Player connected: ')) {
            if ((Test-GreenButton 0.56 0.67) -and (Test-GreenButton 0.80 0.67)) {
                $n++
                Move-Mouse 0.56 0.665 -Relative; Start-Sleep -Milliseconds 600
                Click 0.57 0.67 -Relative
                Write-Step "pressed the green button ($n)"
                Start-Sleep -Seconds 2
                Save-Screenshot "join-$attempt-$n" $script:Unit | Out-Null
            }
            Start-Sleep -Milliseconds 700
        }
        try {
            $line = Wait-BdsLine 'Player Spawned: ' $bdsAt 90
            if ($line -match 'Player Spawned: (.+?) xuid') { $script:BdsPlayer = $Matches[1].Trim() }
            Wait-NewLogLine "$($script:ModName): joined level" $before 30 | Out-Null
            Wait-NewLogLine 'Food state: .*source=client' $before 30 | Out-Null
            Start-Sleep -Seconds 2
            Focus-Game
            Write-Step "joined BDS as '$script:BdsPlayer'"
            return
        } catch {
            Save-Screenshot "join-failed-$attempt" $script:Unit | Out-Null
            # Only a connection that never reached the server is retried: from the add-server
            # form, the caution dialog or the Servers tab, ESC leads back to the title screen. A
            # player that is in the world (or half-joined) is a real failure.
            if ($attempt -eq $Attempts -or (Get-BdsLog $bdsAt | Select-String 'Player connected: ')) { throw }
            Write-Step "server join failed (attempt $attempt): $_; retrying"
            # The client's NetherNet transport sometimes fails to reach Minecraft's signaling
            # service ("Multiplayer Connection Failed ... NetherNet"); its "Back to menu" leads to
            # the title screen. Elsewhere (form, caution dialog) ESC twice does.
            Click 0.24 0.713 -Relative   # "Back to menu" (an empty spot on the other screens)
            # Wait for the title screen's green Play button. No ESC here: on the title screen it
            # opens "Do you want to exit Minecraft?".
            $deadline = (Get-Date).AddSeconds(30)
            while ((Get-Date) -lt $deadline -and -not (Test-GreenButton 0.40 0.474)) { Start-Sleep -Seconds 1 }
            if (-not (Test-GreenButton 0.40 0.474)) { throw "not back on the title screen after a failed join: $_" }
            Start-Sleep -Seconds 2
        }
    }
}
