@echo off
cd /d "%~dp0"
echo Testando o oculos SEM abrir o jogo (cena de calibracao, 30 segundos).
echo O aplicativo do oculos tem de estar aberto e o oculos ligado no cabo.
echo.

powershell -NoProfile -Command "Get-ChildItem -Path '%~dp0port' -Include *.dll,*.exe -Recurse | Unblock-File -ErrorAction SilentlyContinue" >nul 2>&1

if not exist "port\black.exe" (
  echo ERRO: nao achei port\black.exe ao lado deste arquivo.
  pause
  exit /b
)

pushd port
black.exe --vr-selftest 30
set CODE=%ERRORLEVEL%
popd
echo.
echo Terminou com codigo %CODE%   (0 = o oculos respondeu)
if exist "port\vr-trace.txt" (
  echo.
  echo --- rastro completo (o que interessa pode estar bem no alto) ---
  type "port\vr-trace.txt"
)
pause
