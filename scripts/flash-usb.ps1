param([string]$Port = "COM4")
$ErrorActionPreference = "Stop"
& "$PSScriptRoot\esp.ps1" usb -Port $Port
