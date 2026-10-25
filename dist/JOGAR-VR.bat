@echo off
cd /d "%~dp0"
title BLACK VR - port de 64 bits

if not exist "BLACK.exe" (
  echo ERRO: nao achei BLACK.exe ao lado deste arquivo.
  pause
  exit /b
)
if not exist "assets\default.xbe" (
  echo ERRO: nao achei os arquivos do jogo.
  echo Coloque o default.xbe e as pastas do jogo dentro da pasta "assets".
  pause
  exit /b
)

rem liga a VR: abre o oculos e o jogo manda a imagem para ele
set BLACK_VR=1
echo Abrindo o BLACK em VR...
echo O aplicativo do oculos tem de estar aberto e o oculos ligado no cabo.
echo.
BLACK.exe
set CODE=%ERRORLEVEL%
echo.
echo Terminou com codigo %CODE%
pause
