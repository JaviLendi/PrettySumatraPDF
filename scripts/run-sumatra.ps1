param(
    [string]$Config = 'dbg64'
)

# prefer the canonical SumatraPDF.exe if present, otherwise fall back to the -dll name
$base = Join-Path -Path ${PSScriptRoot} -ChildPath "..\out\$Config"
$exe1 = Join-Path -Path $base -ChildPath 'SumatraPDF.exe'
$exe2 = Join-Path -Path $base -ChildPath 'SumatraPDF-dll.exe'

if (Test-Path $exe1) {
    Start-Process -FilePath $exe1 -WorkingDirectory (Split-Path $exe1)
} elseif (Test-Path $exe2) {
    Start-Process -FilePath $exe2 -WorkingDirectory (Split-Path $exe2)
} else {
    Write-Error "No existe ninguno de: $exe1 o $exe2"
    exit 1
}
