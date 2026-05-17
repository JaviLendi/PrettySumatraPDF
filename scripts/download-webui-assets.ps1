$ErrorActionPreference = 'Stop'

$webui = Join-Path $PSScriptRoot '..\prettysumatra\webui'
$vendor = Join-Path $webui 'vendor'
$faDir = Join-Path $vendor 'fontawesome'
$faFonts = Join-Path $faDir 'webfonts'
$interDir = Join-Path $vendor 'inter'
$interFonts = Join-Path $interDir 'fonts'

New-Item -ItemType Directory -Force -Path $faFonts | Out-Null
New-Item -ItemType Directory -Force -Path $interFonts | Out-Null

$miDir = Join-Path $vendor 'material-icons'
$miFonts = Join-Path $miDir 'fonts'

New-Item -ItemType Directory -Force -Path $miFonts | Out-Null

$miCssPath = Join-Path $miDir 'material-icons.css'
$miGoogleUrl = 'https://fonts.googleapis.com/css2?family=Material+Symbols+Rounded:opsz,wght,FILL,GRAD@20..48,400,0,0&display=block'
Invoke-WebRequest -Uri $miGoogleUrl -OutFile $miCssPath
$miCss = Get-Content -Raw $miCssPath
$matches = [regex]::Matches($miCss, 'https://fonts\.gstatic\.com/[^\)\s]+\.(woff2|woff|ttf)')
$seen = @{}
foreach ($m in $matches) {
    $url = $m.Value
    if ($seen.ContainsKey($url)) { continue }
    $seen[$url] = $true
    $name = Split-Path $url -Leaf
    Invoke-WebRequest -Uri $url -OutFile (Join-Path $miFonts $name)
    $miCss = $miCss.Replace($url, "fonts/$name")
}
Set-Content -Path $miCssPath -Value $miCss -Encoding utf8
$miCss = Get-Content -Raw $miCssPath
$miCss = $miCss.Replace("  font-weight: normal;`n", "  font-optical-sizing: auto;`n  font-variation-settings: 'FILL' 0, 'wght' 100, 'GRAD' 20, 'opsz' 20;`n  font-weight: normal;`n")
Set-Content -Path $miCssPath -Value $miCss -Encoding utf8

$interCssPath = Join-Path $interDir 'inter.css'
Invoke-WebRequest -Uri 'https://fonts.googleapis.com/css2?family=Inter:wght@100;200;400;500;600;700;800&display=swap' -OutFile $interCssPath
$interCss = Get-Content -Raw $interCssPath
$matches = [regex]::Matches($interCss, 'https://fonts\.gstatic\.com/[^\)\s]+\.ttf')
$seen = @{}
foreach ($m in $matches) {
    $url = $m.Value
    if ($seen.ContainsKey($url)) { continue }
    $seen[$url] = $true
    $name = Split-Path $url -Leaf
    Invoke-WebRequest -Uri $url -OutFile (Join-Path $interFonts $name)
    $interCss = $interCss.Replace($url, "fonts/$name")
}
Set-Content -Path $interCssPath -Value $interCss -Encoding utf8

Write-Host 'Downloaded offline web UI assets.'
