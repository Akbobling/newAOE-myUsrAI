$ErrorActionPreference = "Continue"
& "C:\Qt\Qt5.9.2\Tools\mingw530_32\bin\g++.exe" -fsyntax-only -std=gnu++1y -I. -IC:\Qt\Qt5.9.2\5.9.2\mingw53_32\include -IC:\Qt\Qt5.9.2\5.9.2\mingw53_32\include\QtWidgets -IC:\Qt\Qt5.9.2\5.9.2\mingw53_32\include\QtGui -IC:\Qt\Qt5.9.2\5.9.2\mingw53_32\include\QtNetwork -IC:\Qt\Qt5.9.2\5.9.2\mingw53_32\include\QtCore -Idebug UsrAI.cpp 2> compile_err.txt
Write-Host "gxx_exit=$LASTEXITCODE"
if (Test-Path compile_err.txt) {
    $lines = Get-Content compile_err.txt | Select-Object -First 60
    $lines | ForEach-Object { Write-Host $_ }
}
