# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Dot-source from build tools. Reads tools/local-paths.json (gitignored).
# See local-paths.example.json for schema.
function Get-LocalPathsConfig {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ToolsDir,
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $JsonPath = Join-Path $ToolsDir 'local-paths.json'
    if (-not (Test-Path -LiteralPath $JsonPath)) {
        return $null
    }

    $Raw = Get-Content -LiteralPath $JsonPath -Raw -Encoding UTF8
    $Cfg = $Raw | ConvertFrom-Json

    $ProjectPath = [string](Get-LocalPathsProp $Cfg 'projectPath')
    $UeCssRoot = [string](Get-LocalPathsProp $Cfg 'ueCssRoot')
    $GamePath = [string](Get-LocalPathsProp $Cfg 'gamePath')
    $LogDir = [string](Get-LocalPathsProp $Cfg 'logDir')
    $Ensure = $true
    if ($null -ne (Get-LocalPathsProp $Cfg 'ensureJunctions')) {
        $Ensure = [bool]$Cfg.ensureJunctions
    }

    $Junctions = [System.Collections.Generic.List[object]]::new()
    $List = Get-LocalPathsProp $Cfg 'junctions'
    if ($List) {
        foreach ($Item in @($List)) {
            $Junctions.Add((Resolve-LocalPathJunction -Item $Item -RepoRoot $RepoRoot -ProjectPath $ProjectPath)) |
                Out-Null
        }
    }

    return [PSCustomObject]@{
        JsonPath        = $JsonPath
        ProjectPath     = $ProjectPath
        UeCssRoot       = $UeCssRoot
        GamePath        = $GamePath
        LogDir          = $LogDir
        EnsureJunctions = $Ensure
        Junctions       = $Junctions
    }
}

function Get-LocalPathsProp {
    param($Object, [string]$Name)
    if ($null -eq $Object) { return $null }
    if ($Object.PSObject.Properties.Name -contains $Name) {
        return $Object.$Name
    }
    return $null
}

function Resolve-LocalPathTarget {
    param([string]$Target, [string]$RepoRoot)
    if ([string]::IsNullOrWhiteSpace($Target)) { return $null }
    if ([System.IO.Path]::IsPathRooted($Target)) {
        return $Target
    }
    return (Join-Path $RepoRoot $Target)
}

function Resolve-LocalPathJunction {
    param($Item, [string]$RepoRoot, [string]$ProjectPath)

    $Link = [string](Get-LocalPathsProp $Item 'link')
    $Name = [string](Get-LocalPathsProp $Item 'name')
    $Target = [string](Get-LocalPathsProp $Item 'target')

    if ([string]::IsNullOrWhiteSpace($Link)) {
        if ([string]::IsNullOrWhiteSpace($Name)) {
            throw 'junction entry needs "name" or "link"'
        }
        if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
            throw "junction name '$Name' needs projectPath to resolve Mods/<name>"
        }
        $ProjectDir = Split-Path -Parent $ProjectPath
        $Link = Join-Path $ProjectDir "Mods\$Name"
        if ([string]::IsNullOrWhiteSpace($Target)) {
            $Target = $Name
        }
    }

    if ([string]::IsNullOrWhiteSpace($Target)) {
        throw "junction '$Link' missing target"
    }

    $ResolvedTarget = Resolve-LocalPathTarget -Target $Target -RepoRoot $RepoRoot
    return [PSCustomObject]@{
        Name   = $Name
        Link   = $Link
        Target = $ResolvedTarget
    }
}

function Ensure-LocalPathJunctions {
    param(
        [Parameter(Mandatory = $true)]
        $Config,
        [string]$OnlyName = ''
    )

    if (-not $Config -or -not $Config.EnsureJunctions) { return }

    foreach ($J in @($Config.Junctions)) {
        if ($OnlyName -and $J.Name -and ($J.Name -ne $OnlyName)) { continue }
        if ($OnlyName -and -not $J.Name) {
            # named filter: skip absolute-only entries unless link leaf matches
            $Leaf = Split-Path -Leaf $J.Link
            if ($Leaf -ne $OnlyName) { continue }
        }

        if (-not (Test-Path -LiteralPath $J.Target)) {
            throw "Junction target missing: $($J.Target)"
        }
        $TargetFull = (Resolve-Path -LiteralPath $J.Target).Path
        $LinkParent = Split-Path -Parent $J.Link
        if (-not (Test-Path -LiteralPath $LinkParent)) {
            New-Item -ItemType Directory -Force -Path $LinkParent | Out-Null
        }

        if (Test-Path -LiteralPath $J.Link) {
            $Item = Get-Item -LiteralPath $J.Link -Force
            if ($Item.LinkType -eq 'Junction' -or $Item.LinkType -eq 'SymbolicLink') {
                $Existing = $Item.Target
                if ($Existing -is [array]) { $Existing = $Existing[0] }
                $Existing = [string]$Existing
                if ($Existing.StartsWith('\??\')) { $Existing = $Existing.Substring(4) }
                if ([string]::Equals($Existing, $TargetFull, [StringComparison]::OrdinalIgnoreCase)) {
                    continue
                }
                Write-Warning "Replacing junction $($J.Link) -> $TargetFull (was $Existing)"
                Remove-Item -LiteralPath $J.Link -Force
            }
            else {
                throw "Path exists and is not a junction/symlink: $($J.Link)"
            }
        }

        New-Item -ItemType Junction -Path $J.Link -Target $TargetFull | Out-Null
        Write-Host "Junction: $($J.Link) -> $TargetFull"
    }
}
