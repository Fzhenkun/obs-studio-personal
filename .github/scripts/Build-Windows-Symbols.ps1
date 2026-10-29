[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string] $Version,
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+$')]
    [string] $BuildNumber,
    [ValidateSet('x64', 'arm64')]
    [string] $Target = 'x64',
    [string] $BuildDirectory,
    [string] $OutputDirectory = 'artifacts',
    [string] $CMake = 'cmake.exe',
    [string] $Generator = 'Visual Studio 18 2026'
)

$ErrorActionPreference = 'Stop'

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Command,
        [Parameter(Mandatory = $false)]
        [string[]] $Arguments = @()
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Command $($Arguments -join ' ')"
    }
}

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $ProjectRoot "build_$Target"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $ProjectRoot $BuildDirectory
}

$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
if (-not [System.IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path $ProjectRoot $OutputDirectory
}
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$Architecture = "windows-$Target"
$Commit = (& git -C $ProjectRoot rev-parse --short=12 HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or -not $Commit) {
    throw 'Unable to determine the current Git commit.'
}

$ArchiveDirectory = Join-Path $OutputDirectory "$Version/build-$BuildNumber/$Architecture-$Commit"
$InstallDirectory = Join-Path $ArchiveDirectory 'package'
$PackageDirectory = Join-Path $ArchiveDirectory 'packages'
$SymbolsDirectory = Join-Path $ArchiveDirectory 'symbols'
$MetadataPath = Join-Path $ArchiveDirectory 'build-info.txt'
$Platform = if ($Target -eq 'x64') { 'x64' } else { 'ARM64' }

New-Item -ItemType Directory -Force $ArchiveDirectory, $InstallDirectory, $PackageDirectory, $SymbolsDirectory |
    Out-Null

$ConfigureArguments = @(
    '-S', $ProjectRoot,
    '-B', $BuildDirectory,
    '-G', $Generator,
    '-A', $Platform,
    "-DOBS_VERSION_OVERRIDE=$Version",
    "-DOBS_BUILD_NUMBER=$BuildNumber"
)

Write-Host "Configuring $Version build $BuildNumber ($Architecture)..."
Invoke-External $CMake $ConfigureArguments

Write-Host 'Building RelWithDebInfo...'
Invoke-External $CMake @(
    '--build', $BuildDirectory,
    '--target', 'obs-studio',
    '--config', 'RelWithDebInfo',
    '--parallel'
)

Write-Host 'Installing runtime files and PDBs...'
Invoke-External $CMake @(
    '--install', $BuildDirectory,
    '--config', 'RelWithDebInfo',
    '--prefix', $InstallDirectory
)

Write-Host 'Creating runtime ZIP...'
Invoke-External $CMake @(
    '--build', $BuildDirectory,
    '--target', 'package',
    '--config', 'RelWithDebInfo'
)

$CpackPackage = Get-ChildItem -Path $BuildDirectory -Filter "obs-studio-*-windows-$Target.zip" -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $CpackPackage) {
    throw "CPack did not produce a Windows $Target ZIP in $BuildDirectory."
}

$RuntimeZip = Join-Path $PackageDirectory "obs-studio-$Version-build-$BuildNumber-$Architecture-$Commit.zip"
Copy-Item $CpackPackage.FullName $RuntimeZip -Force

$PdbFiles = Get-ChildItem -Path $InstallDirectory -Filter '*.pdb' -File -Recurse
if (-not $PdbFiles) {
    throw "No PDB files were found under $InstallDirectory."
}

foreach ($PdbFile in $PdbFiles) {
    $RelativePath = $PdbFile.FullName.Substring($InstallDirectory.Length).TrimStart('\', '/')
    $Destination = Join-Path $SymbolsDirectory $RelativePath
    New-Item -ItemType Directory -Force (Split-Path $Destination -Parent) | Out-Null
    Copy-Item $PdbFile.FullName $Destination -Force
}

@"
OBS_VERSION=$Version
OBS_BUILD_NUMBER=$BuildNumber
ARCHITECTURE=$Architecture
GIT_COMMIT=$Commit
CONFIGURATION=RelWithDebInfo
PDB_COUNT=$($PdbFiles.Count)
"@ | Set-Content $MetadataPath -Encoding UTF8
Copy-Item $MetadataPath (Join-Path $SymbolsDirectory 'build-info.txt') -Force

$SymbolsZip = Join-Path $PackageDirectory "obs-studio-$Version-build-$BuildNumber-$Architecture-$Commit-pdbs.zip"
Compress-Archive -Path (Join-Path $SymbolsDirectory '*') -DestinationPath $SymbolsZip -Force

Write-Host "Runtime package: $RuntimeZip"
Write-Host "Symbols package: $SymbolsZip"
Write-Host "Metadata: $MetadataPath"
