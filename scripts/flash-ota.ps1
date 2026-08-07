param([string]$Device = "esp32-wifi-term.local")
$ErrorActionPreference = "Stop"
& "$PSScriptRoot\esp.ps1" ota -Device $Device
