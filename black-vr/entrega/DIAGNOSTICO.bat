@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set REL=RELATORIO-VR.txt

rem ---- trava de versao: se o vr-sonda.exe nao esta aqui, este .bat e o ANTIGO.
if not exist "port\vr-sonda.exe" goto :antigo

echo ============================================================
echo   BLACK VR - DIAGNOSTICO DA VERSAO 9
echo   (se este titulo nao aparecer, o arquivo e o antigo)
echo ============================================================
echo.

echo === BLACK VR - diagnostico v9 === > "%REL%"
echo data: %DATE%  %TIME% >> "%REL%"
echo pasta: %CD% >> "%REL%"
echo. >> "%REL%"

if exist "game\default.xbe" (echo game\default.xbe: SIM >> "%REL%") else (echo game\default.xbe: NAO >> "%REL%")
if exist "port\black.exe" (echo port\black.exe: SIM >> "%REL%") else (echo port\black.exe: NAO >> "%REL%")
if exist "port\blackrt.dll" (echo port\blackrt.dll: SIM >> "%REL%") else (echo port\blackrt.dll: NAO >> "%REL%")
if exist "port\openxr_loader.dll" (echo port\openxr_loader.dll: SIM >> "%REL%") else (echo port\openxr_loader.dll: NAO >> "%REL%")
if exist "port\vr-sonda.exe" (echo port\vr-sonda.exe: SIM >> "%REL%") else (echo port\vr-sonda.exe: NAO >> "%REL%")
echo. >> "%REL%"

rem ---- 1. teste separado do jogo: 9 modos, cada um num programa proprio.
rem     Se um modo cair, os outros continuam: e assim que se descobre qual caminho funciona.
echo [TESTE SEPARADO DO JOGO] >> "%REL%"
for %%V in (4 3 6 9 1 7 8 2 5) do call :rodar %%V
echo. >> "%REL%"

rem ---- 2. junta as saidas dos modos
for %%V in (4 3 6 9 1 7 8 2 5) do (
  if exist "port\sonda-%%V.txt" (
    echo. >> "%REL%"
    type "port\sonda-%%V.txt" >> "%REL%"
    del "port\sonda-%%V.txt" >nul 2>&1
  )
)
echo. >> "%REL%"

rem ---- 3. o jogo (apaga os rastros velhos antes)
if exist "port\vr-trace.txt" del "port\vr-trace.txt" >nul 2>&1
if exist "port\black.log" del "port\black.log" >nul 2>&1
set XR_LOADER_DEBUG=
set XR_DEBUG_FILE=
if exist "port\blackrt.dll" (
  echo [JOGO] >> "%REL%"
  powershell -NoProfile -Command "Get-ChildItem -Path '%~dp0port' -Filter *.dll | Unblock-File -ErrorAction SilentlyContinue; Get-ChildItem -Path '%~dp0port' -Filter *.exe | Unblock-File -ErrorAction SilentlyContinue" >nul 2>&1
  pushd port
  black.exe --vr-selftest 12 > jogo-saida.txt 2>&1
  set CODE=!ERRORLEVEL!
  popd
  echo [resultado] codigo de saida: !CODE!   ^(0 = ok^) >> "%REL%"
  if exist "port\jogo-saida.txt" (
    echo. >> "%REL%"
    echo [TEXTO DO JOGO] >> "%REL%"
    type "port\jogo-saida.txt" >> "%REL%"
    del "port\jogo-saida.txt" >nul 2>&1
  )
  echo. >> "%REL%"
  echo [RASTRO port\vr-trace.txt] >> "%REL%"
  if exist "port\vr-trace.txt" (type "port\vr-trace.txt" >> "%REL%") else (echo ^(nao foi criado^) >> "%REL%")
  echo. >> "%REL%"
  echo [LOG port\black.log] >> "%REL%"
  if exist "port\black.log" (
    powershell -NoProfile -Command "$l = Get-Content '%~dp0port\black.log'; if ($l.Count -gt 120) { $l[-120..-1] } else { $l }" >> "%REL%" 2>nul
  ) else (
    echo ^(vazio^) >> "%REL%"
  )
)
echo. >> "%REL%"
echo === fim === >> "%REL%"

type "%REL%"
echo.
echo ------------------------------------------------------------
echo Relatorio salvo em: %CD%\%REL%
echo Manda esse arquivo (ou o texto acima) de volta.
echo ------------------------------------------------------------
pause
exit /b

:rodar
set V=%1
echo   - modo %V% ... >> "%REL%"
if not exist "port\vr-sonda.exe" exit /b
del "port\xr-loader.log" >nul 2>&1
pushd port
set XR_LOADER_DEBUG=all
set XR_DEBUG_FILE=%CD%\xr-loader.log
vr-sonda.exe %V% > sonda-console-%V%.txt 2>&1
set RC=!ERRORLEVEL!
set XR_LOADER_DEBUG=
set XR_DEBUG_FILE=
popd
echo     codigo !RC! >> "%REL%"
if exist "port\sonda-console-%V%.txt" (
  type "port\sonda-console-%V%.txt" >> "%REL%"
  del "port\sonda-console-%V%.txt" >nul 2>&1
)
if exist "port\xr-loader.log" (
  echo     --- log do conector (inicio) --- >> "%REL%"
  powershell -NoProfile -Command "Get-Content '%~dp0port\xr-loader.log' -TotalCount 40" >> "%REL%" 2>nul
  del "port\xr-loader.log" >nul 2>&1
)
exit /b

:antigo
echo.
echo ************************************************************
echo   ESTE DIAGNOSTICO E O ANTIGO - NAO SERVE MAIS.
echo.
echo   Falta o arquivo:  port\vr-sonda.exe
echo   Ele vem no pacote NOVO (BLACK-VR-pronto-v9.zip).
echo.
echo   O que fazer:
echo     1. Baixe o pacote novo pelo link que eu mandei.
echo     2. Abra o zip e clique em "Extrair tudo".
echo     3. Escolha ESTA pasta como destino:
echo        %CD%
echo     4. Se ele perguntar, escolha "Substituir os arquivos".
echo     5. Rode o DIAGNOSTICO.bat de novo.
echo ************************************************************
echo.
pause
exit /b
