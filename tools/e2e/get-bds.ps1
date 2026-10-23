<#
.SYNOPSIS
  Install the Bedrock Dedicated Server (Windows) for the remote e2e scenario.

.DESCRIPTION
  Two flavors, both outside the repository under %LOCALAPPDATA%\BeetrootSkin-bds (nothing is
  committed). The server must speak the client's network protocol: the client is 1.26.51.01.

  -Flavor levilamina (default; what `remote` uses unless $env:BEETROOT_BDS_FLAVOR = 'vanilla')
    `lip install github.com/LiteLDev/LeviLamina@<LeviLaminaVersion>` into
    BeetrootSkin-bds\levilamina-<LeviLaminaVersion>. lip pulls the matching bedrock-server build
    (github.com/LiteLDev/bds), LeviLamina, CrashLogger and PreLoader, and patches
    bedrock_server.exe into bedrock_server_mod.exe. No server mods are installed. This server
    accepts transport=raknet, a direct connection that needs no online service.

  -Flavor vanilla
    Fetches https://www.minecraft.net/bedrockdedicatedserver/bin-win/bedrock-server-<Version>.zip
    (Mojang's official download location; `-Latest` asks Mojang's download-links API for the
    current release) into BeetrootSkin-bds\<Version>. It only accepts NetherNet, which needs
    Minecraft's online signaling service (see tools/e2e/README.md).

  By installing a server you accept the Minecraft EULA and Privacy Policy
  (https://www.minecraft.net/eula, https://go.microsoft.com/fwlink/?LinkId=521839), like with the
  download button on minecraft.net.

.EXAMPLE
  pwsh -NoProfile -File tools/e2e/get-bds.ps1                                  # LeviLamina 26.51.7
  pwsh -NoProfile -File tools/e2e/get-bds.ps1 -Flavor vanilla                  # official 1.26.51.1
  pwsh -NoProfile -File tools/e2e/get-bds.ps1 -Flavor vanilla -Version 1.26.52.3
  pwsh -NoProfile -File tools/e2e/get-bds.ps1 -Flavor vanilla -Latest
#>
param(
    [ValidateSet('levilamina', 'vanilla')]
    [string]$Flavor = 'levilamina',
    [string]$LeviLaminaVersion = '26.51.7',
    [string]$Version = '1.26.51.1',
    [switch]$Latest,
    [string]$Root = (Join-Path $env:LOCALAPPDATA 'BeetrootSkin-bds'),
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $Root | Out-Null

if ($Flavor -eq 'levilamina') {
    $dir = Join-Path $Root "levilamina-$LeviLaminaVersion"
    if ((Test-Path (Join-Path $dir 'bedrock_server_mod.exe')) -and -not $Force) {
        Write-Host "[bds] BDS + LeviLamina $LeviLaminaVersion already present in $dir"
        return
    }
    if (-not (Get-Command lip -ErrorAction SilentlyContinue)) {
        throw "lip is not installed. Get it from https://github.com/futrime/lip/releases (or 'winget install futrime.lip')"
    }
    # Install into a temporary folder and rename it only after it succeeded, so an interrupted
    # install never leaves a half server behind.
    $tmp = "$dir.tmp-$PID"
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    New-Item -ItemType Directory $tmp | Out-Null
    Write-Host "[bds] installing github.com/LiteLDev/LeviLamina@$LeviLaminaVersion into $tmp (downloads ~250 MB)"
    Push-Location $tmp
    try {
        & lip install "github.com/LiteLDev/LeviLamina@$LeviLaminaVersion" 2>&1 | ForEach-Object { "$_" } |
            Where-Object { $_ -notmatch '^Downloading|^\S*https?://\S*: \d+%$' } | ForEach-Object { Write-Host "  | $_" }
        if ($LASTEXITCODE -ne 0) { throw "lip install failed (exit $LASTEXITCODE)" }
    } finally { Pop-Location }
    if (-not (Test-Path (Join-Path $tmp 'bedrock_server_mod.exe'))) { throw "lip finished but bedrock_server_mod.exe is missing in $tmp" }
    Remove-Item -Recurse -Force (Join-Path $tmp '.cache') -ErrorAction SilentlyContinue   # lip's download cache
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    Move-Item $tmp $dir
    Write-Host "[bds] BDS + LeviLamina $LeviLaminaVersion installed to $dir"
    return
}

if ($Latest) {
    $links = Invoke-RestMethod 'https://net-secondary.web.minecraft-services.net/api/v1.0/download/links'
    $url   = ($links.result.links | Where-Object downloadType -eq 'serverBedrockWindows').downloadUrl
    if ($url -notmatch 'bedrock-server-([\d.]+)\.zip$') { throw "Unexpected download link: $url" }
    $Version = $Matches[1]
} else {
    $url = "https://www.minecraft.net/bedrockdedicatedserver/bin-win/bedrock-server-$Version.zip"
}
if ($url -notmatch '^https://www\.minecraft\.net/bedrockdedicatedserver/') { throw "Refusing a non-official URL: $url" }

$dir = Join-Path $Root $Version
if ((Test-Path (Join-Path $dir 'bedrock_server.exe')) -and -not $Force) {
    Write-Host "[bds] $Version already present in $dir"
    return
}
if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
New-Item -ItemType Directory -Force $dir | Out-Null
$zip = Join-Path $Root "bedrock-server-$Version.zip"
Write-Host "[bds] downloading $url"
# minecraft.net rejects requests without a browser-like user agent.
Invoke-WebRequest $url -OutFile $zip -UserAgent 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'
Expand-Archive -Path $zip -DestinationPath $dir -Force
Remove-Item $zip -Force
if (-not (Test-Path (Join-Path $dir 'bedrock_server.exe'))) { throw "bedrock_server.exe missing after extracting $url" }
Write-Host "[bds] Bedrock Dedicated Server $Version extracted to $dir"
