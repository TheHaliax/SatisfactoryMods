# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Alpakit Release via CLI: cooks the SMR upload zip (client + Windows server + Linux server,
# merged). Mirrors the UAT command SML's build.yml runs.
# Paths: -ProjectPath / -UeCssRoot, or SAT_PROJECT_PATH / UE_CSS_ROOT, or tools/local-paths.ps1.
param(
    [string]$ProjectPath = '',
    [string]$ModName = 'StructuralPower',
    [string]$UeCssRoot = '',
    [switch]$SkipIcons
)

$ErrorActionPreference = 'Stop'

$LocalPathsFile = Join-Path $PSScriptRoot 'local-paths.ps1'
if (Test-Path -LiteralPath $LocalPathsFile) { . $LocalPathsFile }
if ([string]::IsNullOrWhiteSpace($ProjectPath) -and $LocalProjectPath) { $ProjectPath = $LocalProjectPath }
if ([string]::IsNullOrWhiteSpace($UeCssRoot) -and $LocalUeCssRoot) { $UeCssRoot = $LocalUeCssRoot }
if ([string]::IsNullOrWhiteSpace($ProjectPath)) { $ProjectPath = $env:SAT_PROJECT_PATH }
if ([string]::IsNullOrWhiteSpace($UeCssRoot)) { $UeCssRoot = $env:UE_CSS_ROOT }
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    throw 'Pass -ProjectPath, set SAT_PROJECT_PATH, or define $LocalProjectPath in tools/local-paths.ps1'
}
if ([string]::IsNullOrWhiteSpace($UeCssRoot)) {
    throw 'Pass -UeCssRoot, set UE_CSS_ROOT, or define $LocalUeCssRoot in tools/local-paths.ps1'
}

$RunUat = Join-Path $UeCssRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$LogDir = if ($LocalLogDir) { $LocalLogDir } else { $env:TEMP }
$LogFile = Join-Path $LogDir "pack-$($ModName.ToLowerInvariant())-release.log"
New-Item -ItemType Directory -Force -Path (Split-Path $LogFile) | Out-Null

if (-not (Test-Path $RunUat)) {
    Write-Error "RunUAT not found at $RunUat"
}

if (-not (Test-Path $ProjectPath)) {
    Write-Error "Project not found at $ProjectPath"
}

$ProjectPath = (Resolve-Path $ProjectPath).Path

$ProjectDir = Split-Path $ProjectPath -Parent
$ModRoot = Join-Path $ProjectDir "Mods\$ModName"
if (-not (Test-Path -LiteralPath (Join-Path $ModRoot "$ModName.uplugin"))) {
    throw "Mod root missing uplugin: $ModRoot"
}

if (-not $SkipIcons) {
    & (Join-Path $PSScriptRoot 'Invoke-ModIcons.ps1') -ModRoot $ModRoot -ModName $ModName
}

# The cook runs the editor, which must LOAD the plugin's editor module. Two gotchas bite here:
#   1. '-Module=' builds do NOT emit the '.modules' manifest, and the loader maps
#      module -> dll via that manifest.
#   2. 'git clean' wipes untracked plugin Binaries, deleting the editor dll + manifest.
# So build the editor module and (re)write its manifest first, or the cook fails with
# "module '<Mod>' could not be found" (ExitCode 25, Error_UnknownCookFailure).
$PluginBinWin64 = Join-Path $ProjectDir "Mods\$ModName\Binaries\Win64"
$EditorDll = "UnrealEditor-$ModName.dll"
$EditorDllPath = Join-Path $PluginBinWin64 $EditorDll
$DotNet = Join-Path $UeCssRoot 'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'
if (-not (Test-Path $DotNet)) { $DotNet = 'dotnet' }
$Ubt = Join-Path $UeCssRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'

$ubtEditorArgs = @('FactoryEditor', 'Win64', 'Development', "-Project=$ProjectPath", "-Module=$ModName")
if (-not (Test-Path $EditorDllPath)) {
    # UBT can report "up to date" off stale intermediates when the dll was cleaned; force it.
    $ubtEditorArgs += '-Rebuild'
}
Write-Host "Building $ModName editor module (cook prerequisite)..."
& $DotNet $Ubt @ubtEditorArgs 2>&1 | Tee-Object -FilePath $LogFile | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Editor module build failed with exit code $LASTEXITCODE" }
if (-not (Test-Path $EditorDllPath)) { throw "Editor module dll missing after build: $EditorDllPath" }

# Editor BuildId must match the project's editor build; copy it from the project manifest.
$ProjectEditorModules = Join-Path $ProjectDir 'Binaries\Win64\UnrealEditor.modules'
if (-not (Test-Path $ProjectEditorModules)) {
    throw "Project editor manifest missing: $ProjectEditorModules (build FactoryEditor Development first)"
}
$BuildId = (Get-Content $ProjectEditorModules -Raw | ConvertFrom-Json).BuildId
$PluginEditorManifest = @"
{
	"BuildId": "$BuildId",
	"Modules":
	{
		"$ModName": "$EditorDll"
	}
}
"@
$PluginEditorModules = Join-Path $PluginBinWin64 'UnrealEditor.modules'
[System.IO.File]::WriteAllText($PluginEditorModules, $PluginEditorManifest, (New-Object System.Text.UTF8Encoding $false))
Write-Host "Wrote editor manifest: $PluginEditorModules (BuildId $BuildId)"

# Same command Alpakit Release runs (see SML .github/workflows/build.yml).
$uatArgs = @(
    "-ScriptsForProject=`"$ProjectPath`""
    'PackagePlugin'
    "-project=`"$ProjectPath`""
    "-DLCName=$ModName"
    '-merge'
    '-build'
    '-server'
    '-clientconfig=Shipping'
    '-serverconfig=Shipping'
    '-platform=Win64'
    '-serverplatform=Win64+Linux'
    '-nocompileeditor'
    '-installed'
)

Write-Host "Alpakit Release via CLI ($ModName, Shipping, Win64 + Win64 Server + Linux Server)..."
Write-Host "Log: $LogFile"

& $RunUat @uatArgs 2>&1 | Tee-Object -FilePath $LogFile

if ($LASTEXITCODE -ne 0) {
    throw "RunUAT PackagePlugin failed with exit code $LASTEXITCODE"
}

$archiveRoot = Join-Path (Split-Path $ProjectPath -Parent) "Saved\ArchivedPlugins\$ModName"
$zip = Join-Path $archiveRoot "$ModName.zip"

if (-not (Test-Path $zip)) {
    throw "Expected archive not found: $zip"
}

Write-Host "SMR upload zip: $zip"
Get-Item $zip | Format-List FullName, Length, LastWriteTime
