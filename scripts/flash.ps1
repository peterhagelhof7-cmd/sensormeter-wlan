<#
.SYNOPSIS
  Installiert alle Abhaengigkeiten und flasht eines der drei Sensormeter-
  Firmware-Projekte (Sensormeter / Sensormeter WLAN / Sensormeter Display)
  auf einem beliebigen Windows-PC.

.DESCRIPTION
  Fragt zuerst interaktiv (oder per -Project), welches der drei
  Schwesterprojekte geflasht werden soll:
    1) Sensormeter        (WT32-ETH01, Ethernet + bis zu 2 Sensoren)
    2) Sensormeter WLAN    (generisches ESP32-WROOM-32-DevKit, WLAN-only)
    3) Sensormeter Display (ESP32-Touchdisplay, SNMP-Client)

  Anschliessend identischer Ablauf fuer alle drei:
  - Installiert Python + PlatformIO, falls nicht vorhanden (ueber winget/pip)
  - Installiert Git, falls nicht vorhanden (ueber winget)
  - Klont das gewaehlte Repository, falls unter -RepoPath noch kein
    Checkout liegt (bei vorhandenem, sauberem Checkout wird stattdessen
    "git pull" versucht)
  - Legt firmware/include/config.h aus der Vorlage an, falls das gewaehlte
    Projekt eine braucht und sie noch fehlt (wird nie ueberschrieben)
  - Baut die Firmware (pio run) zur Kontrolle
  - Flasht sie auf das per USB angeschlossene Board (pio run --target upload)

  Dieses Skript liegt identisch in allen drei Repos (scripts/flash.ps1) -
  unabhaengig davon, welches der drei Projekte gerade lokal ausgecheckt ist,
  laesst sich darueber jedes der drei flashen (die jeweils anderen beiden
  werden bei Bedarf automatisch in einen Unterordner neben dem Skript
  geklont).

  Abhaengigkeits-Erkennung ist bewusst "funktional" (ruft z.B. "python
  --version" auf und prueft die Ausgabe), nicht nur eine PATH-Pruefung:
  Windows legt standardmaessig einen "python"-Store-Alias auf PATH, der
  vorhanden aussieht, aber kein echtes Python ist - eine reine
  Get-Command-Pruefung wuerde das faelschlich als "installiert" werten.
  Aus demselben Grund wird nach einer Installation der tatsaechliche
  Funktionstest wiederholt statt sich auf den winget-Exitcode zu verlassen
  (winget liefert z.B. bei "bereits installiert, kein Update verfuegbar"
  einen Nicht-Null-Exitcode, obwohl das kein Fehler ist).

.PARAMETER Project
  "sensormeter", "wlan" oder "display". Wird weggelassen, fragt das Skript
  interaktiv nach (Eingabe von 1/2/3 oder dem Projektnamen).

.PARAMETER RepoPath
  Zielordner fuer den Checkout, falls das gewaehlte Repo dort noch nicht
  vorhanden ist. Default: ein projektspezifischer Ordnername neben diesem
  Skript (z.B. .\sensormeter-wlan).

.PARAMETER Port
  Serieller Port des Boards (z.B. COM5), falls die automatische Erkennung
  fehlschlaegt oder mehrere USB-Seriell-Adapter angeschlossen sind.

.PARAMETER SkipUpload
  Nur bauen, nicht flashen (z.B. um vorab zu pruefen, ob alles compiliert,
  ohne dass ein Board angeschlossen ist).

.EXAMPLE
  .\flash.ps1

.EXAMPLE
  .\flash.ps1 -Project wlan -Port COM5

.EXAMPLE
  .\flash.ps1 -Project display -RepoPath C:\Projekte\sensormeter-display -SkipUpload
#>

[CmdletBinding()]
param(
  [ValidateSet("sensormeter", "wlan", "display")]
  [string]$Project,
  [string]$RepoPath,
  [string]$Port,
  [switch]$SkipUpload
)

$ErrorActionPreference = "Stop"

$Projects = @{
  "sensormeter" = @{
    DisplayName = "Sensormeter (WT32-ETH01, Ethernet + bis zu 2 Sensoren)"
    RepoUrl     = "https://github.com/peterhagelhof7-cmd/sensormeter.git"
    FolderName  = "sensormeter"
    HasConfigH  = $true
    FlashNote   = "Board muss am USB-Seriell-Adapter (Debug-Burning-Schnittstelle, NICHT am 20-Pin-Hauptheader!) angeschlossen sein und sich im Boot-/Download-Modus befinden - siehe docs/flash-vorbereitung.pdf."
    SuccessNote = "Seriellen Monitor ansehen: pio device monitor (115200 Baud)."
  }
  "wlan" = @{
    DisplayName = "Sensormeter WLAN (generisches ESP32-WROOM-32-DevKit, WLAN-only)"
    RepoUrl     = "https://github.com/peterhagelhof7-cmd/sensormeter-wlan.git"
    FolderName  = "sensormeter-wlan"
    HasConfigH  = $true
    FlashNote   = "Board per USB-C-Kabel anschliessen - ggf. CP2102-/CH340-Treiber installieren."
    SuccessNote = "Beim ersten Start: WLAN ueber die Weboberflaeche einrichten - ohne gespeicherte Zugangsdaten versucht das Geraet nach 5 Minuten, dem Netz 'installer'/'installer' beizutreten (siehe docs/admin-guide.html Abschnitt 2.2)."
  }
  "display" = @{
    DisplayName = "Sensormeter Display (ESP32-Touchdisplay, HW-458B)"
    RepoUrl     = "https://github.com/peterhagelhof7-cmd/sensormeter-display.git"
    FolderName  = "sensormeter-display"
    HasConfigH  = $false
    FlashNote   = "Board per USB-Kabel anschliessen - ggf. CH340-Treiber installieren."
    SuccessNote = "Beim ersten Start: Touch-Kalibrierung durchfuehren, dann WLAN einrichten (siehe README.md)."
  }
}

function Write-Step {
  param([string]$Text)
  Write-Host ""
  Write-Host "==> $Text" -ForegroundColor Cyan
}

# ------------------------------------------------------------------
if (-not $Project) {
  Write-Host "Welches Projekt soll geflasht werden?"
  Write-Host "  1) $($Projects['sensormeter'].DisplayName)"
  Write-Host "  2) $($Projects['wlan'].DisplayName)"
  Write-Host "  3) $($Projects['display'].DisplayName)"
  do {
    $choice = Read-Host "Auswahl [1-3]"
    $Project = switch ($choice) {
      "1" { "sensormeter" }
      "2" { "wlan" }
      "3" { "display" }
      "sensormeter" { "sensormeter" }
      "wlan" { "wlan" }
      "display" { "display" }
      default { $null }
    }
    if (-not $Project) { Write-Host "Ungueltige Eingabe - bitte 1, 2, 3 oder den Projektnamen eingeben." -ForegroundColor Yellow }
  } while (-not $Project)
}

$Selected = $Projects[$Project]
Write-Step "Projekt: $($Selected.DisplayName)"

if (-not $RepoPath) {
  $RepoPath = Join-Path $PSScriptRoot $Selected.FolderName
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
  git clone $Selected.RepoUrl $RepoPath
  if ($LASTEXITCODE -ne 0) {
    throw "git clone fehlgeschlagen (Exitcode $LASTEXITCODE)"
  }
}

if (-not (Test-Path (Join-Path $firmwarePath "platformio.ini"))) {
  throw "firmware/platformio.ini wurde auch nach dem Checkout nicht gefunden - stimmt -RepoPath ($RepoPath)?"
}

# ------------------------------------------------------------------
if ($Selected.HasConfigH) {
  Write-Step "Pruefe config.h..."
  $configExample = Join-Path $firmwarePath "include\config.h.example"
  $configReal = Join-Path $firmwarePath "include\config.h"

  if (-not (Test-Path $configReal)) {
    Copy-Item $configExample $configReal
    Write-Host "include/config.h aus der Vorlage angelegt."
  } else {
    Write-Host "config.h existiert bereits - wird nicht ueberschrieben."
  }
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
  Write-Host "  (keine gefunden - ist das Board per USB angeschlossen?)" -ForegroundColor Yellow
} else {
  $ports | ForEach-Object { Write-Host "  - $_" }
}

Write-Host ""
Write-Host "Hinweis: $($Selected.FlashNote)" -ForegroundColor Yellow

Write-Step "Flashe Firmware (pio run --target upload)..."
if ($Port) {
  pio run --target upload --upload-port $Port
} else {
  pio run --target upload
}

if ($LASTEXITCODE -ne 0) {
  Write-Host ""
  Write-Host "Upload fehlgeschlagen. Haeufige Ursachen:" -ForegroundColor Red
  Write-Host "  - Falscher COM-Port (siehe Liste oben, ggf. mit -Port COM<n> erneut versuchen)"
  Write-Host "  - CH340-/CP2102-USB-Treiber fehlt (Geraete-Manager pruefen)"
  Write-Host "  - Board nicht angeschlossen oder nicht im Bootloader-/Download-Modus"
  throw "pio run --target upload fehlgeschlagen (Exitcode $LASTEXITCODE)"
}

Write-Host ""
Write-Host "Fertig! Firmware erfolgreich geflasht." -ForegroundColor Green
Write-Host $Selected.SuccessNote
