param(
    [string]$BuildDir = 'build',
    [string]$Configuration = 'Release',
    [string]$InnoCompiler = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$packageRoot = Join-Path $projectRoot 'dist\package'
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $projectRoot "$BuildDir\generated\version.hpp")
$releaseVersion = [regex]::Match($versionHeader, 'appVersion\[\] = "([^"]+)"').Groups[1].Value
$displayVersion = [regex]::Match($versionHeader, 'appVersionDisplay\[\] = "([^"]+)"').Groups[1].Value
if ($releaseVersion -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$' -or -not $displayVersion) { throw 'Configure the build to generate valid release metadata first' }
$numericVersion = $releaseVersion.Split('-')[0]
cmake --install (Join-Path $projectRoot $BuildDir) --config $Configuration --prefix $packageRoot
if ($LASTEXITCODE -ne 0) { throw 'Package staging failed' }
$archive = Join-Path $projectRoot "dist\dcs2psvr2-shaker-$releaseVersion-Windows-x64.zip"
Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $archive -Force
if (-not (Test-Path -LiteralPath $InnoCompiler)) { throw 'Set -InnoCompiler to the installed ISCC.exe path. The portable ZIP is ready.' }
& $InnoCompiler "/DAppVersion=$releaseVersion" "/DAppNumericVersion=$numericVersion" "/DAppDisplayVersion=$displayVersion" (Join-Path $projectRoot 'installer\dcs2psvr2-shaker.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
Get-FileHash -Algorithm SHA256 -LiteralPath $archive,(Join-Path $projectRoot "dist\dcs2psvr2-shaker-$releaseVersion-Setup.exe") | ForEach-Object {
    "$($_.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($_.Path))"
} | Set-Content -Encoding utf8 (Join-Path $projectRoot 'dist\SHA256SUMS.txt')
