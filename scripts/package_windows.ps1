# Build a standalone Windows executable for termtrans.
#
# The script configures a static vcpkg triplet by default, builds termtrans,
# then copies the resulting application executable to dist with a versioned name.

[CmdletBinding()]
param(
    [string]$BuildDir,
    [string]$StageDir,
    [string]$OutDir,
    [string]$Config = "Release",
    [string]$Version,
    [string]$Generator,
    [string]$Architecture = "x64",
    [string]$VcpkgRoot,
    [string]$VcpkgTriplet = "x64-windows-static",
    [string]$ToolchainFile,
    [switch]$NoClean
)

$ErrorActionPreference = "Stop"

function Get-ScriptRoot {
    if ($PSScriptRoot) {
        return $PSScriptRoot
    }

    return Split-Path -Parent $MyInvocation.MyCommand.Path
}

function Get-ProjectVersion {
    param([string]$RepoRoot)

    if ($Version) {
        return $Version
    }

    $cmakeText = Get-Content -Raw -Path (Join-Path $RepoRoot "CMakeLists.txt")
    $match = [regex]::Match(
        $cmakeText,
        "project\(termtrans\s+VERSION\s+([0-9][^\s]*)\s+LANGUAGES\s+CXX\)"
    )
    if ($match.Success) {
        return $match.Groups[1].Value
    }

    return "0.1.0"
}

function Assert-Command {
    param([string]$CommandName)

    if (-not (Get-Command $CommandName -ErrorAction SilentlyContinue)) {
        throw "Missing command: $CommandName"
    }
}

function Resolve-VcpkgToolchain {
    if ($ToolchainFile) {
        if (-not (Test-Path -LiteralPath $ToolchainFile)) {
            throw "CMake toolchain file was not found: $ToolchainFile"
        }

        return (Resolve-Path -LiteralPath $ToolchainFile).Path
    }

    $roots = @()
    if ($VcpkgRoot) {
        $roots += $VcpkgRoot
    }
    if ($env:VCPKG_ROOT) {
        $roots += $env:VCPKG_ROOT
    }
    $roots += "C:\git\vcpkg"

    foreach ($root in $roots) {
        $candidate = Join-Path $root "scripts/buildsystems/vcpkg.cmake"
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Find-TermtransExecutable {
    param(
        [string]$BuildRoot,
        [string]$BuildConfig
    )

    $directCandidates = @(
        (Join-Path $BuildRoot (Join-Path $BuildConfig "termtrans.exe")),
        (Join-Path $BuildRoot "termtrans.exe")
    )

    foreach ($candidate in $directCandidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $matches = Get-ChildItem -LiteralPath $BuildRoot -Recurse -Filter "termtrans.exe" -File |
        Where-Object { $_.FullName -notmatch "\\CMakeFiles\\" } |
        Sort-Object `
            @{ Expression = { if ($_.FullName -like "*\$BuildConfig\termtrans.exe") { 0 } else { 1 } } },
            @{ Expression = "LastWriteTime"; Descending = $true }

    if ($matches) {
        return $matches[0].FullName
    }

    throw "Built termtrans.exe was not found under: $BuildRoot"
}

$scriptRoot = Get-ScriptRoot
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..")).Path

if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build/package-windows"
}
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot "dist"
}

Assert-Command "cmake"

if ($StageDir) {
    Write-Warning "-StageDir is ignored because package_windows.ps1 now emits a standalone executable instead of an installer."
}

$resolvedVersion = Get-ProjectVersion -RepoRoot $repoRoot
$outputExe = Join-Path $OutDir "termtrans-$resolvedVersion-windows-$Architecture.exe"
$toolchain = Resolve-VcpkgToolchain

if (-not $NoClean -and (Test-Path -LiteralPath $BuildDir)) {
    $fullRepoRoot = [System.IO.Path]::GetFullPath($repoRoot)
    $fullBuildDir = [System.IO.Path]::GetFullPath($BuildDir)
    if (-not $fullBuildDir.StartsWith($fullRepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean build directory outside repo root: $BuildDir"
    }

    Remove-Item -Recurse -Force -LiteralPath $BuildDir
}

$configureArgs = @(
    "-S", $repoRoot,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DTERMTRANS_STATIC_RUNTIME=ON",
    "-DVCPKG_TARGET_TRIPLET=$VcpkgTriplet"
)
if ($toolchain) {
    $configureArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
} else {
    Write-Warning "vcpkg toolchain file was not found; CMake will use the current environment to resolve dependencies."
}
if ($Generator) {
    $configureArgs = @("-G", $Generator) + $configureArgs
}

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

& cmake @configureArgs
& cmake --build $BuildDir --config $Config --target termtrans

$builtExe = Find-TermtransExecutable -BuildRoot $BuildDir -BuildConfig $Config
Copy-Item -LiteralPath $builtExe -Destination $outputExe -Force

Write-Host "Created Windows executable: $outputExe"
