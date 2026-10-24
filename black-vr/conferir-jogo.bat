@echo off
rem ============================================================
rem  BLACK VR - conferir se a sua copia serve para o port
rem
rem  Duplo clique. Ele procura o default.xbe, calcula o SHA-256 e diz
rem  em portugues claro se e uma das duas versoes que o port reconhece.
rem  No fim grava LISTA.txt com tudo, para colar no chat.
rem ============================================================
setlocal enabledelayedexpansion
chcp 65001 >nul
title BLACK VR - conferir o jogo

set "OUT=%~dp0LISTA.txt"
set "NTSC=DF2739C372D254A90AEECCF5971D12097F22D89FBB90DB021FDA96DD4DEB68F6"
set "PAL=67F35121CCF622E7A293C402219596FE6456CA4C3D27E6F8139E449264DA30F0"

>"%OUT%" echo BLACK VR - conferir a copia do jogo
>>"%OUT%" echo.

set "GAME=%~1"
if "%GAME%"=="" set /p GAME=Arraste a pasta do jogo para esta janela e aperte Enter: 
set "GAME=%GAME:"=%"
if "%GAME%"=="" goto sem_pasta

>>"%OUT%" echo Pasta informada: %GAME%
>>"%OUT%" echo.

echo Procurando o default.xbe...
set "XBE="
if exist "%GAME%\default.xbe" set "XBE=%GAME%\default.xbe"
if not defined XBE if exist "%GAME%\game\default.xbe" set "XBE=%GAME%\game\default.xbe"
if not defined XBE for /r "%GAME%" %%F in (default.xbe) do if not defined XBE set "XBE=%%F"

>>"%OUT%" echo === conteudo da pasta ===
dir /b "%GAME%" >>"%OUT%" 2>nul
>>"%OUT%" echo.
if exist "%GAME%\*.iso" >>"%OUT%" echo ATENCAO: existe um arquivo .iso nesta pasta.
if exist "%GAME%\black.exe" >>"%OUT%" echo ATENCAO: existe um black.exe aqui (port ja compilado?).
if exist "%GAME%\blackrt.dll" >>"%OUT%" echo ATENCAO: existe um blackrt.dll aqui (port ja compilado?).
>>"%OUT%" echo.

if not defined XBE goto sem_xbe

>>"%OUT%" echo Arquivo encontrado: !XBE!
echo Calculando o SHA-256 (pode levar alguns segundos)...
set "HASH="
for /f "skip=1 delims=" %%A in ('certutil -hashfile "!XBE!" SHA256') do (
    if not defined HASH (
        set "T=%%A"
        set "T=!T: =!"
        rem so aceita a linha que tem tamanho de hash (32+ caracteres)
        if not "!T:~31,1!"=="" set "HASH=!T!"
    )
)

>>"%OUT%" echo SHA-256 do default.xbe:
>>"%OUT%" echo   !HASH!
>>"%OUT%" echo.

if /i "!HASH!"=="%NTSC%" goto serve_ntsc
if /i "!HASH!"=="%PAL%" goto serve_pal
goto nao_serve

:serve_ntsc
>>"%OUT%" echo RESULTADO: SERVE - versao NTSC-U (America), a principal que o port suporta.
echo.
echo   SERVE. Versao NTSC-U (America).
echo   Falta so compilar o programa: use o compilar.bat.
goto fim

:serve_pal
>>"%OUT%" echo RESULTADO: SERVE - versao PAL Europa, tambem suportada pelo port.
echo.
echo   SERVE. Versao PAL Europa.
echo   Falta so compilar o programa: use o compilar.bat.
goto fim

:nao_serve
>>"%OUT%" echo RESULTADO: NAO SERVE - este default.xbe nao e de uma das duas versoes suportadas.
>>"%OUT%" echo.
>>"%OUT%" echo Quase sempre e uma traducao de fas (PT-BR, por exemplo) ou um repack: o texto muda,
>>"%OUT%" echo o codigo muda de endereco e o port nao reconhece a build. Os unicos que servem:
>>"%OUT%" echo   NTSC-U  DF2739C372D254A90AEECCF5971D12097F22D89FBB90DB021FDA96DD4DEB68F6
>>"%OUT%" echo   PAL     67F35121CCF622E7A293C402219596FE6456CA4C3D27E6F8139E449264DA30F0
echo.
echo   NAO SERVE. Este default.xbe nao e NTSC-U nem PAL original.
echo   (traducao de fas ou repack: o port nao reconhece a build)
goto fim

:sem_xbe
>>"%OUT%" echo NAO ACHEI um default.xbe nesta pasta.
>>"%OUT%" echo.
>>"%OUT%" echo Isso normalmente significa uma destas coisas:
>>"%OUT%" echo   - e uma ISO de PlayStation 2 (o port e do jogo de Xbox, nao abre ISO de PS2);
>>"%OUT%" echo   - e um port ja compilado por outra pessoa (black.exe + blackrt.dll);
>>"%OUT%" echo   - a extracao do RAR ainda nao foi feita.
echo.
echo   NAO ACHEI default.xbe. Veja o LISTA.txt: ele lista o que ha na pasta.
goto fim

:sem_pasta
>>"%OUT%" echo Nenhuma pasta informada.
echo.
echo   Voce nao arrastou nenhuma pasta. Rode de novo e arraste a pasta que tem o default.xbe.

:fim
>>"%OUT%" echo.
>>"%OUT%" echo ------------------------------------------------------------
echo.
echo O resultado foi gravado em:
echo   %OUT%
echo Copie o conteudo desse arquivo e cole no chat.
start "" notepad "%OUT%"
pause
endlocal
