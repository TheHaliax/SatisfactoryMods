# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Regenerate Icon1024/512/128.png from tools/ BaseIcon + Mask + UnispaceBd.
# Defaults match make-icons.py color scheme -- do not invent palettes here.
param(
    [Parameter(Mandatory = $true)]
    [string]$ModRoot,

    [string]$ModName = '',

    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'

$ModRoot = (Resolve-Path -LiteralPath $ModRoot).Path
if ([string]::IsNullOrWhiteSpace($ModName)) {
    $ModName = Split-Path -Leaf $ModRoot
}

$ScriptDir = $PSScriptRoot
$MakeIcons = Join-Path $ScriptDir 'make-icons.py'
$Uplugin = Join-Path $ModRoot "$ModName.uplugin"
$OutDir = Join-Path $ModRoot 'Resources'

if (-not (Test-Path -LiteralPath $MakeIcons)) {
    throw "make-icons.py not found at $MakeIcons"
}
if (-not (Test-Path -LiteralPath $Uplugin)) {
    throw "uplugin not found at $Uplugin"
}

$Font = Join-Path $ScriptDir 'UnispaceBd.ttf'
$Bg = Join-Path $ScriptDir 'BaseIcon1024.png'
$Mask = Join-Path $ScriptDir 'Mask1024.png'
foreach ($Asset in @($Font, $Bg, $Mask)) {
    if (-not (Test-Path -LiteralPath $Asset)) {
        throw "Icon pipeline asset missing: $Asset"
    }
}

Write-Host "Icons: $ModName <- $Uplugin"
& $Python $MakeIcons `
    --background $Bg `
    --mask $Mask `
    --font $Font `
    --uplugin $Uplugin `
    --out-dir $OutDir
if ($LASTEXITCODE -ne 0) {
    throw "make-icons.py failed for $ModName (exit $LASTEXITCODE)"
}

Get-ChildItem -LiteralPath $OutDir -Filter 'Icon*.png' |
    Format-Table Name, Length, LastWriteTime
