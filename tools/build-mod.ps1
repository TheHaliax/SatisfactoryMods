# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Single mod build entry (-Mod All = every repo mod with a matching .uplugin).
#   Quick   — UBT module → local Steam Mods (DLL + config + resources)
#   Release — Alpakit PackagePlugin zip (Win client + Win/Linux server)
#
# Order: version check → icons (when wanted) → UBT / Alpakit.
# Bad version aborts before icons or compile.
#
# Examples:
#   powershell -File tools/build-mod.ps1 -Mod PipelineColor
#   powershell -File tools/build-mod.ps1 -Mod All -Icons
#   powershell -File tools/build-mod.ps1 -Mod StructuralPower -Mode Release
#   powershell -File tools/icons.ps1 All
#   powershell -File tools/check-version.ps1 -Mod All
param(
    [Parameter(Mandatory = $true)]
    [string]$Mod,

    [ValidateSet('Quick', 'Release')]
    [string]$Mode = 'Quick',

    [ValidateSet('Shipping', 'Development')]
    [string]$Config = 'Shipping',

    [switch]$Icons,
    [switch]$NoIcons,
    [switch]$NoCopy,
    [switch]$Clean,
    [switch]$SkipVersionCheck,

    [string]$ProjectPath = '',
    [string]$GamePath = '',
    [string]$UeCssRoot = '',
    [string]$ModRoot = ''
)

$ErrorActionPreference = 'Stop'
$ToolsDir = $PSScriptRoot
$RepoRoot = Split-Path -Parent $ToolsDir
. (Join-Path $ToolsDir 'repo-mods.ps1')
. (Join-Path $ToolsDir 'load-local-paths.ps1')

# --- paths (CLI / env / tools/local-paths.json) ---
$LocalCfg = Get-LocalPathsConfig -ToolsDir $ToolsDir -RepoRoot $RepoRoot
if ($LocalCfg) {
    if ([string]::IsNullOrWhiteSpace($ProjectPath) -and $LocalCfg.ProjectPath) {
        $ProjectPath = $LocalCfg.ProjectPath
    }
    if ([string]::IsNullOrWhiteSpace($GamePath) -and $LocalCfg.GamePath) {
        $GamePath = $LocalCfg.GamePath
    }
    if ([string]::IsNullOrWhiteSpace($UeCssRoot) -and $LocalCfg.UeCssRoot) {
        $UeCssRoot = $LocalCfg.UeCssRoot
    }
}
if ([string]::IsNullOrWhiteSpace($ProjectPath)) { $ProjectPath = $env:SAT_PROJECT_PATH }
if ([string]::IsNullOrWhiteSpace($GamePath)) { $GamePath = $env:SAT_GAME_PATH }
if ([string]::IsNullOrWhiteSpace($UeCssRoot)) { $UeCssRoot = $env:UE_CSS_ROOT }

if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    throw 'Pass -ProjectPath, set SAT_PROJECT_PATH, or projectPath in tools/local-paths.json (see local-paths.example.json)'
}
if ([string]::IsNullOrWhiteSpace($UeCssRoot)) {
    throw 'Pass -UeCssRoot, set UE_CSS_ROOT, or ueCssRoot in tools/local-paths.json'
}
if ($Mode -eq 'Quick' -and [string]::IsNullOrWhiteSpace($GamePath)) {
    throw 'Quick mode needs -GamePath, SAT_GAME_PATH, or gamePath in tools/local-paths.json'
}

# --- all mods ---
if (Test-IsAllModsToken $Mod) {
    if (-not [string]::IsNullOrWhiteSpace($ModRoot)) {
        throw '-ModRoot cannot be used with -Mod All'
    }
    $Names = @(Get-RepoModNames -RepoRoot $RepoRoot)
    if ($Names.Count -eq 0) {
        throw "No mods with .uplugin under $RepoRoot"
    }

    if ($LocalCfg) {
        Ensure-LocalPathJunctions -Config $LocalCfg
    }

    $ChildArgs = @{
        Mode   = $Mode
        Config = $Config
    }
    if (-not [string]::IsNullOrWhiteSpace($ProjectPath)) { $ChildArgs.ProjectPath = $ProjectPath }
    if (-not [string]::IsNullOrWhiteSpace($GamePath)) { $ChildArgs.GamePath = $GamePath }
    if (-not [string]::IsNullOrWhiteSpace($UeCssRoot)) { $ChildArgs.UeCssRoot = $UeCssRoot }
    if ($Icons) { $ChildArgs.Icons = $true }
    if ($NoIcons) { $ChildArgs.NoIcons = $true }
    if ($NoCopy) { $ChildArgs.NoCopy = $true }
    if ($Clean) { $ChildArgs.Clean = $true }
    if ($SkipVersionCheck) { $ChildArgs.SkipVersionCheck = $true }

    Write-Host ("All mods ({0}): {1}" -f $Mode, ($Names -join ', '))
    foreach ($Name in $Names) {
        Write-Host ''
        Write-Host "======== $Name ($Mode) ========"
        & $PSCommandPath -Mod $Name @ChildArgs
    }
    Write-Host ''
    Write-Host "Done (All / $Mode)."
    exit 0
}

$ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
$ProjectDir = Split-Path -Parent $ProjectPath
$LogDir = if ($LocalCfg -and $LocalCfg.LogDir) { $LocalCfg.LogDir } else { 'D:\Modding\Satisfactory\logs' }
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$LogFile = Join-Path $LogDir ("build-{0}-{1}.log" -f $Mod.ToLowerInvariant(), $Mode.ToLowerInvariant())

function Resolve-ModRoot {
    param([string]$Name, [string]$Explicit, [string]$Repo, [string]$ProjDir)

    if (-not [string]::IsNullOrWhiteSpace($Explicit)) {
        if (-not (Test-Path -LiteralPath $Explicit)) {
            throw "ModRoot not found: $Explicit"
        }
        return (Resolve-Path -LiteralPath $Explicit).Path
    }

    $RepoMod = Join-Path $Repo $Name
    if (Test-Path -LiteralPath (Join-Path $RepoMod "$Name.uplugin")) {
        return (Resolve-Path -LiteralPath $RepoMod).Path
    }

    $Link = Join-Path $ProjDir "Mods\$Name"
    if (Test-Path -LiteralPath $Link) {
        $Item = Get-Item -LiteralPath $Link -Force
        if ($Item.LinkType -eq 'Junction') {
            $Target = $Item.Target
            if ($Target.StartsWith('\??\')) { $Target = $Target.Substring(4) }
            return (Resolve-Path -LiteralPath $Target).Path
        }
        if (Test-Path -LiteralPath (Join-Path $Link "$Name.uplugin")) {
            return (Resolve-Path -LiteralPath $Link).Path
        }
    }

    throw "Cannot resolve $Name ModRoot. Pass -ModRoot."
}

function Resolve-UeDotNet {
    param([string]$Root)
    $Bundled = Join-Path $Root 'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'
    if (Test-Path -LiteralPath $Bundled) { return $Bundled }
    return 'dotnet'
}

function Stop-StaleUbt {
    Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" -EA SilentlyContinue |
        Where-Object { $_.CommandLine -match 'UnrealBuildTool\.dll' } |
        ForEach-Object {
            Write-Warning "Stopping stale UBT (PID $($_.ProcessId))"
            Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue
        }
}

function Invoke-Ubt {
    param([string]$DotNet, [string]$UbtDll, [string[]]$UbtCliArgs)
    $Out = & $DotNet $UbtDll @UbtCliArgs 2>&1
    $Code = $LASTEXITCODE
    if ($Code -eq 10) {
        Write-Warning 'UBT mutex conflict; retry once...'
        Stop-StaleUbt
        Start-Sleep -Seconds 2
        $Out = & $DotNet $UbtDll @UbtCliArgs 2>&1
        $Code = $LASTEXITCODE
    }
    return [PSCustomObject]@{ Output = $Out; ExitCode = $Code }
}

function Ensure-StarterJunction {
    param([string]$Name, [string]$Root, [string]$ProjDir)
    $Link = Join-Path $ProjDir "Mods\$Name"
    if (-not (Test-Path -LiteralPath $Link)) {
        New-Item -ItemType Junction -Path $Link -Target $Root | Out-Null
        Write-Host "Junction: $Link -> $Root"
    }
}

function Ensure-ModJunction {
    param(
        [string]$Name,
        [string]$Root,
        [string]$ProjDir,
        $Config
    )

    if ($Config) {
        $Listed = @($Config.Junctions | Where-Object {
                ($_.Name -eq $Name) -or ((Split-Path -Leaf $_.Link) -eq $Name)
            })
        if ($Listed.Count -gt 0) {
            Ensure-LocalPathJunctions -Config $Config -OnlyName $Name
            return
        }
    }

    Ensure-StarterJunction -Name $Name -Root $Root -ProjDir $ProjDir
}

$ModRoot = Resolve-ModRoot -Name $Mod -Explicit $ModRoot -Repo $RepoRoot -ProjDir $ProjectDir
$Uplugin = Join-Path $ModRoot "$Mod.uplugin"
if (-not (Test-Path -LiteralPath $Uplugin)) {
    throw "Missing uplugin: $Uplugin"
}

# --- version before icons / build ---
if (-not $SkipVersionCheck) {
    $CheckVersion = Join-Path $ToolsDir 'check-version.ps1'
    & $CheckVersion -Mod $Mod -ModRoot $ModRoot -RepoRoot $RepoRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Version check failed for $Mod (exit $LASTEXITCODE). Fix uplugin/CHANGELOG or pass -SkipVersionCheck."
    }
}

$WantIcons = $Icons -or ($Mode -eq 'Release' -and -not $NoIcons)
if ($NoIcons) { $WantIcons = $false }

if ($WantIcons) {
    & (Join-Path $ToolsDir 'icons.ps1') -Mod $Mod -ModRoot $ModRoot
}

$DotNet = Resolve-UeDotNet -Root $UeCssRoot
$Ubt = Join-Path $UeCssRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
if (-not (Test-Path -LiteralPath $Ubt)) {
    throw "UnrealBuildTool not found: $Ubt"
}

Ensure-ModJunction -Name $Mod -Root $ModRoot -ProjDir $ProjectDir -Config $LocalCfg

if ($Mode -eq 'Quick') {
    $UbtArgs = @(
        'FactoryGameSteam', 'Win64', $Config,
        "-Project=$ProjectPath",
        "-Module=$Mod"
    )
    if ($Clean) { $UbtArgs += '-Rebuild' }

    Write-Host "Quick build $Mod ($Config) from $ModRoot"
    $Result = Invoke-Ubt -DotNet $DotNet -UbtDll $Ubt -UbtCliArgs $UbtArgs
    $Result.Output | Tee-Object -FilePath $LogFile
    if ($Result.ExitCode -ne 0) {
        throw "UBT failed exit $($Result.ExitCode). Log: $LogFile"
    }

    $DllName = "FactoryGameSteam-$Mod-Win64-$Config.dll"
    $ModulesName = "FactoryGameSteam-Win64-$Config.modules"
    $SourceDll = Join-Path $ModRoot "Binaries\Win64\$DllName"
    if (-not (Test-Path -LiteralPath $SourceDll)) {
        throw "DLL missing: $SourceDll"
    }

    $ModulesJson = @"
{
	"BuildId": "SML",
	"Modules":
	{
		"$Mod": "$DllName"
	}
}
"@
    $SourceModules = Join-Path $ModRoot "Binaries\Win64\$ModulesName"
    New-Item -ItemType Directory -Force -Path (Split-Path $SourceModules) | Out-Null
    [System.IO.File]::WriteAllText(
        $SourceModules, $ModulesJson, (New-Object System.Text.UTF8Encoding $false))

    if (-not $NoCopy) {
        $GameModRoot = Join-Path $GamePath "FactoryGame\Mods\$Mod"
        $DestBin = Join-Path $GameModRoot 'Binaries\Win64'
        New-Item -ItemType Directory -Force -Path $DestBin | Out-Null
        Copy-Item -Force $Uplugin $GameModRoot
        Copy-Item -Force $SourceDll (Join-Path $DestBin $DllName)
        Copy-Item -Force $SourceModules (Join-Path $DestBin $ModulesName)

        foreach ($Leaf in @('Resources', 'Config')) {
            $Src = Join-Path $ModRoot $Leaf
            if (Test-Path -LiteralPath $Src) {
                $Dst = Join-Path $GameModRoot $Leaf
                New-Item -ItemType Directory -Force -Path $Dst | Out-Null
                Copy-Item -Force (Join-Path $Src '*') $Dst
            }
        }

        Write-Host "Deployed $GameModRoot"
        Get-ChildItem $DestBin | Format-Table Name, Length, LastWriteTime
    }
}
else {
    # Release — editor module + Alpakit PackagePlugin
    $PluginBin = Join-Path $ModRoot 'Binaries\Win64'
    $EditorDll = "UnrealEditor-$Mod.dll"
    $EditorDllPath = Join-Path $PluginBin $EditorDll

    $EditorArgs = @(
        'FactoryEditor', 'Win64', 'Development',
        "-Project=$ProjectPath",
        "-Module=$Mod"
    )
    if (-not (Test-Path -LiteralPath $EditorDllPath)) {
        $EditorArgs += '-Rebuild'
    }

    Write-Host "Editor module $Mod (Release prereq)..."
    $Ed = Invoke-Ubt -DotNet $DotNet -UbtDll $Ubt -UbtCliArgs $EditorArgs
    $Ed.Output | Tee-Object -FilePath $LogFile
    if ($Ed.ExitCode -ne 0) {
        throw "Editor UBT failed exit $($Ed.ExitCode). Log: $LogFile"
    }
    if (-not (Test-Path -LiteralPath $EditorDllPath)) {
        throw "Editor DLL missing: $EditorDllPath"
    }

    $ProjectEditorModules = Join-Path $ProjectDir 'Binaries\Win64\UnrealEditor.modules'
    if (-not (Test-Path -LiteralPath $ProjectEditorModules)) {
        Write-Warning 'Project UnrealEditor.modules missing - restoring via FactoryEditor WriteMetadata...'
        $StampArgs = @('FactoryEditor', 'Win64', 'Development', "-Project=$ProjectPath")
        $Stamp = Invoke-Ubt -DotNet $DotNet -UbtDll $Ubt -UbtCliArgs $StampArgs
        $Stamp.Output | Tee-Object -FilePath $LogFile -Append
        if ($Stamp.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $ProjectEditorModules)) {
            throw "Project UnrealEditor.modules still missing after FactoryEditor build: $ProjectEditorModules"
        }
    }
    $BuildId = (Get-Content $ProjectEditorModules -Raw | ConvertFrom-Json).BuildId
    $EditorManifest = @(
        '{'
        "`t`"BuildId`": `"$BuildId`","
        "`t`"Modules`":"
        "`t{"
        "`t`t`"$Mod`": `"$EditorDll`""
        "`t}"
        '}'
    ) -join "`n"
    $PluginEditorModules = Join-Path $PluginBin 'UnrealEditor.modules'
    New-Item -ItemType Directory -Force -Path $PluginBin | Out-Null
    [System.IO.File]::WriteAllText(
        $PluginEditorModules, $EditorManifest + "`n", (New-Object System.Text.UTF8Encoding $false))
    Write-Host "Editor manifest BuildId=$BuildId"

    $RunUat = Join-Path $UeCssRoot 'Engine\Build\BatchFiles\RunUAT.bat'
    if (-not (Test-Path -LiteralPath $RunUat)) {
        throw "RunUAT missing: $RunUat"
    }

    $UatArgs = @(
        "-ScriptsForProject=`"$ProjectPath`""
        'PackagePlugin'
        "-project=`"$ProjectPath`""
        "-DLCName=$Mod"
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

    Write-Host "Alpakit Release $Mod (Win64 + Win/Linux server)..."
    Write-Host "Log: $LogFile"
    & $RunUat @UatArgs 2>&1 | Tee-Object -FilePath $LogFile
    if ($LASTEXITCODE -ne 0) {
        throw "PackagePlugin failed exit $LASTEXITCODE. Log: $LogFile"
    }

    $Zip = Join-Path $ProjectDir "Saved\ArchivedPlugins\$Mod\$Mod.zip"
    if (-not (Test-Path -LiteralPath $Zip)) {
        throw "Archive missing: $Zip"
    }
    Write-Host "SMR zip: $Zip"
    Get-Item $Zip | Format-List FullName, Length, LastWriteTime
}

Write-Host "Done ($Mod / $Mode)."
