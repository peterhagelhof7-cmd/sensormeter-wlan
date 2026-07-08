<#
.SYNOPSIS
  Installiert alle Abhaengigkeiten und flasht die Sensormeter-WLAN-Firmware
  (generisches ESP32-WROOM-32-DevKit) auf einem beliebigen Windows-PC.

.DESCRIPTION
  - Installiert Python + PlatformIO, falls nicht vorhanden (ueber winget/pip)
  - Installiert Git, falls nicht vorhanden (ueber winget)
  - Klont https://github.com/peterhagelhof7-cmd/sensormeter-wlan, falls
    unter -RepoPath noch kein Checkout liegt (bei vorhandenem, sauberem
    Checkout wird stattdessen "git pull" versucht)
  - Legt include/config.h aus config.h.example an, falls sie noch fehlt
  - Baut die Firmware (pio run) zur Kontrolle
  - Flasht sie auf das per USB angeschlossene Board (pio run --target upload)

  Es gibt keine WLAN-Zugangsdaten in config.h - diese werden ausschliesslich
  ueber die Weboberflaeche eingerichtet (siehe docs/admin-guide.html
  Abschnitt 2). Ohne gespeicherte Zugangsdaten versucht das Geraet nach
  5 Minuten, einem Netz namens "installer"/"installer" beizutreten (siehe
  docs/admin-guide.html Abschnitt 2.2 zu dieser bekannten Einschraenkung).

  Abhaengigkeits-Erkennung ist bewusst "funktional" (ruft z.B. "python
  --version" auf und prueft die Ausgabe), nicht nur eine PATH-Pruefung:
  Windows legt standardmaessig einen "python"-Store-Alias auf PATH, der
  vorhanden aussieht, aber kein echtes Python ist - eine reine
  Get-Command-Pruefung wuerde das faelschlich als "installiert" werten.
  Aus demselben Grund wird nach einer Installation der tatsaechliche
  Funktionstest wiederholt statt sich auf den winget-Exitcode zu verlassen
  (winget liefert z.B. bei "bereits installiert, kein Update verfuegbar"
  einen Nicht-Null-Exitcode, obwohl das kein Fehler ist).

.PARAMETER RepoPath
  Zielordner fuer den Checkout, falls das Repo noch nicht vorhanden ist.
  Default: .\sensormeter-wlan neben diesem Skript.

.PARAMETER Port
  Serieller Port des Boards (z.B. COM5), falls die automatische Erkennung
  fehlschlaegt oder mehrere USB-Seriell-Adapter angeschlossen sind.

.PARAMETER SkipUpload
  Nur bauen, nicht flashen (z.B. um vorab zu pruefen, ob alles compiliert,
  ohne dass ein Board angeschlossen ist).

.EXAMPLE
  .\flash-sensormeter-wlan.ps1

.EXAMPLE
  .\flash-sensormeter-wlan.ps1 -Port COM5

.EXAMPLE
  .\flash-sensormeter-wlan.ps1 -RepoPath C:\Projekte\sensormeter-wlan -SkipUpload
#>

[CmdletBinding()]
param(
  [string]$RepoPath = (Join-Path $PSScriptRoot "sensormeter-wlan"),
  [string]$Port,
  [switch]$SkipUpload
)

$ErrorActionPreference = "Stop"
$RepoUrl = "https://github.com/peterhagelhof7-cmd/sensormeter-wlan.git"

function Write-Step {
  param([string]$Text)
  Write-Host ""
  Write-Host "==> $Text" -ForegroundColor Cyan
}

function Update-SessionPath {
  $machinePath = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
  $userPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
  $env:Path = "$machinePath;$userPath"
}

# Funktionale Pruefung statt reiner PATH-Pruefung (siehe .DESCRIPTION oben:
# der Windows-eigene "python"-Store-Alias sieht fuer Get-Command vorhanden
# aus, ist aber kein echtes Python).
function Test-ToolWorks {
  param(
    [string]$Command,
    [string]$Pattern
  )
  try {
    $output = & $Command --version 2>&1 | Out-String
  } catch {
    return $false
  }
  if ($LASTEXITCODE -ne 0) { return $false }
  return ($output -match $Pattern)
}

function Install-ToolIfMissing {
  param(
    [string]$Name,
    [string]$Command,
    [string]$Pattern,
    [scriptblock]$InstallAction
  )
  Write-Step "Pruefe $Name..."
  if (Test-ToolWorks -Command $Command -Pattern $Pattern) {
    Write-Host "OK: $Name ist bereits vorhanden und funktionsfaehig."
    return
  }

  Write-Host "$Name nicht gefunden oder nicht funktionsfaehig - installiere..."
  & $InstallAction
  Update-SessionPath

  if (-not (Test-ToolWorks -Command $Command -Pattern $Pattern)) {
    throw "$Name ist nach der Installation weiterhin nicht nutzbar. Bitte Terminal/PowerShell neu starten und Skript erneut ausfuehren (PATH-Aenderungen greifen sonst erst in einer neuen Sitzung)."
  }
  Write-Host "OK: $Name einsatzbereit."
}

# ------------------------------------------------------------------
Install-ToolIfMissing -Name "Python" -Command "python" -Pattern "^Python \d" -InstallAction {
  winget install --id Python.Python.3.12 -e --source winget --accept-package-agreements --accept-source-agreements
}

# ------------------------------------------------------------------
Install-ToolIfMissing -Name "Git" -Command "git" -Pattern "^git version" -InstallAction {
  winget install --id Git.Git -e --source winget --accept-package-agreements --accept-source-agreements
}

# ------------------------------------------------------------------
Install-ToolIfMissing -Name "PlatformIO" -Command "pio" -Pattern "PlatformIO Core" -InstallAction {
  python -m pip install --upgrade pip
  python -m pip install --upgrade platformio
}

# ------------------------------------------------------------------
Write-Step "Pruefe Projekt-Checkout..."
$firmwarePath = Join-Path $RepoPath "firmware"

if (Test-Path (Join-Path $firmwarePath "platformio.ini")) {
  Write-Host "Repo bereits vorhanden unter $RepoPath"
  $status = git -C $RepoPath status --porcelain
  if ([string]::IsNullOrWhiteSpace($status)) {
    Write-Host "Keine lokalen Aenderungen - hole neueste Version (git pull)..."
    git -C $RepoPath pull
  } else {
    Write-Host "Lokale Aenderungen im Checkout gefunden - ueberspringe 'git pull', um nichts zu ueberschreiben." -ForegroundColor Yellow
  }
} else {
  Write-Step "Klone Repository nach $RepoPath ..."
  git clone $RepoUrl $RepoPath
  if ($LASTEXITCODE -ne 0) {
    throw "git clone fehlgeschlagen (Exitcode $LASTEXITCODE)"
  }
}

if (-not (Test-Path (Join-Path $firmwarePath "platformio.ini"))) {
  throw "firmware/platformio.ini wurde auch nach dem Checkout nicht gefunden - stimmt -RepoPath ($RepoPath)?"
}

# ------------------------------------------------------------------
$configPath = Join-Path $firmwarePath "include\config.h"
$configExamplePath = Join-Path $firmwarePath "include\config.h.example"
if (-not (Test-Path $configPath)) {
  Write-Step "Lege include/config.h an (aus config.h.example)..."
  Copy-Item $configExamplePath $configPath
}

# ------------------------------------------------------------------
Set-Location $firmwarePath

Write-Step "Baue Firmware (pio run)..."
pio run
if ($LASTEXITCODE -ne 0) {
  throw "Build fehlgeschlagen (pio-Exitcode $LASTEXITCODE)"
}

if ($SkipUpload) {
  Write-Host ""
  Write-Host "SkipUpload gesetzt - Build erfolgreich, kein Flash-Vorgang durchgefuehrt." -ForegroundColor Green
  exit 0
}

# ------------------------------------------------------------------
Write-Step "Verfuegbare serielle Ports:"
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if ($ports.Count -eq 0) {
  Write-Host "  (keine gefunden - ist das Board per USB-C angeschlossen?)" -ForegroundColor Yellow
} else {
  $ports | ForEach-Object { Write-Host "  - $_" }
}

Write-Host ""
Write-Host "Hinweis: Das Board per USB-C-Kabel anschliessen - ggf. muss der" -ForegroundColor Yellow
Write-Host "CP2102- oder CH340-Treiber installiert sein, damit Windows den" -ForegroundColor Yellow
Write-Host "USB-Seriell-Adapter erkennt (siehe docs/admin-guide.html)." -ForegroundColor Yellow

Write-Step "Flashe Firmware (pio run --target upload)..."
if ($Port) {
  pio run --target upload --upload-port $Port
} else {
  pio run --target upload
}

if ($LASTEXITCODE -ne 0) {
  Write-Host ""
  Write-Host "Upload fehlgeschlagen. Haeufige Ursachen:" -ForegroundColor Red
  Write-Host "  - CP2102-/CH340-Treiber fehlt (siehe Geraete-Manager)"
  Write-Host "  - Falscher COM-Port (siehe Liste oben, ggf. mit -Port COM<n> erneut versuchen)"
  Write-Host "  - Board nicht angeschlossen oder nicht im Bootloader-Modus (BOOT-Taste waehrend des Uploads gedrueckt halten, falls kein Auto-Reset-Chip verbaut ist)"
  throw "pio run --target upload fehlgeschlagen (Exitcode $LASTEXITCODE)"
}

Write-Host ""
Write-Host "Fertig! Firmware erfolgreich geflasht." -ForegroundColor Green
Write-Host "Beim ersten Start: WLAN ueber die Weboberflaeche einrichten - ohne gespeicherte Zugangsdaten versucht das Geraet nach 5 Minuten, dem Netz 'installer'/'installer' beizutreten (siehe docs/admin-guide.html Abschnitt 2.2 fuer Details/bekannte Einschraenkung)."
Write-Host "Seriellen Monitor ansehen: pio device monitor  (im Ordner $firmwarePath, 115200 Baud)"
