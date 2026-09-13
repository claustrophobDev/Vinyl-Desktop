# Собирает готовую к раздаче папку: outputs\Vinyl-<версия>\ и архив рядом.
# Внутри уже все что нужно - exe, удалялка и ядро. Распаковал и запустил.
#
#   powershell -ExecutionPolicy Bypass -File tools\pack.ps1
#
# Версию берем из src\app\Install.h, чтобы она была в одном месте.

param(
    [string]$Build = "build-release",
    [string]$Deps = "E:\dev\deps",
    [switch]$NoBuild,   # не пересобирать, взять что уже лежит
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# --- версия ---
$header = Get-Content "src\app\Install.h" -Raw
if ($header -match 'kVersion\s*=\s*"([^"]+)"') {
    $version = $Matches[1]
} else {
    Write-Host "не нашел версию в src\app\Install.h"
    exit 1
}
Write-Host "версия $version"

# --- сборка ---
# релиз обязательно с правами администратора, без них не поднимется tun-интерфейс
if (-not $NoBuild) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Host "не нашел vswhere, запусти из Developer PowerShell или собери сам"
        exit 1
    }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        Write-Host "не нашел vcvars64.bat"
        exit 1
    }

    # генератор задаем явно: по умолчанию берется Visual Studio, а он раскладывает exe
    # по подпапкам конфигурации и лезет со своим манифестом
    if (Test-Path "$Build\CMakeCache.txt") {
        $cache = Get-Content "$Build\CMakeCache.txt" -Raw
        if ($cache -notmatch 'CMAKE_GENERATOR:INTERNAL=NMake Makefiles') {
            Remove-Item $Build -Recurse -Force
        }
    }

    $cmd = "call `"$vcvars`" >nul 2>&1 && cmake -S . -B $Build -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release -DVINYL_REQUIRE_ADMIN=ON -DVINYL_DEPS_DIR=`"$($Deps -replace '\\','/')`" && cmake --build $Build"
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) {
        Write-Host "сборка упала"
        exit 1
    }
}

$exe = Join-Path $Build "vinyl.exe"
$uninstall = Join-Path $Build "uninstall.exe"
foreach ($file in @($exe, $uninstall)) {
    if (-not (Test-Path $file)) {
        Write-Host "нет файла: $file"
        exit 1
    }
}

# --- раскладываем папку ---
$out = "outputs\Vinyl-$version"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $out | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $out "core") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $out "data") | Out-Null

Copy-Item $exe $out
Copy-Item $uninstall $out

$singbox = Join-Path $Deps "sing-box-1.14.0-windows-amd64\sing-box.exe"
$wintun = Join-Path $Deps "wintun\bin\amd64\wintun.dll"
foreach ($file in @($singbox, $wintun)) {
    if (-not (Test-Path $file)) {
        Write-Host "нет зависимости: $file"
        exit 1
    }
    Copy-Item $file (Join-Path $out "core")
}

# короткая инструкция для того кто распакует
$readme = @"
Vinyl $version

Как пользоваться:
1. Распакуйте всю папку куда угодно, хоть на флешку.
2. Запустите vinyl.exe. Windows спросит права администратора - они нужны,
   без них не получится создать сетевой адаптер для VPN.
3. Скопируйте ссылку на подписку или ключ (vless://, trojan://, ss:// и другие)
   и нажмите "Вставить из буфера" на вкладке "Серверы".
4. Нажмите на пластинку на главной, чтобы подключиться.

Всё лежит в этой же папке:
  data\        настройки, список серверов и логи
  core\        ядро sing-box и драйвер сетевого адаптера
  uninstall.exe убирает Vinyl из списка программ и меню пуск

Программа ничего не устанавливает в систему. Чтобы удалить полностью,
запустите uninstall.exe и удалите эту папку.

Developer claustrophobDev
"@
# UTF8 тут пишет метку в начале файла, иначе блокнот покажет кракозябры вместо русского
Set-Content -Path (Join-Path $out "Как пользоваться.txt") -Value $readme -Encoding UTF8

# --- архив ---
if (-not $NoZip) {
    $zip = "outputs\Vinyl-$version.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path $out -DestinationPath $zip
}

$size = (Get-ChildItem $out -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ""
Write-Host ("готово: {0}  ({1:N0} МБ)" -f $out, $size)
Get-ChildItem $out -Recurse -File | ForEach-Object {
    "  " + $_.FullName.Substring((Resolve-Path $out).Path.Length + 1)
}
if (-not $NoZip) {
    $zipSize = (Get-Item "outputs\Vinyl-$version.zip").Length / 1MB
    Write-Host ""
    Write-Host ("архив: outputs\Vinyl-$version.zip  ({0:N0} МБ)" -f $zipSize)
}

