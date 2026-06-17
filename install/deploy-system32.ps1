# Model-A install: place the Chroma shim system-wide so any Chroma game resolves
# RzChromaSDK64.dll / RzChromatic64.dll (64-bit, from System32) or RzChromaSDK.dll /
# RzChromatic.dll (32-bit, from SysWOW64) with no per-game drop. Must run elevated.
param([string]$DistDir = (Join-Path $PSScriptRoot '..\dist'))
$ErrorActionPreference = 'Stop'
$logPath = Join-Path $PSScriptRoot '..\sys32-deploy.log'
try {
    # 64-bit games load from System32
    Copy-Item (Join-Path $DistDir 'RzChromaSDK64.dll') 'C:\Windows\System32\RzChromaSDK64.dll' -Force
    Copy-Item (Join-Path $DistDir 'RzChromatic64.dll') 'C:\Windows\System32\RzChromatic64.dll' -Force
    $msg = "x64 System32: SDK64=" + (Test-Path 'C:\Windows\System32\RzChromaSDK64.dll')
    # 32-bit games (e.g. Dead Cells) load from SysWOW64
    if (Test-Path (Join-Path $DistDir 'RzChromaSDK.dll')) {
        Copy-Item (Join-Path $DistDir 'RzChromaSDK.dll') 'C:\Windows\SysWOW64\RzChromaSDK.dll' -Force
        Copy-Item (Join-Path $DistDir 'RzChromatic.dll') 'C:\Windows\SysWOW64\RzChromatic.dll' -Force
        $msg += " | x86 SysWOW64: SDK=" + (Test-Path 'C:\Windows\SysWOW64\RzChromaSDK.dll')
    } else { $msg += " | x86: dist DLL missing (run build32.bat)" }
} catch { $msg = "ERR " + $_.Exception.Message }
Set-Content -Path $logPath -Value $msg
