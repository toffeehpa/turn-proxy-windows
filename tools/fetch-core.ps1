# Ядро из релиза free-turn-proxy: сверка с checksums.txt и переименование в freeturn-core.exe.
param(
    [Parameter(Mandatory)] [string] $OutDir,
    [string] $Version = (Get-Content (Join-Path $PSScriptRoot '..\core.version') -Raw).Trim(),
    [ValidateSet('amd64', 'arm64', '386')] [string] $Arch = 'amd64'
)
$ErrorActionPreference = 'Stop'

if ($Version -notmatch '^v\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?$') {
    throw "bad core version: $Version"
}
# Только официальный репозиторий ядра на github.com, без редиректов на чужие домены в адресе.
$base = "https://github.com/toffeehpa/free-turn-proxy/releases/download/$Version"
$asset = "client-windows-$Arch.exe"
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("freeturn-core-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
    Invoke-WebRequest -Uri "$base/checksums.txt" -OutFile "$tmp\checksums.txt" -UseBasicParsing
    Invoke-WebRequest -Uri "$base/$asset" -OutFile "$tmp\$asset" -UseBasicParsing

    $line = Get-Content "$tmp\checksums.txt" | Where-Object { $_ -match "^([0-9a-f]{64})\s+$([regex]::Escape($asset))$" }
    if (-not $line) {
        throw "$asset is missing in checksums.txt"
    }
    $expected = ($line -split '\s+')[0]
    $actual = (Get-FileHash "$tmp\$asset" -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) {
        throw "SHA-256 mismatch for ${asset}: expected $expected, got $actual"
    }

    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    Move-Item -Force "$tmp\$asset" (Join-Path $OutDir 'freeturn-core.exe')
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/toffeehpa/free-turn-proxy/$Version/LICENSE" `
        -OutFile (Join-Path $OutDir 'LICENSE-freeturn-core.txt') -UseBasicParsing
    Write-Host "freeturn-core.exe $Version ($Arch) sha256 $actual"
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
