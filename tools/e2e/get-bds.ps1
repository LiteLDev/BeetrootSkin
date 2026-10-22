<#
.SYNOPSIS
  Download and extract the official Bedrock Dedicated Server (Windows) for the remote e2e scenario.

.DESCRIPTION
  Fetches https://www.minecraft.net/bedrockdedicatedserver/bin-win/bedrock-server-<Version>.zip
  (Mojang's official download location; `-Latest` asks Mojang's download-links API for the current
  release instead) into a fresh directory %LOCALAPPDATA%\BeetrootSkin-bds\<Version> outside the
  repository. Nothing is committed. The server version must speak the client's network protocol:
  the client is 1.26.51.01, whose matching server build is 1.26.51.1.

  By downloading the server you accept the Minecraft EULA and Privacy Policy
  (https://www.minecraft.net/eula, https://go.microsoft.com/fwlink/?LinkId=521839), like with the
  download button on minecraft.net.

.EXAMPLE
  pwsh -NoProfile -File tools/e2e/get-bds.ps1                  # 1.26.51.1 (matches the client)
  pwsh -NoProfile -File tools/e2e/get-bds.ps1 -Version 1.26.52.3
  pwsh -NoProfile -File tools/e2e/get-bds.ps1 -Latest
#>
param(
    [string]$Version = '1.26.51.1',
    [switch]$Latest,
    [string]$Root = (Join-Path $env:LOCALAPPDATA 'BeetrootSkin-bds'),
    [switch]$Force
)
$ErrorActionPreference = 'Stop'

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
