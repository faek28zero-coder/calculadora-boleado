@echo off
cd /d "%~dp0"
echo ============================================================
echo   TESTE DO OCULOS - pacote v11 (programa de 64 bits)
echo ============================================================
echo.
echo Vai aparecer um quadro colorido DENTRO do oculos por 20 segundos.
echo O aplicativo do oculos tem de estar aberto e o oculos ligado no cabo.
echo.
pause

powershell -NoProfile -Command "Get-ChildItem -Path '%~dp0port' -Include *.dll,*.exe -Recurse | Unblock-File -ErrorAction SilentlyContinue" >nul 2>&1

if not exist "port\x64\vr-sonda64.exe" (
  echo ERRO: nao achei port\x64\vr-sonda64.exe.
  echo Esse arquivo vem no pacote NOVO ^(BLACK-VR-pronto-v10.zip^).
  echo Extraia o pacote novo POR CIMA desta pasta, escolhendo "Substituir".
  pause
  exit /b
)

pushd port\x64
vr-sonda64.exe 10
set CODE=%ERRORLEVEL%
popd
echo.
echo Terminou com codigo %CODE%   (0 = a imagem chegou no oculos)
echo.
if exist "port\x64\sonda64-10.txt" (
  echo --- detalhes do teste ---
  type "port\x64\sonda64-10.txt"
)
echo.
echo Conta pra mim: apareceu o quadro colorido no oculos? (sim / nao)
pause
