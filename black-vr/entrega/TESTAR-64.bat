@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set REL=RELATORIO-64.txt

rem ---- sem as janelas de "o programa parou de funcionar" (elas travavam o teste)
reg add "HKCU\Software\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f >nul 2>&1

if not exist "vr-sonda64.exe" goto :faltou
if not exist "openxr_loader.dll" goto :faltou

echo ============================================================
echo   BLACK VR - TESTE DE 64 BITS (versao 11)
echo ============================================================
echo.
echo   1. Abra o aplicativo do oculos (Meta Horizon Link).
echo   2. Oculos ligado no cabo.
echo   3. COLOQUE O OCULOS NA CABECA: vao aparecer imagens nele.
echo.
echo   Nao precisa do jogo nesta pasta. E so o teste do oculos.
echo.
pause

echo === BLACK VR - teste de 64 bits, versao 11 === > "%REL%"
echo data: %DATE%  %TIME% >> "%REL%"
echo pasta: %CD% >> "%REL%"
echo. >> "%REL%"

powershell -NoProfile -Command "Get-ChildItem -Path '%~dp0' -Include *.dll,*.exe -Recurse | Unblock-File -ErrorAction SilentlyContinue" >nul 2>&1

echo [TESTES] >> "%REL%"
for %%V in (4 12 11 10) do call :rodar %%V
echo. >> "%REL%"

set VIU=
set /p VIU=Apareceu algum quadro colorido no oculos? (s = sim, n = nao): 
echo resposta do usuario (apareceu imagem no oculos?): !VIU! >> "%REL%"
echo. >> "%REL%"
echo === fim === >> "%REL%"

echo.
echo ------------------------------------------------------------
echo Relatorio salvo em: %CD%\%REL%
echo Me manda esse arquivo (ou o texto acima).
echo ------------------------------------------------------------
pause
exit /b

rem ============================================================
rem  Cada teste tem tempo limite: se travar, e encerrado e o
rem  relatorio continua.
rem ============================================================
:rodar
set V=%1
echo. >> "%REL%"
echo   - modo %V% ... >> "%REL%"
if not exist "vr-sonda64.exe" exit /b
if exist "xr-loader64.log" del "xr-loader64.log" >nul 2>&1
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$env:XR_LOADER_DEBUG='all'; $env:XR_DEBUG_FILE=(Join-Path (Get-Location) 'xr-loader64.log');" ^
  "$p = Start-Process -FilePath '.\vr-sonda64.exe' -ArgumentList %V% -PassThru -NoNewWindow " ^
  "  -RedirectStandardOutput 'console-%V%.txt' -RedirectStandardError 'erro-%V%.txt';" ^
  "if ($p.WaitForExit(150000)) { '    codigo ' + $p.ExitCode } else { $p.Kill(); '    TRAVOU: encerrado depois de 150 s' }" >> "%REL%" 2>&1

if exist "sonda64-%V%.txt" (
  type "sonda64-%V%.txt" >> "%REL%"
  del "sonda64-%V%.txt" >nul 2>&1
)
if exist "console-%V%.txt" (
  echo     --- saida da janela --- >> "%REL%"
  type "console-%V%.txt" >> "%REL%"
  del "console-%V%.txt" >nul 2>&1
)
if exist "erro-%V%.txt" (
  echo     --- erros --- >> "%REL%"
  type "erro-%V%.txt" >> "%REL%"
  del "erro-%V%.txt" >nul 2>&1
)
if exist "xr-loader64.log" (
  echo     --- log do conector (inicio) --- >> "%REL%"
  powershell -NoProfile -Command "Get-Content '%~dp0xr-loader64.log' -TotalCount 30" >> "%REL%" 2>nul
  del "xr-loader64.log" >nul 2>&1
)
exit /b

:faltou
echo.
echo ************************************************************
echo   FALTAM ARQUIVOS NESTA PASTA.
echo.
echo   Tem de ter, do lado deste .bat:
echo      vr-sonda64.exe
echo      openxr_loader.dll
echo.
echo   Baixe os dois da pasta do Drive e jogue nesta mesma pasta.
echo ************************************************************
echo.
pause
exit /b
