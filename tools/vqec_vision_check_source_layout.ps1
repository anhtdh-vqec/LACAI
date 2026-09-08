param([string] $_root = (Split-Path -Parent $PSScriptRoot))

# Read-only structural checks, not a C++ parser, compiler or ABI/ownership validator.
$ErrorActionPreference = 'Stop'
$project_root = (Resolve-Path -LiteralPath $_root).Path
$source_roots = @('src', 'include', 'tests', 'tools')
$source_extensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.ps1', '.py', '.sh')
$source_files = @($source_roots | ForEach-Object {
    Get-ChildItem -LiteralPath (Join-Path $project_root $_) -Recurse -File |
        Where-Object { $_.Extension -in $source_extensions }
})
$issues = [System.Collections.Generic.List[string]]::new()
# These are the project include roots exported by current CMake targets. Checking
# existence across this set is NOT proof of target-specific transitive visibility.
$include_roots = @('include', 'src', 'src/app', 'src/adapters/camera',
    'src/adapters/qualcomm', 'src/runtime/model_registry', 'src/outputs', 'src/adapters/fw_output')
foreach ($source_file in $source_files) {
    if ($source_file.Name -cnotmatch '^vqec_vision_[a-z][a-z0-9]*(?:_[a-z0-9]+)*\.[a-z0-9]+$') {
        $issues.Add("Invalid source filename: $($source_file.FullName)")
    }
    if ($source_file.Extension -notin @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx')) {
        continue
    }
    $source_text = [System.IO.File]::ReadAllText($source_file.FullName)
    foreach ($include_match in [regex]::Matches($source_text, '(?m)^\s*#\s*include\s*"([^"]+)"')) {
        $include_path = $include_match.Groups[1].Value
        # All quoted includes in the current tree are project-owned, not external SDK headers.
        $candidate_paths = @((Join-Path $source_file.DirectoryName $include_path))
        $candidate_paths += @($include_roots | ForEach-Object {
            Join-Path (Join-Path $project_root $_) $include_path
        })
        $is_found = $false
        foreach ($candidate_path in $candidate_paths) {
            if (Test-Path -LiteralPath $candidate_path -PathType Leaf) { $is_found = $true }
        }
        if (-not $is_found) {
            $issues.Add("Unresolved quoted include: $($source_file.Name): $include_path")
        }
    }
}
$cmake_text = [System.IO.File]::ReadAllText((Join-Path $project_root 'CMakeLists.txt'))
foreach ($path_match in [regex]::Matches($cmake_text, '(?:src|tests|tools)/[a-zA-Z0-9_/]+\.(?:cpp|cc|cxx|c)\b')) {
    if (-not (Test-Path -LiteralPath (Join-Path $project_root $path_match.Value) -PathType Leaf)) {
        $issues.Add("Missing CMake source: $($path_match.Value)")
    }
}
if ($issues.Count -gt 0) {
    $issues | ForEach-Object { Write-Output $_ }
    exit 1
}
Write-Output "PASS: $($source_files.Count) source/tool filenames, quoted includes and CMake source paths."
