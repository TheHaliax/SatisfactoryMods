# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Iterative dev build: PipelineColor runtime module → local Satisfactory Mods.
# Paths: -ProjectPath / -GamePath / -UeCssRoot, or SAT_PROJECT_PATH / SAT_GAME_PATH /
# UE_CSS_ROOT, or tools/local-paths.ps1 (gitignored).
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
$ModName = 'PipelineColor'

$LocalPathsFile = Join-Path $PSScriptRoot 'local-paths.ps1'
if (Test-Path -LiteralPath $LocalPathsFile) { . $LocalPathsFile }
if ([string]::IsNullOrWhiteSpace($ProjectPath) -and $LocalProjectPath) { $ProjectPath = $LocalProjectPath }
if ([string]::IsNullOrWhiteSpace($GamePath) -and $LocalGamePath) { $GamePath = $LocalGamePath }
if ([string]::IsNullOrWhiteSpace($UeCssRoot) -and $LocalUeCssRoot) { $UeCssRoot = $LocalUeCssRoot }
if ([string]::IsNullOrWhiteSpace($ProjectPath)) { $ProjectPath = $env:SAT_PROJECT_PATH }
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
    $StarterModLink = Join-Path (Split-Path -Parent $ProjectPath) "Mods\$ModName"
}

function Resolve-PipelineColorModRoot {
    param([string]$ExplicitModRoot, [string]$ScriptRoot, [string]$LinkPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitModRoot)) {
        if (-not (Test-Path -LiteralPath $ExplicitModRoot)) {
            throw "ModRoot not found at $ExplicitModRoot"
        }
        return (Resolve-Path -LiteralPath $ExplicitModRoot).Path
    }

    $RepoModRoot = Join-Path (Split-Path -Parent $ScriptRoot) $ModName
    if (Test-Path -LiteralPath (Join-Path $RepoModRoot "$ModName.uplugin")) {
        return (Resolve-Path -LiteralPath $RepoModRoot).Path
    }

    if (Test-Path -LiteralPath $LinkPath) {
        $LinkItem = Get-Item -LiteralPath $LinkPath -Force
        if ($LinkItem.LinkType -eq 'Junction') {
            $Target = $LinkItem.Target
            if ($Target.StartsWith('\??\')) { $Target = $Target.Substring(4) }
            return (Resolve-Path -LiteralPath $Target).Path
        }
        if (Test-Path -LiteralPath (Join-Path $LinkPath "$ModName.uplugin")) {
            return (Resolve-Path -LiteralPath $LinkPath).Path
        }
    }

    throw "Cannot resolve $ModName ModRoot. Pass -ModRoot explicitly."
}

function Resolve-UeDotNet {
    param([string]$Root)
    $Bundled = Join-Path $Root 'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'
    if (Test-Path -LiteralPath $Bundled) { return $Bundled }
    return 'dotnet'
}

function Stop-StaleUbtProcesses {
    Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.CommandLine -notmatch 'UnrealBuildTool\.dll') { return }
        Write-Warning "Stopping stale UBT (PID $($_.ProcessId))"
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    }
}

$Ubt = Join-Path $UeCssRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
$LogDir = if ($LocalLogDir) { $LocalLogDir } else { $env:TEMP }
$LogFile = Join-Path $LogDir 'pack-pipelinecolor.log'
New-Item -ItemType Directory -Force -Path (Split-Path $LogFile) | Out-Null

if (-not (Test-Path -LiteralPath $Ubt)) {
    throw "UnrealBuildTool not found at $Ubt"
}

$ModRoot = Resolve-PipelineColorModRoot -ExplicitModRoot $ModRoot -ScriptRoot $PSScriptRoot -LinkPath $StarterModLink
$DotNetExe = Resolve-UeDotNet -Root $UeCssRoot

if (-not (Test-Path -LiteralPath $StarterModLink)) {
    New-Item -ItemType Junction -Path $StarterModLink -Target $ModRoot | Out-Null
    Write-Host "OK junction: $StarterModLink -> $ModRoot"
}

$UbtArgs = @(
    'FactoryGameSteam', 'Win64', $Config,
    "-Project=$ProjectPath",
    "-Module=$ModName"
)

if ($Clean) {
    $UbtArgs += '-Rebuild'
}

Write-Host "Building $ModName ($Config) from $ModRoot"
$Output = & $DotNetExe $Ubt @UbtArgs 2>&1
$ExitCode = $LASTEXITCODE
if ($ExitCode -eq 10) {
    Write-Warning 'UBT mutex conflict; clearing stale and retrying...'
    Stop-StaleUbtProcesses
    Start-Sleep -Seconds 2
    $Output = & $DotNetExe $Ubt @UbtArgs 2>&1
    $ExitCode = $LASTEXITCODE
}
$Output | Tee-Object -FilePath $LogFile
if ($ExitCode -ne 0) {
    throw "UBT failed with exit code $ExitCode. Log: $LogFile"
}

$DllName = "FactoryGameSteam-$ModName-Win64-$Config.dll"
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
		"$ModName": "$DllName"
	}
}
"@

$SourceModulesPath = Join-Path $ModRoot "Binaries\Win64\$ModulesName"
New-Item -ItemType Directory -Force -Path (Split-Path $SourceModulesPath) | Out-Null
[System.IO.File]::WriteAllText($SourceModulesPath, $ModulesJson, (New-Object System.Text.UTF8Encoding $false))

if (-not $SkipIcons) {
    & (Join-Path $PSScriptRoot 'Invoke-ModIcons.ps1') -ModRoot $ModRoot -ModName $ModName
}

$GameModRoot = Join-Path $GamePath "FactoryGame\Mods\$ModName"
$DestBin = Join-Path $GameModRoot 'Binaries\Win64'
New-Item -ItemType Directory -Force -Path $DestBin | Out-Null

if (-not $NoCopy) {
    Copy-Item -Force (Join-Path $ModRoot "$ModName.uplugin") $GameModRoot
    Copy-Item -Force $SourceDll (Join-Path $DestBin $DllName)
    Copy-Item -Force $SourceModulesPath (Join-Path $DestBin $ModulesName)

    $ResourcesSrc = Join-Path $ModRoot 'Resources'
    if (Test-Path -LiteralPath $ResourcesSrc) {
        $ResourcesDest = Join-Path $GameModRoot 'Resources'
        New-Item -ItemType Directory -Force -Path $ResourcesDest | Out-Null
        Copy-Item -Force (Join-Path $ResourcesSrc '*') $ResourcesDest
    }
    $ConfigSrc = Join-Path $ModRoot 'Config'
    if (Test-Path -LiteralPath $ConfigSrc) {
        $ConfigDest = Join-Path $GameModRoot 'Config'
        New-Item -ItemType Directory -Force -Path $ConfigDest | Out-Null
        Copy-Item -Force (Join-Path $ConfigSrc '*') $ConfigDest
    }

    Write-Host "Deployed to $GameModRoot"
    Get-ChildItem $DestBin | Format-Table Name, Length, LastWriteTime
}

Write-Host "Done ($ModName)."
