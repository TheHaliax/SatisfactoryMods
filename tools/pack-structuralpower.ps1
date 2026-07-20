# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Iterative dev build: compiles the StructuralPower runtime module and deploys the DLL into a
# local Satisfactory install. UBT '-Module=' builds skip the '.modules' manifest, so the
# loader can't map the module to its DLL — this hand-writes it.
#
# Paths: pass -ProjectPath / -GamePath / -UeCssRoot, or set SAT_PROJECT_PATH / SAT_GAME_PATH /
# UE_CSS_ROOT, or create tools/local-paths.ps1 (gitignored) with $LocalProjectPath etc.
param(
    [ValidateSet('Shipping', 'Development')]
    [string]$Config = 'Shipping',
    [switch]$NoCopy,
    [switch]$Clean,
    [switch]$SkipIcons,
    [string]$ProjectPath = '',
    [string]$ModRoot = '',
    [string]$StarterModLink = '',
    [string]$GamePath = '',
    [string]$UeCssRoot = ''
)

$ErrorActionPreference = 'Stop'

$LocalPathsFile = Join-Path $PSScriptRoot 'local-paths.ps1'
if (Test-Path -LiteralPath $LocalPathsFile) { . $LocalPathsFile }
if ([string]::IsNullOrWhiteSpace($ProjectPath) -and $LocalProjectPath) { $ProjectPath = $LocalProjectPath }
if ([string]::IsNullOrWhiteSpace($GamePath) -and $LocalGamePath) { $GamePath = $LocalGamePath }
if ([string]::IsNullOrWhiteSpace($UeCssRoot) -and $LocalUeCssRoot) { $UeCssRoot = $LocalUeCssRoot }
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = $env:SAT_PROJECT_PATH
}
if ([string]::IsNullOrWhiteSpace($GamePath)) { $GamePath = $env:SAT_GAME_PATH }
if ([string]::IsNullOrWhiteSpace($UeCssRoot)) { $UeCssRoot = $env:UE_CSS_ROOT }
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    throw 'Pass -ProjectPath, set SAT_PROJECT_PATH, or define $LocalProjectPath in tools/local-paths.ps1'
}
if ([string]::IsNullOrWhiteSpace($GamePath)) {
    throw 'Pass -GamePath, set SAT_GAME_PATH, or define $LocalGamePath in tools/local-paths.ps1'
}
if ([string]::IsNullOrWhiteSpace($UeCssRoot)) {
    throw 'Pass -UeCssRoot, set UE_CSS_ROOT, or define $LocalUeCssRoot in tools/local-paths.ps1'
}
if ([string]::IsNullOrWhiteSpace($StarterModLink)) {
    $StarterModLink = Join-Path (Split-Path -Parent $ProjectPath) 'Mods\StructuralPower'
}

function Resolve-StructuralPowerModRoot {
    param(
        [string]$ExplicitModRoot,
        [string]$ScriptRoot,
        [string]$LinkPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitModRoot)) {
        if (-not (Test-Path -LiteralPath $ExplicitModRoot)) {
            throw "ModRoot not found at $ExplicitModRoot"
        }
        return (Resolve-Path -LiteralPath $ExplicitModRoot).Path
    }

    # This script lives in <repo>/tools, so the mod is the sibling <repo>/StructuralPower.
    $RepoModRoot = Join-Path (Split-Path -Parent $ScriptRoot) 'StructuralPower'
    if (Test-Path -LiteralPath (Join-Path $RepoModRoot 'StructuralPower.uplugin')) {
        return (Resolve-Path -LiteralPath $RepoModRoot).Path
    }

    if (Test-Path -LiteralPath $LinkPath) {
        $LinkItem = Get-Item -LiteralPath $LinkPath -Force
        if ($LinkItem.LinkType -eq 'Junction') {
            $Target = $LinkItem.Target
            if ($Target.StartsWith('\??\')) {
                $Target = $Target.Substring(4)
            }
            if (-not (Test-Path -LiteralPath $Target)) {
                throw "StarterProject junction target missing: $Target"
            }
            return (Resolve-Path -LiteralPath $Target).Path
        }

        if (Test-Path -LiteralPath (Join-Path $LinkPath 'StructuralPower.uplugin')) {
            return (Resolve-Path -LiteralPath $LinkPath).Path
        }
    }

    throw 'Cannot resolve StructuralPower ModRoot. Pass -ModRoot explicitly.'
}

function Resolve-UeDotNet {
    param([string]$Root)

    $Bundled = Join-Path $Root 'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'
    if (Test-Path -LiteralPath $Bundled) {
        return $Bundled
    }

    return 'dotnet'
}

function Stop-StaleUbtProcesses {
    $Stopped = 0
    Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.CommandLine -notmatch 'UnrealBuildTool\.dll') {
            return
        }

        Write-Warning "Stopping stale UBT (PID $($_.ProcessId))"
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
        $script:Stopped++
    }

    if ($Stopped -gt 0) {
        Start-Sleep -Seconds 2
    }
}

function Invoke-Ubt {
    param(
        [string]$DotNetExe,
        [string]$UbtDll,
        [string[]]$BuildArgs
    )

    $Output = & $DotNetExe $UbtDll @BuildArgs 2>&1
    $ExitCode = $LASTEXITCODE

    # Exit 10 is UBT's build-mutex conflict; a stale UBT is usually holding it.
    if ($ExitCode -eq 10) {
        Write-Warning 'UBT mutex conflict (exit 10). Clearing stale build and retrying once...'
        Stop-StaleUbtProcesses
        $Output = & $DotNetExe $UbtDll @BuildArgs 2>&1
        $ExitCode = $LASTEXITCODE
    }

    return [PSCustomObject]@{
        Output   = $Output
        ExitCode = $ExitCode
    }
}

$Ubt = Join-Path $UeCssRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
$LogDir = if ($LocalLogDir) { $LocalLogDir } else { $env:TEMP }
$LogFile = Join-Path $LogDir 'pack-structuralpower.log'
New-Item -ItemType Directory -Force -Path (Split-Path $LogFile) | Out-Null

if (-not (Test-Path -LiteralPath $Ubt)) {
    throw "UnrealBuildTool not found at $Ubt"
}

$ModRoot = Resolve-StructuralPowerModRoot -ExplicitModRoot $ModRoot -ScriptRoot $PSScriptRoot -LinkPath $StarterModLink
$DotNetExe = Resolve-UeDotNet -Root $UeCssRoot

$UpluginPath = Join-Path $ModRoot 'StructuralPower.uplugin'
if (-not (Test-Path -LiteralPath $UpluginPath)) {
    throw "StructuralPower.uplugin not found under ModRoot: $ModRoot"
}

$UbtArgs = @(
    'FactoryGameSteam', 'Win64', $Config,
    "-Project=$ProjectPath",
    '-Module=StructuralPower'
)

if ($Clean) {
    Write-Warning '-Clean: full FactoryGameSteam rebuild (~4-5 min). Normal iter: omit -Clean.'
    $UbtArgs += '-Rebuild'
}
else {
    Write-Host 'Incremental build: StructuralPower module only (pass -Clean for full rebuild).'
}

Write-Host "Building StructuralPower ($Config) from $ModRoot"
Write-Host "DotNet: $DotNetExe"

$Result = Invoke-Ubt -DotNetExe $DotNetExe -UbtDll $Ubt -BuildArgs $UbtArgs
$Result.Output | Tee-Object -FilePath $LogFile
if ($Result.ExitCode -ne 0) {
    throw "UBT failed with exit code $($Result.ExitCode). Log: $LogFile"
}

$DllName = "FactoryGameSteam-StructuralPower-Win64-$Config.dll"
$ModulesName = "FactoryGameSteam-Win64-$Config.modules"
$SourceDll = Join-Path $ModRoot "Binaries\Win64\$DllName"
if (-not (Test-Path -LiteralPath $SourceDll)) {
    throw "Built DLL not found at $SourceDll"
}

$ModulesJson = @"
{
	"BuildId": "SML",
	"Modules":
	{
		"StructuralPower": "$DllName"
	}
}
"@

$SourceModulesPath = Join-Path $ModRoot "Binaries\Win64\$ModulesName"
New-Item -ItemType Directory -Force -Path (Split-Path $SourceModulesPath) | Out-Null
[System.IO.File]::WriteAllText($SourceModulesPath, $ModulesJson, (New-Object System.Text.UTF8Encoding $false))

if (-not $SkipIcons) {
    & (Join-Path $PSScriptRoot 'Invoke-ModIcons.ps1') -ModRoot $ModRoot -ModName 'StructuralPower'
}

$GameModRoot = Join-Path $GamePath 'FactoryGame\Mods\StructuralPower'
$DestBin = Join-Path $GameModRoot 'Binaries\Win64'
New-Item -ItemType Directory -Force -Path $DestBin | Out-Null

if (-not $NoCopy) {
    Copy-Item -Force (Join-Path $ModRoot 'StructuralPower.uplugin') $GameModRoot
    Copy-Item -Force $SourceDll (Join-Path $DestBin $DllName)
    Copy-Item -Force $SourceModulesPath (Join-Path $DestBin $ModulesName)

    $ResourcesSrc = Join-Path $ModRoot 'Resources'
    if (Test-Path -LiteralPath $ResourcesSrc) {
        Copy-Item -Recurse -Force $ResourcesSrc (Join-Path $GameModRoot 'Resources')
    }

    $ConfigSrc = Join-Path $ModRoot 'Config'
    if (Test-Path -LiteralPath $ConfigSrc) {
        Copy-Item -Recurse -Force $ConfigSrc (Join-Path $GameModRoot 'Config')
    }

    Write-Host "Deployed to $GameModRoot"
    Get-ChildItem $DestBin | Format-Table Name, Length, LastWriteTime
}

Write-Host 'Done.'
