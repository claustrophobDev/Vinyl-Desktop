# Собирает портативную папку: exe, удалялку и ядро рядом.
# Запускать из корня проекта:  powershell -ExecutionPolicy Bypass -File tools\pack.ps1

param(
    [string]$Build = "build",
    [string]$Out = "dist\Vinyl",
    [string]$Deps = "E:\dev\deps"
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $Build "vinyl.exe"
if (-not (Test-Path $exe)) {
    Write-Host "сначала собери проект: cmake --build $Build"
    exit 1
}

# папку собираем заново, чтобы не тащить старые файлы
if (Test-Path $Out) { Remove-Item $Out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Out "core") | Out-Null

Copy-Item $exe $Out
Copy-Item (Join-Path $Build "uninstall.exe") $Out

$singbox = Join-Path $Deps "sing-box-1.14.0-windows-amd64\sing-box.exe"
$wintun = Join-Path $Deps "wintun\bin\amd64\wintun.dll"
foreach ($file in @($singbox, $wintun)) {
    if (-not (Test-Path $file)) {
        Write-Host "нет зависимости: $file"
        exit 1
    }
    Copy-Item $file (Join-Path $Out "core")
}

# папка data появится сама при первом запуске, но пусть будет сразу
New-Item -ItemType Directory -Force -Path (Join-Path $Out "data") | Out-Null

$size = (Get-ChildItem $Out -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ("готово: {0}  ({1:N0} МБ)" -f $Out, $size)
Get-ChildItem $Out -Recurse -File | ForEach-Object {
    "  " + $_.FullName.Substring((Resolve-Path $Out).Path.Length + 1)
}
