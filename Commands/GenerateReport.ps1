# GenerateReport.ps1
# Az UE által generált index.html + index.json alapján önálló, file://-on is
# működő report.html-t hoz létre. CDN-ről tölti a függőségeket, az adatokat
# inline beágyazza, így internetkapcsolat nélkül is megnyitható helyi fájlként.

param(
    [string]$ReportDir = (Join-Path (Split-Path -Parent $PSScriptRoot) "Saved\TestResults")
)

$jsonPath = Join-Path $ReportDir "index.json"
$htmlPath = Join-Path $ReportDir "index.html"
$outPath  = Join-Path $ReportDir "report.html"

if (-not (Test-Path $jsonPath)) { Write-Error "index.json not found: $jsonPath"; exit 1 }
if (-not (Test-Path $htmlPath)) { Write-Error "index.html not found: $htmlPath"; exit 1 }

$json = Get-Content $jsonPath -Encoding UTF8 -Raw
$html = Get-Content $htmlPath -Encoding UTF8 -Raw

# --- CSS: bower -> CDN ---
$html = $html -replace [regex]::Escape('href="/bower_components/font-awesome/css/font-awesome.min.css"'),
    'href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css"'

$html = $html -replace [regex]::Escape('href="/bower_components/bootstrap/dist/css/bootstrap.min.css"'),
    'href="https://cdnjs.cloudflare.com/ajax/libs/bootstrap/3.4.1/css/bootstrap.min.css"'

# Nem CDN-en elérhető CSS-ek eltávolítása (twentytwenty, featherlight)
$html = $html -replace '<link[^>]+twentytwenty[^>]+>', ''
$html = $html -replace '<link[^>]+featherlight[^>]+>', ''

# --- JS: bower -> CDN ---
$html = $html -replace [regex]::Escape('src="/bower_components/jquery/dist/jquery.min.js"'),
    'src="https://cdnjs.cloudflare.com/ajax/libs/jquery/3.7.1/jquery.min.js"'

$html = $html -replace [regex]::Escape('src="/bower_components/bootstrap/dist/js/bootstrap.min.js"'),
    'src="https://cdnjs.cloudflare.com/ajax/libs/bootstrap/3.4.1/js/bootstrap.min.js"'

$html = $html -replace [regex]::Escape('src="/bower_components/dustjs-linkedin/dist/dust-full.min.js"'),
    'src="https://cdnjs.cloudflare.com/ajax/libs/dustjs-linkedin/2.7.5/dust-full.min.js"'

$html = $html -replace [regex]::Escape('src="/bower_components/clipboard/dist/clipboard.min.js"'),
    'src="https://cdnjs.cloudflare.com/ajax/libs/clipboard.js/2.0.11/clipboard.min.js"'

$html = $html -replace [regex]::Escape('src="/bower_components/numeral/min/numeral.min.js"'),
    'src="https://cdnjs.cloudflare.com/ajax/libs/numeral.js/2.0.6/numeral.min.js"'

# Nem CDN-en elérhető JS-ek eltávolítása
$html = $html -replace '<script[^>]+jquery\.event\.move[^>]+></script>', ''
$html = $html -replace '<script[^>]+jquery_lazyload[^>]+></script>', ''
$html = $html -replace '<script[^>]+jquery\.lazyload[^>]+></script>', ''
$html = $html -replace '<script[^>]+twentytwenty[^>]+></script>', ''
$html = $html -replace '<script[^>]+featherlight[^>]+></script>', ''
$html = $html -replace '<script[^>]+anchor-js[^>]+></script>', ''
$html = $html -replace '<script[^>]+anchor\.min[^>]+></script>', ''

# --- JSON adatok beágyazása ---
# Az index.html $.getJSON("index.json", ...) hívását felváltjuk inline adattal,
# mert file:// protokollon a jQuery AJAX hívás nem működik.
$inlineData = "<script type=`"text/javascript`">var __inlineJson = $json;</script>"
$html = $html -replace '(<div id="output">)', "$inlineData`n    `$1"

# $.getJSON callback -> közvetlen hívás az inline adattal
$html = $html -replace '\$\.getJSON\s*\(\s*"index\.json"\s*,\s*function\s*\(json\)\s*\{',
    '(function(json) {'

$html = $html -replace '\}\s*\)\s*\n?\s*\.fail\s*\(function[^}]+\}\s*\)\s*;',
    '})(__inlineJson);'

# Ha az előző regex nem fogta meg (formázástól függően), fallback:
$html = $html -replace '\}\)\s*\.fail\s*\(function\s*\([^)]*\)\s*\{[^}]*\}\s*\)\s*;',
    '})(__inlineJson);'

$html | Set-Content $outPath -Encoding UTF8
Write-Host "report.html generated: $outPath"
