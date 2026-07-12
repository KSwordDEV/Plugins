param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'vswhere.exe was not found. Install Visual Studio 2022 Build Tools.'
}

$installationPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $installationPath) {
    throw 'Visual Studio 2022 MSBuild was not found.'
}

$msbuild = Join-Path $installationPath 'MSBuild\Current\Bin\MSBuild.exe'
$project = Join-Path $projectRoot 'nDPI-Network-Inspector.vcxproj'
& $msbuild $project /t:Rebuild /p:Configuration=$Configuration /p:Platform=x64 /m:1 /v:minimal
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$builtExecutable = Join-Path $projectRoot "bin\x64\$Configuration\ndpi-network-inspector.exe"
if (-not (Test-Path -LiteralPath $builtExecutable)) {
    throw "Build completed without the expected executable: $builtExecutable"
}
Copy-Item -LiteralPath $builtExecutable -Destination (Join-Path $projectRoot 'ndpi-network-inspector.exe') -Force
Write-Host "Built: $builtExecutable"
