param(
    [ValidateSet("build", "usb", "ota", "logs")]
    [string]$Mode = "build",

    [string]$Port = "COM4",
    [string]$Device = "esp32-wifi-term.local",
    [string]$Config = "esp32-wifi-term.yaml"
)

$ErrorActionPreference = "Stop"

function Invoke-ESPHome {
    param([string[]]$Args)

    $cmd = Get-Command esphome -ErrorAction SilentlyContinue
    if ($null -ne $cmd) {
        & esphome @Args
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        return
    }

    # Fallback to the venv used earlier in this project.
    $venvPython = "C:\esphome\Scripts\python.exe"
    if (Test-Path $venvPython) {
        & $venvPython -m esphome @Args
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        return
    }

    throw "ESPHome CLI not found. Install it with: py -m pip install esphome"
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

switch ($Mode) {
    "build" {
        Write-Host "==> Compiling $Config" -ForegroundColor Cyan
        Invoke-ESPHome @("compile", $Config)
    }
    "usb" {
        Write-Host "==> Building and flashing $Config to $Port" -ForegroundColor Cyan
        Invoke-ESPHome @("run", $Config, "--device", $Port)
    }
    "ota" {
        Write-Host "==> Building and flashing $Config over OTA to $Device" -ForegroundColor Cyan
        Invoke-ESPHome @("run", $Config, "--device", $Device)
    }
    "logs" {
        Write-Host "==> Opening logs from $Device" -ForegroundColor Cyan
        Invoke-ESPHome @("logs", $Config, "--device", $Device)
    }
}
