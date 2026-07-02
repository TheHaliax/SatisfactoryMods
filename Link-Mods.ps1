param(
    [string]$Mod = "",
    [string]$ModRepoRoot = $PSScriptRoot,
    [string]$StarterModsPath = "E:\Modding\Satisfactory\StarterProject\Mods"
)

$ErrorActionPreference = "Stop"

function Link-ModDirectory {
    param(
        [string]$ModName,
        [string]$SourceRoot,
        [string]$ModsPath
    )

    $source = Join-Path $SourceRoot $ModName
    $link = Join-Path $ModsPath $ModName

    if (-not (Test-Path $source)) {
        Write-Warning "Skip $ModName — source not found: $source"
        return
    }

    if (-not (Test-Path (Join-Path $source "$ModName.uplugin"))) {
        Write-Warning "Skip $ModName — no $ModName.uplugin in source"
        return
    }

    if (Test-Path $link) {
        $item = Get-Item $link -Force
        if ($item.LinkType -eq "Junction") {
            $existingTarget = $item.Target
            if ($existingTarget -eq $source) {
                Write-Host "OK (already linked): $ModName -> $source"
                return
            }
            Remove-Item $link -Force
        }
        else {
            $backup = "$link._prejunction"
            if (Test-Path $backup) {
                Remove-Item -Recurse -Force $backup
            }
            Write-Host "Renaming existing folder -> $backup (delete manually if stale)"
            Rename-Item -Path $link -NewName (Split-Path $backup -Leaf)
        }
    }

    New-Item -ItemType Junction -Path $link -Target $source | Out-Null
    Write-Host "Linked: $ModName -> $source"
}

if (-not (Test-Path $StarterModsPath)) {
    throw "StarterProject Mods path not found: $StarterModsPath"
}

if ($Mod) {
    Link-ModDirectory -ModName $Mod -SourceRoot $ModRepoRoot -ModsPath $StarterModsPath
}
else {
    Get-ChildItem $ModRepoRoot -Directory | ForEach-Object {
        Link-ModDirectory -ModName $_.Name -SourceRoot $ModRepoRoot -ModsPath $StarterModsPath
    }
}

Write-Host "Done."
