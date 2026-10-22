# Prepare short webui copy and build installer
param(
    [string]$ShortPath = "C:\ps_webui_temp",
    [string]$ISCCPath = "C:\Users\javil\AppData\Local\Programs\Inno Setup 6\ISCC.exe",
    [string]$IssFile = "PrettySumatraPDF.iss"
)

Write-Host "Preparing short webui copy at $ShortPath"
if (Test-Path $ShortPath) {
    Remove-Item -Recurse -Force $ShortPath
}
New-Item -ItemType Directory -Path $ShortPath | Out-Null

$srcDir = Join-Path $PSScriptRoot "..\..\prettysumatra\webui"
$src = Resolve-Path $srcDir
Write-Host "Copying from $src to $ShortPath"
# Use robocopy for robust copy
$robocopyArgs = @(
    $src.Path,
    $ShortPath,
    "/E",
    "/COPY:DAT",
    "/R:3",
    "/W:5",
    "/NFL",
    "/NDL",
    "/NJH",
    "/NJS",
    "/NC",
    "/NS"
)
$rc = & robocopy @robocopyArgs
$rc = $LASTEXITCODE
if ($LASTEXITCODE -ge 8) {
    Write-Error "Robocopy failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# Set env var for this process
$env:SHORT_WEBUI_SRC = $ShortPath
Write-Host "SHORT_WEBUI_SRC set to $env:SHORT_WEBUI_SRC"

# Run ISCC in this process so the preprocessor can read the env var
Write-Host "Running ISCC: $ISCCPath $IssFile"
& "$ISCCPath" "$PSScriptRoot\$IssFile"
exit $LASTEXITCODE
