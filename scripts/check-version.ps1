#Requires -Version 5.1
<#
    Guards each mod's version metadata against drift. ficsit.app rejects a plugin whose
    SemVer major differs from Version (this repo hit that on the 2.0.0 upload), and a
    changelog that lags the shipped version misleads players. Run before every release.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $RepoRoot)
{
    $ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $PSCommandPath }
    $RepoRoot = Split-Path -Parent $ScriptDir
}

function Get-Prop
{
    param($Object, [Parameter(Mandatory)][string]$Name)

    if ($Object.PSObject.Properties.Name -contains $Name)
    {
        return $Object.$Name
    }
    return $null
}

function Get-MajorVersion
{
    param([Parameter(Mandatory)][string]$Version)

    $Match = [regex]::Match($Version, '(\d+)')
    if (-not $Match.Success)
    {
        throw "Cannot parse a major version from '$Version'"
    }
    return [int]$Match.Groups[1].Value
}

$Plugins = Get-ChildItem -LiteralPath $RepoRoot -Directory |
    ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -Filter '*.uplugin' -File -ErrorAction SilentlyContinue }

if (-not $Plugins)
{
    Write-Host 'No .uplugin files found; nothing to check.'
    exit 0
}

$Failures = [System.Collections.Generic.List[string]]::new()

foreach ($Plugin in $Plugins)
{
    $ModName = $Plugin.BaseName
    $Manifest = Get-Content -LiteralPath $Plugin.FullName -Raw | ConvertFrom-Json

    $Version = Get-Prop $Manifest 'Version'
    $SemVersion = Get-Prop $Manifest 'SemVersion'
    if ($null -eq $Version -or $null -eq $SemVersion)
    {
        $Failures.Add("${ModName}: uplugin missing Version or SemVersion")
        continue
    }
    $Version = [int]$Version

    if ((Get-MajorVersion $SemVersion) -ne $Version)
    {
        $Failures.Add("${ModName}: Version ($Version) != SemVersion major ($SemVersion)")
    }

    $VersionName = Get-Prop $Manifest 'VersionName'
    if ($VersionName -and $VersionName -ne $SemVersion)
    {
        $Failures.Add("${ModName}: VersionName ($VersionName) != SemVersion ($SemVersion)")
    }

    $RemoteRange = Get-Prop $Manifest 'RemoteVersionRange'
    if ($RemoteRange -and (Get-MajorVersion $RemoteRange) -ne $Version)
    {
        $Failures.Add("${ModName}: RemoteVersionRange ($RemoteRange) major != Version ($Version)")
    }

    $Changelog = Join-Path $Plugin.DirectoryName 'CHANGELOG.md'
    if (Test-Path -LiteralPath $Changelog)
    {
        $Heading = Select-String -LiteralPath $Changelog -Pattern '^##\s+([0-9]+\.[0-9]+\.[0-9]+)' |
            Select-Object -First 1
        if (-not $Heading)
        {
            $Failures.Add("${ModName}: CHANGELOG.md has no '## x.y.z' heading")
        }
        elseif ($Heading.Matches[0].Groups[1].Value -ne $SemVersion)
        {
            $Top = $Heading.Matches[0].Groups[1].Value
            $Failures.Add("${ModName}: CHANGELOG top ($Top) != SemVersion ($SemVersion)")
        }
    }

    Write-Host "Checked $ModName -> $SemVersion"
}

if ($Failures.Count -gt 0)
{
    Write-Host ''
    Write-Host 'Version check FAILED:'
    $Failures | ForEach-Object { Write-Host "  - $_" }
    exit 1
}

Write-Host 'Version check passed.'
exit 0
