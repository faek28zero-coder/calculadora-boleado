@echo off
cd /d "%~dp0"
echo Abrindo o BLACK em VR...
echo.
echo Antes: o aplicativo do oculos (Meta Horizon Link) tem de estar ABERTO
echo e o oculos ligado no cabo.
echo.

rem arquivos vindos da internet ficam marcados: tira a marca antes de abrir
powershell -NoProfile -Command "Get-ChildItem -Path '%~dp0port' -Include *.dll,*.exe -Recurse | Unblock-File -ErrorAction SilentlyContinue" >nul 2>&1

if not exist "port\black.exe" (
  echo ERRO: nao achei a pasta "port" com o black.exe ao lado deste arquivo.
  pause
  exit /b
)
if not exist "game\default.xbe" (
  echo AVISO: nao achei game\default.xbe nesta pasta. O jogo fica em "game".
)

pushd port
black.exe --vr
popd
echo.
echo O jogo fechou (codigo %ERRORLEVEL%).
pause
