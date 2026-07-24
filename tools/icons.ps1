# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Universal mod icon generator. Call alone or from build-mod.ps1 (-Icons).
# Compositor: make-icons.py + BaseIcon/Mask/UnispaceBd (defaults only — no palette invent).
#
#   powershell -File tools/icons.ps1 StructuralPower
#   powershell -File tools/icons.ps1 All
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Mod,

    [string]$ModRoot = '',
    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
$ToolsDir = $PSScriptRoot
$RepoRoot = Split-Path -Parent $ToolsDir
. (Join-Path $ToolsDir 'repo-mods.ps1')

function Invoke-ModIcons {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModName,
        [string]$Root = '',
        [string]$PythonExe = 'python'
    )

    if ([string]::IsNullOrWhiteSpace($Root)) {
        $Root = Join-Path $RepoRoot $ModName
    }
    $Root = (Resolve-Path -LiteralPath $Root).Path
    $Leaf = Split-Path -Leaf $Root

    $MakeIcons = Join-Path $ToolsDir 'make-icons.py'
    $Uplugin = Join-Path $Root "$Leaf.uplugin"
    $OutDir = Join-Path $Root 'Resources'
    $Font = Join-Path $ToolsDir 'UnispaceBd.ttf'
    $Bg = Join-Path $ToolsDir 'BaseIcon1024.png'
    $Mask = Join-Path $ToolsDir 'Mask1024.png'

    foreach ($Path in @($MakeIcons, $Uplugin, $Font, $Bg, $Mask)) {
        if (-not (Test-Path -LiteralPath $Path)) {
            throw "Missing: $Path"
        }
    }

    Write-Host "Icons: $Leaf <- $Uplugin"
    & $PythonExe $MakeIcons `
        --background $Bg `
        --mask $Mask `
        --font $Font `
        --uplugin $Uplugin `
        --out-dir $OutDir
    if ($LASTEXITCODE -ne 0) {
        throw "make-icons.py failed for $Leaf (exit $LASTEXITCODE)"
    }

    Get-ChildItem -LiteralPath $OutDir -Filter 'Icon*.png' |
        Format-Table Name, Length, LastWriteTime
}

if (Test-IsAllModsToken $Mod) {
    if (-not [string]::IsNullOrWhiteSpace($ModRoot)) {
        throw '-ModRoot cannot be used with All'
    }
    $Names = @(Get-RepoModNames -RepoRoot $RepoRoot)
    if ($Names.Count -eq 0) {
        throw "No mods with .uplugin under $RepoRoot"
    }
    Write-Host ("Icons All: {0}" -f ($Names -join ', '))
    foreach ($Name in $Names) {
        Write-Host ''
        Write-Host "======== $Name ========"
        Invoke-ModIcons -ModName $Name -PythonExe $Python
    }
    exit 0
}

Invoke-ModIcons -ModName $Mod -Root $ModRoot -PythonExe $Python
