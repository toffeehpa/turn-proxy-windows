# wintun.dll для -tun ядра: официальный архив wintun.net, сверка с закреплённым SHA-256.
param(
    [Parameter(Mandatory)] [string] $OutDir,
    [ValidateSet('amd64', 'arm64', '386')] [string] $Arch = 'amd64'
)
$ErrorActionPreference = 'Stop'

$version = '0.14.1'
$sha256 = '07c256185d6ee3652e09fa55c0b673e2624b565e02c4b9091c79ca7d2f24ef51'
$dir = @{ amd64 = 'amd64'; arm64 = 'arm64'; '386' = 'x86' }[$Arch]
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("wintun-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $tmp | Out-Null
try {
    $zip = "$tmp\wintun-$version.zip"
    Invoke-WebRequest -Uri "https://www.wintun.net/builds/wintun-$version.zip" -OutFile $zip -UseBasicParsing
    $actual = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $sha256) {
        throw "SHA-256 mismatch for wintun-${version}.zip: expected $sha256, got $actual"
    }
    Expand-Archive -Path $zip -DestinationPath $tmp

    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    Copy-Item "$tmp\wintun\bin\$dir\wintun.dll" (Join-Path $OutDir 'wintun.dll')
    Copy-Item "$tmp\wintun\LICENSE.txt" (Join-Path $OutDir 'LICENSE-wintun.txt')
    Write-Host "wintun.dll $version ($Arch)"
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
