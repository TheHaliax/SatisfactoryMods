# Formats or checks first-party C++ (StructuralPower, PipelineColor, RemoteStandby).
param(
	[switch]$Check,
	[string]$ClangFormat = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

if (-not $ClangFormat) {
	$candidates = @(
		"C:\Program Files\LLVM\bin\clang-format.exe",
		"clang-format"
	)
	foreach ($c in $candidates) {
		if ($c -eq "clang-format") {
			$cmd = Get-Command clang-format -ErrorAction SilentlyContinue
			if ($cmd) { $ClangFormat = $cmd.Source; break }
		} elseif (Test-Path $c) {
			$ClangFormat = $c
			break
		}
	}
}

if (-not $ClangFormat -or -not (Test-Path $ClangFormat)) {
	Write-Error "clang-format not found. Install LLVM or pass -ClangFormat."
}

$roots = @(
	(Join-Path $RepoRoot "StructuralPower\Source"),
	(Join-Path $RepoRoot "PipelineColor\Source"),
	(Join-Path $RepoRoot "RemoteStandby\Source")
)

$files = @()
foreach ($root in $roots) {
	if (Test-Path $root) {
		$files += Get-ChildItem -Path $root -Recurse -Include *.cpp, *.h -File
	}
}

$files = $files | Sort-Object FullName -Unique
Write-Host "clang-format ($($files.Count) files) via $ClangFormat"

$failed = 0
foreach ($f in $files) {
	if ($Check) {
		& $ClangFormat --dry-run -Werror --style=file $f.FullName
		if ($LASTEXITCODE -ne 0) { $failed++ }
	} else {
		& $ClangFormat -i --style=file $f.FullName
		if ($LASTEXITCODE -ne 0) { $failed++ }
	}
}

if ($failed -gt 0) {
	Write-Error "clang-format reported $failed failure(s)."
}

Write-Host "OK."
