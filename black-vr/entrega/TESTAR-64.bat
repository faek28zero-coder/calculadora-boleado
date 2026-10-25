@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set REL=RELATORIO-64.txt
title BLACK VR - teste 64 bits (v13)

rem ---- sem as janelas de "o programa parou de funcionar"
reg add "HKCU\Software\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1 /f >nul 2>&1

if not exist "vr-sonda64.exe" goto :faltou
if not exist "openxr_loader.dll" goto :faltou

echo ============================================================
echo   BLACK VR - TESTE DE 64 BITS (versao 13)
echo ============================================================
echo.
echo   1. Abra o aplicativo do oculos (Meta Horizon Link).
echo   2. Oculos ligado no cabo.
echo   3. COLOQUE O OCULOS NA CABECA AGORA.
echo.
echo   A janela abaixo mostra TUDO o que o teste faz, linha por linha,
echo   com o tempo de cada passo. Se ficar parado, e ali que travou.
echo.
echo   O quadro colorido tem de aparecer no oculos nos primeiros
echo   segundos do modo 10. Se o oculos ficar em "carregando" para
echo   sempre, o teste desiste sozinho em 90 segundos.
echo.
pause

echo === BLACK VR - teste de 64 bits, versao 13 === > "%REL%"
echo data: %DATE%  %TIME% >> "%REL%"
echo pasta: %CD% >> "%REL%"
echo. >> "%REL%"

powershell -NoProfile -Command "Get-ChildItem -Path '%~dp0' -Include *.dll,*.exe -Recurse | Unblock-File -ErrorAction SilentlyContinue" >nul 2>&1

echo [TESTE COM IMAGEM] >> "%REL%"
call :rodar 10
echo. >> "%REL%"

set VIU=
set /p VIU=Apareceu o quadro colorido no oculos? (s = sim, n = nao): 
echo resposta do usuario (apareceu imagem no oculos?): !VIU! >> "%REL%"
echo. >> "%REL%"

echo [OUTROS TESTES] >> "%REL%"
call :rodar 4
call :rodar 11
call :rodar 12
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
rem  Roda um modo mostrando tudo na janela. A propria sonda tem
rem  prazo interno (90 s para a sessao, 60 s esperando o oculos),
rem  por isso a janela nao fica travada.
rem ============================================================
:rodar
set V=%1
if not exist "vr-sonda64.exe" exit /b
if exist "sonda64-%V%.txt" del "sonda64-%V%.txt" >nul 2>&1
echo.
echo ============================================================
echo    MODO %V%
echo ============================================================
echo. >> "%REL%"
echo   - modo %V% ... >> "%REL%"
vr-sonda64.exe %V%
set RC=!ERRORLEVEL!
echo.
echo    (modo %V% terminou com codigo !RC!)
echo     codigo !RC! >> "%REL%"
if exist "sonda64-%V%.txt" (
  type "sonda64-%V%.txt" >> "%REL%"
  del "sonda64-%V%.txt" >nul 2>&1
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
echo   Baixe o VR-64BITS.zip da pasta do Drive e extraia tudo aqui.
echo ************************************************************
echo.
pause
exit /b
