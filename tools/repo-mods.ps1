# SPDX-FileCopyrightText: 2026 Haliax
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Shared: discover first-party mods (folder name + matching .uplugin).
# Dot-source from other tools scripts.
function Get-RepoModNames {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    Get-ChildItem -LiteralPath $RepoRoot -Directory |
        Where-Object {
            Test-Path -LiteralPath (Join-Path $_.FullName "$($_.Name).uplugin")
        } |
        Sort-Object Name |
        ForEach-Object { $_.Name }
}

function Test-IsAllModsToken {
    param([string]$Value)
    return ($Value -ieq 'All' -or $Value -eq '*')
}
