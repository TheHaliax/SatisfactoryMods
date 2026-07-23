#Requires -Version 5.1
# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Guards mod version metadata against drift. ficsit.app rejects a plugin whose
# SemVer major differs from Version. Changelog top must match SemVersion.
#
#   powershell -File tools/check-version.ps1
#   powershell -File tools/check-version.ps1 -Mod All
#   powershell -File tools/check-version.ps1 -Mod PipelineColor
param(
    [string]$Mod = '',
    [string]$RepoRoot = '',
    [string]$ModRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $RepoRoot) {
    $RepoRoot = Split-Path -Parent $PSScriptRoot
}
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
. (Join-Path $PSScriptRoot 'repo-mods.ps1')

function Get-Prop {
    param($Object, [Parameter(Mandatory)][string]$Name)
    if ($Object.PSObject.Properties.Name -contains $Name) {
        return $Object.$Name
    }
    return $null
}

function Get-MajorVersion {
    param([Parameter(Mandatory)][string]$Version)
    $Match = [regex]::Match($Version, '(\d+)')
    if (-not $Match.Success) {
        throw "Cannot parse a major version from '$Version'"
    }
    return [int]$Match.Groups[1].Value
}

function Test-ModVersion {
    param([Parameter(Mandatory)][string]$UpluginPath)

    $ModName = [System.IO.Path]::GetFileNameWithoutExtension($UpluginPath)
    $Failures = [System.Collections.Generic.List[string]]::new()
    $Manifest = Get-Content -LiteralPath $UpluginPath -Raw | ConvertFrom-Json

    $Version = Get-Prop $Manifest 'Version'
    $SemVersion = Get-Prop $Manifest 'SemVersion'
    if ($null -eq $Version -or $null -eq $SemVersion) {
        $Failures.Add("${ModName}: uplugin missing Version or SemVersion")
        return [PSCustomObject]@{ Name = $ModName; SemVersion = $null; Failures = $Failures }
    }
    $Version = [int]$Version

    if ((Get-MajorVersion $SemVersion) -ne $Version) {
        $Failures.Add("${ModName}: Version ($Version) != SemVersion major ($SemVersion)")
    }

    $VersionName = Get-Prop $Manifest 'VersionName'
    if ($VersionName -and $VersionName -ne $SemVersion) {
        $Failures.Add("${ModName}: VersionName ($VersionName) != SemVersion ($SemVersion)")
    }

    $RemoteRange = Get-Prop $Manifest 'RemoteVersionRange'
    if ($RemoteRange -and (Get-MajorVersion $RemoteRange) -ne $Version) {
        $Failures.Add("${ModName}: RemoteVersionRange ($RemoteRange) major != Version ($Version)")
    }

    $Changelog = Join-Path (Split-Path -Parent $UpluginPath) 'CHANGELOG.md'
    if (Test-Path -LiteralPath $Changelog) {
        $Heading = Select-String -LiteralPath $Changelog -Pattern '^##\s+([0-9]+\.[0-9]+\.[0-9]+)' |
            Select-Object -First 1
        if (-not $Heading) {
            $Failures.Add("${ModName}: CHANGELOG.md has no '## x.y.z' heading")
        }
        elseif ($Heading.Matches[0].Groups[1].Value -ne $SemVersion) {
            $Top = $Heading.Matches[0].Groups[1].Value
            $Failures.Add("${ModName}: CHANGELOG top ($Top) != SemVersion ($SemVersion)")
        }
    }

    return [PSCustomObject]@{
        Name       = $ModName
        SemVersion = $SemVersion
        Failures   = $Failures
    }
}

$Plugins = [System.Collections.Generic.List[string]]::new()

if (-not [string]::IsNullOrWhiteSpace($ModRoot)) {
    if (Test-IsAllModsToken $Mod) {
        throw '-ModRoot cannot be used with -Mod All'
    }
    $Name = Split-Path -Leaf $ModRoot
    $Path = Join-Path $ModRoot "$Name.uplugin"
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing uplugin: $Path"
    }
    $Plugins.Add((Resolve-Path -LiteralPath $Path).Path) | Out-Null
}
elseif (Test-IsAllModsToken $Mod -or [string]::IsNullOrWhiteSpace($Mod)) {
    foreach ($Name in (Get-RepoModNames -RepoRoot $RepoRoot)) {
        $Plugins.Add((Join-Path $RepoRoot "$Name\$Name.uplugin")) | Out-Null
    }
}
else {
    $Path = Join-Path $RepoRoot "$Mod\$Mod.uplugin"
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing uplugin: $Path"
    }
    $Plugins.Add((Resolve-Path -LiteralPath $Path).Path) | Out-Null
}

if ($Plugins.Count -eq 0) {
    Write-Host 'No .uplugin files found; nothing to check.'
    exit 0
}

$AllFailures = [System.Collections.Generic.List[string]]::new()
foreach ($PluginPath in $Plugins) {
    $Result = Test-ModVersion -UpluginPath $PluginPath
    if ($Result.Failures.Count -eq 0) {
        Write-Host "Checked $($Result.Name) -> $($Result.SemVersion)"
    }
    else {
        foreach ($F in $Result.Failures) { $AllFailures.Add($F) | Out-Null }
    }
}

if ($AllFailures.Count -gt 0) {
    Write-Host ''
    Write-Host 'Version check FAILED:'
    $AllFailures | ForEach-Object { Write-Host "  - $_" }
    exit 1
}

Write-Host 'Version check passed.'
exit 0
