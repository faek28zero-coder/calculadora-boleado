@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set REL=RELATORIO-VR.txt

rem ---- trava de versao: sem os dois testes, este .bat e o ANTIGO.
if not exist "port\vr-sonda.exe" goto :antigo
if not exist "port\x64\vr-sonda64.exe" goto :antigo

rem ---- sem as janelas de "o programa parou de funcionar": elas travavam o teste
reg add "HKCU\Software\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f >nul 2>&1

echo ============================================================
echo   BLACK VR - DIAGNOSTICO DA VERSAO 11
echo   64 bits primeiro, depois 32 bits
echo   (se este titulo nao aparecer, o arquivo e o antigo)
echo ============================================================
echo.

echo === BLACK VR - diagnostico v11 (64 bits + 32 bits) === > "%REL%"
echo data: %DATE%  %TIME% >> "%REL%"
echo pasta: %CD% >> "%REL%"
echo. >> "%REL%"

if exist "game\default.xbe" (echo game\default.xbe: SIM >> "%REL%") else (echo game\default.xbe: NAO >> "%REL%")
if exist "port\black.exe" (echo port\black.exe: SIM >> "%REL%") else (echo port\black.exe: NAO >> "%REL%")
if exist "port\blackrt.dll" (echo port\blackrt.dll: SIM >> "%REL%") else (echo port\blackrt.dll: NAO >> "%REL%")
if exist "port\openxr_loader.dll" (echo port\openxr_loader.dll: SIM >> "%REL%") else (echo port\openxr_loader.dll: NAO >> "%REL%")
if exist "port\vr-sonda.exe" (echo port\vr-sonda.exe: SIM >> "%REL%") else (echo port\vr-sonda.exe: NAO >> "%REL%")
if exist "port\x64\vr-sonda64.exe" (echo port\x64\vr-sonda64.exe: SIM >> "%REL%") else (echo port\x64\vr-sonda64.exe: NAO >> "%REL%")
if exist "port\x64\openxr_loader.dll" (echo port\x64\openxr_loader.dll: SIM >> "%REL%") else (echo port\x64\openxr_loader.dll: NAO >> "%REL%")
echo. >> "%REL%"

echo  ============================================================
echo    COLOQUE O OCULOS NA CABECA AGORA
echo.
echo    Os testes de 64 bits vao tentar desenhar no oculos:
echo    um quadro colorido mudando de cor, vermelho de um lado
echo    e verde do outro. Se voce ver isso, deu certo.
echo  ============================================================
echo.
pause

rem ---- 1. versao de 64 bits: o caminho novo
echo [TESTE 64 BITS - o caminho novo] >> "%REL%"
for %%V in (4 12 11 10) do call :rodar64 %%V
echo. >> "%REL%"

set VIU=
set /p VIU=Apareceu algum quadro colorido no oculos? (s = sim, n = nao): 
echo resposta do usuario (apareceu imagem no oculos?): !VIU! >> "%REL%"
echo. >> "%REL%"

rem ---- 2. versao de 32 bits: confirma o lado antigo (só dois modos, o resto ja sabemos)
echo [TESTE 32 BITS - o caminho de hoje] >> "%REL%"
for %%V in (4 1) do call :rodar %%V
call :mostrar32 4
call :mostrar32 1
echo. >> "%REL%"

rem ---- 3. o jogo (apaga os rastros velhos antes)
if exist "port\vr-trace.txt" del "port\vr-trace.txt" >nul 2>&1
if exist "port\black.log" del "port\black.log" >nul 2>&1
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

rem ============================================================
rem  Cada teste roda com tempo limite: se travar, e morto e o
rem  relatorio continua (antes o teste travava a janela toda).
rem ============================================================

:rodar64
set V=%1
echo. >> "%REL%"
echo   - modo 64 bits %V% ... >> "%REL%"
if not exist "port\x64\vr-sonda64.exe" exit /b
if exist "port\x64\xr-loader64.log" del "port\x64\xr-loader64.log" >nul 2>&1
pushd port\x64
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$env:XR_LOADER_DEBUG='all'; $env:XR_DEBUG_FILE=(Join-Path (Get-Location) 'xr-loader64.log');" ^
  "$p = Start-Process -FilePath '.\vr-sonda64.exe' -ArgumentList %V% -PassThru -NoNewWindow " ^
  "  -RedirectStandardOutput 'console64-%V%.txt' -RedirectStandardError 'erro64-%V%.txt';" ^
  "if ($p.WaitForExit(120000)) { '    codigo ' + $p.ExitCode } else { $p.Kill(); '    TRAVOU: encerrado depois de 120 s' }" >> "%~dp0%REL%" 2>&1
popd
call :mostrar64 %V%
exit /b

:mostrar64
set V=%1
if exist "port\x64\sonda64-%V%.txt" (
  type "port\x64\sonda64-%V%.txt" >> "%REL%"
  del "port\x64\sonda64-%V%.txt" >nul 2>&1
)
if exist "port\x64\console64-%V%.txt" (
  echo     --- saida da janela --- >> "%REL%"
  type "port\x64\console64-%V%.txt" >> "%REL%"
  del "port\x64\console64-%V%.txt" >nul 2>&1
)
if exist "port\x64\erro64-%V%.txt" (
  echo     --- erros --- >> "%REL%"
  type "port\x64\erro64-%V%.txt" >> "%REL%"
  del "port\x64\erro64-%V%.txt" >nul 2>&1
)
if exist "port\x64\xr-loader64.log" (
  echo     --- log do conector de 64 bits (inicio) --- >> "%REL%"
  powershell -NoProfile -Command "Get-Content '%~dp0port\x64\xr-loader64.log' -TotalCount 30" >> "%REL%" 2>nul
  del "port\x64\xr-loader64.log" >nul 2>&1
)
exit /b

:rodar
set V=%1
echo. >> "%REL%"
echo   - modo 32 bits %V% ... >> "%REL%"
if not exist "port\vr-sonda.exe" exit /b
if exist "port\xr-loader.log" del "port\xr-loader.log" >nul 2>&1
pushd port
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$env:XR_LOADER_DEBUG='all'; $env:XR_DEBUG_FILE=(Join-Path (Get-Location) 'xr-loader.log');" ^
  "$p = Start-Process -FilePath '.\vr-sonda.exe' -ArgumentList %V% -PassThru -NoNewWindow " ^
  "  -RedirectStandardOutput 'console-%V%.txt' -RedirectStandardError 'erro-%V%.txt';" ^
  "if ($p.WaitForExit(90000)) { '    codigo ' + $p.ExitCode } else { $p.Kill(); '    TRAVOU: encerrado depois de 90 s' }" >> "%~dp0%REL%" 2>&1
popd
exit /b

:mostrar32
set V=%1
if exist "port\sonda-%V%.txt" (
  type "port\sonda-%V%.txt" >> "%REL%"
  del "port\sonda-%V%.txt" >nul 2>&1
)
if exist "port\console-%V%.txt" (
  echo     --- saida da janela --- >> "%REL%"
  type "port\console-%V%.txt" >> "%REL%"
  del "port\console-%V%.txt" >nul 2>&1
)
if exist "port\erro-%V%.txt" (
  echo     --- erros --- >> "%REL%"
  type "port\erro-%V%.txt" >> "%REL%"
  del "port\erro-%V%.txt" >nul 2>&1
)
exit /b

:antigo
echo.
echo ************************************************************
echo   ESTE DIAGNOSTICO E O ANTIGO - NAO SERVE MAIS.
echo.
echo   Faltam os arquivos:
echo      port\vr-sonda.exe         (teste de 32 bits)
echo      port\x64\vr-sonda64.exe   (teste de 64 bits)
echo   Eles vem no pacote NOVO (BLACK-VR-pronto-v11.zip).
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
