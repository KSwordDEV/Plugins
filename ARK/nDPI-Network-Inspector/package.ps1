param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $SkipBuild) {
    & (Join-Path $projectRoot 'build.ps1') -Configuration Release
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$stageRoot = Join-Path $projectRoot '.package'
$installDirectory = Join-Path $stageRoot 'ndpi-network-inspector'
$archivePath = Join-Path $projectRoot 'nDPI-Network-Inspector.zip'
if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $installDirectory | Out-Null

foreach ($fileName in @('plugin.json', 'ndpi-network-inspector.exe', 'README.md', 'LICENSE.txt', 'NOTICE')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $fileName) -Destination $installDirectory -Force
}
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
Compress-Archive -LiteralPath $installDirectory -DestinationPath $archivePath -CompressionLevel Optimal
Remove-Item -LiteralPath $stageRoot -Recurse -Force

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash
Write-Host "Archive: $archivePath"
Write-Host "SHA256: $hash"
