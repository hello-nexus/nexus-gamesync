# Model-A install: place the capture shims system-wide so any game resolves them
# by bare name. 64-bit games load from System32; 32-bit games load from SysWOW64.
# Shims: Razer Chroma (RzChromaSDK*/RzChromatic*), Alienware/Dell LightFX
# (LightFX.dll), Logitech (LogitechLedEnginesWrapper.dll + LogitechLed.dll).
# x64 DLLs live in dist\; x86 DLLs live in dist\x86\ (LightFX/Logitech share
# their names across arches, so the arches are kept in separate dirs). Run elevated.
param(
    [string]$DistDir = (Join-Path $PSScriptRoot '..\dist'),
    [string]$Dist32Dir = (Join-Path $PSScriptRoot '..\dist\x86')
)
$ErrorActionPreference = 'Stop'
$logPath = Join-Path $PSScriptRoot '..\sys32-deploy.log'

$x64 = @('RzChromaSDK64.dll', 'RzChromatic64.dll', 'LightFX.dll', 'LogitechLedEnginesWrapper.dll', 'LogitechLed.dll')
$x86 = @('RzChromaSDK.dll', 'RzChromatic.dll', 'LightFX.dll', 'LogitechLedEnginesWrapper.dll', 'LogitechLed.dll')

try {
    foreach ($f in $x64) { Copy-Item (Join-Path $DistDir $f) (Join-Path 'C:\Windows\System32' $f) -Force }
    $msg = "x64 System32: " + (($x64 | ForEach-Object { Test-Path (Join-Path 'C:\Windows\System32' $_) }) -join ',')
    if (Test-Path (Join-Path $Dist32Dir 'RzChromaSDK.dll')) {
        foreach ($f in $x86) { Copy-Item (Join-Path $Dist32Dir $f) (Join-Path 'C:\Windows\SysWOW64' $f) -Force }
        $msg += " | x86 SysWOW64: " + (($x86 | ForEach-Object { Test-Path (Join-Path 'C:\Windows\SysWOW64' $_) }) -join ',')
    } else { $msg += " | x86: dist\x86 DLLs missing (run build32.bat)" }
} catch { $msg = "ERR " + $_.Exception.Message }
Set-Content -Path $logPath -Value $msg
