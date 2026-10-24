@echo off
rem ============================================================
rem  BLACK VR - transformar o seu BLACK de PC em BLACK de VR
rem
rem  Duplo clique. O programa:
rem    1. pergunta onde esta o seu BLACK (voce arrasta a pasta)
rem    2. confere se a sua copia do jogo e a versao certa
rem    3. baixa o codigo do port e aplica o meu trabalho de VR
rem    4. compila (20 min) e INSTALA na sua pasta, sem apagar nada
rem    5. cria dois atalhos: JOGAR-VR.bat e TESTAR-OCULOS.bat
rem
rem  O seu jogo, seus saves e suas configuracoes ficam guardados.
rem ============================================================
setlocal enabledelayedexpansion
chcp 65001 >nul
title BLACK VR - montar o seu PCVR

set "HERE=%~dp0"
set "HERE=%HERE:~0,-1%"
set "FORK=%HERE%\_fork\black-pc"
set "BASE=Spark198rus/black-pc"
set "BASE_COMMIT=b826153"
set "NTSC=DF2739C372D254A90AEECCF5971D12097F22D89FBB90DB021FDA96DD4DEB68F6"
set "PAL=67F35121CCF622E7A293C402219596FE6456CA4C3D27E6F8139E449264DA30F0"

cls
echo ============================================================
echo   BLACK VR - transformar o seu BLACK de PC em BLACK de VR
echo ============================================================
echo.
echo   Eu preciso achar a pasta onde esta o SEU jogo.
echo   E a pasta que tem dentro dela um arquivo chamado default.xbe.
echo.
echo   Jeito facil: abra a pasta no Explorador, arraste ela
echo   para esta janela preta e aperte Enter.
echo.

set "ALVO=%~1"
:pedir
if not "%ALVO%"=="" goto achar
set /p ALVO=  Arraste a pasta do BLACK aqui:
set "ALVO=%ALVO:"=%"
if "%ALVO%"=="" (
    echo.
    echo   Nenhuma pasta informada. Fechando.
    goto :sair_fim
)

:achar
rem tira barra invertida do fim
if "%ALVO:~-1%"=="\" set "ALVO=%ALVO:~0,-1%"

set "GAME="
set "ROOT="
if exist "%ALVO%\game\default.xbe" (
    set "ROOT=%ALVO%"
    set "GAME=%ALVO%\game"
) else if exist "%ALVO%\default.xbe" (
    set "GAME=%ALVO%"
) else (
    echo   Procurando o arquivo do jogo dentro da pasta...
    for /r "%ALVO%" %%F in (default.xbe) do (
        if not defined GAME set "GAME=%%~dpF"
    )
    if defined GAME if "!GAME:~-1!"=="\" set "GAME=!GAME:~0,-1!"
)

if not defined GAME (
    echo.
    echo   [NAO ACHEI] Nao existe default.xbe nessa pasta nem dentro dela.
    echo.
    echo   Confira se voce arrastou a pasta certa. Ela precisa ter
    echo   o default.xbe (pode estar dentro de uma pasta chamada game).
    echo.
    set "ALVO="
    pause
    goto :pedir
)

if not defined ROOT (
    for %%D in ("%GAME%\..") do set "ROOT=%%~fD"
)

echo.
echo   Pasta do jogo .... %GAME%
echo   Pasta principal .. %ROOT%
echo.

rem ---------- confere a versao do jogo ----------
set "XBE=%GAME%\default.xbe"
echo   Conferindo se a sua copia do jogo serve (10 segundos)...
set "HASH="
for /f "skip=1 delims=" %%A in ('certutil -hashfile "%XBE%" SHA256') do (
    if not defined HASH (
        set "T=%%A"
        set "T=!T: =!"
        if not "!T:~31,1!"=="" set "HASH=!T!"
    )
)
if /i "!HASH!"=="%NTSC%" (
    echo   [OK] Versao americana ^(NTSC-U^) - a certa.
    goto :ferramentas
)
if /i "!HASH!"=="%PAL%" (
    echo   [OK] Versao europeia ^(PAL^) - tambem serve.
    goto :ferramentas
)
echo.
echo   [ATENCAO] A sua copia do jogo nao e a original americana nem a europeia.
echo   Codigo: !HASH!
echo   O mais provavel e ser uma traducao de fas. O jogo ate pode abrir,
echo   mas pode travar.
echo.
choice /c SN /m "  Continuar mesmo assim"
if errorlevel 2 goto :sair_fim

:ferramentas
echo.
rem ---------- ferramentas ----------
where git >nul 2>&1
if errorlevel 1 (
    echo   [FALTA] O Git nao esta instalado.
    echo           Baixe em https://git-scm.com/download/win e instale
    echo           (pode ir clicando Next, Next, Next).
    goto :erro
)
where cmake >nul 2>&1
if errorlevel 1 (
    echo   [FALTA] O CMake nao esta instalado.
    echo           Baixe em https://cmake.org/download/ e instale
    echo           marcando a opcao "Add CMake to the system PATH".
    goto :erro
)
echo   [OK] Git e CMake encontrados.

rem ---------- o codigo do port ----------
echo.
if exist "%FORK%\.git" (
    echo   [OK] Codigo do port ja baixado antes, reaproveitando.
) else (
    echo   Baixando o codigo do port do BLACK ^(uns 30 MB^)...
    if not exist "%HERE%\_fork" mkdir "%HERE%\_fork"
    git clone --quiet https://github.com/%BASE%.git "%FORK%"
    if errorlevel 1 (
        echo   [ERRO] Nao consegui baixar. Verifique a internet.
        goto :erro
    )
)
git -C "%FORK%" checkout --quiet %BASE_COMMIT% 2>nul
git -C "%FORK%" reset --hard --quiet
echo   [OK] Codigo na versao de base.

rem ---------- o modulo de VR ----------
echo.
echo   Colocando o modulo de VR no codigo...
if not exist "%FORK%\loader\src\vr" mkdir "%FORK%\loader\src\vr"
if not exist "%FORK%\vendor\openxr" mkdir "%FORK%\vendor\openxr"
copy /y "%HERE%\src\vr\*.h"   "%FORK%\loader\src\vr\" >nul
copy /y "%HERE%\src\vr\*.cpp" "%FORK%\loader\src\vr\" >nul
copy /y "%HERE%\vendor\openxr\*.h" "%FORK%\vendor\openxr\" >nul
echo   [OK] Arquivos copiados.

echo   Aplicando as emendas no codigo do port...
for %%P in ("%HERE%\patches\*.patch") do (
    echo        %%~nxP
    git -C "%FORK%" apply --whitespace=nowarn "%%~fP"
    if errorlevel 1 (
        echo.
        echo   [ERRO] A emenda %%~nxP nao aplicou.
        echo          Tire um print desta janela e mande para o agente.
        goto :erro
    )
)
echo   [OK] Emendas aplicadas.

rem ---------- compilar ----------
echo.
echo   Preparando a compilacao ^(MSVC 32 bits^)...
cmake -S "%FORK%\loader" -B "%FORK%\loader\build" -A Win32 -DCMAKE_BUILD_TYPE=Release > "%HERE%\_cmake-config.txt" 2>&1
if errorlevel 1 (
    echo.
    echo   [ERRO] O CMake nao conseguiu preparar.
    echo          Normalmente e o Visual Studio sem o "C++ para x86".
    echo          Abra o Visual Studio Installer, clique em Modificar e marque:
    echo            - Desenvolvimento para desktop com C++
    echo            - e, em Detalhes, as ferramentas do MSVC v143 para x86.
    echo.
    echo          Detalhes tecnicos em: %HERE%\_cmake-config.txt
    goto :erro
)

echo   Compilando... isso leva uns 20 minutos. Pode deixar rodando.
echo   ^(A janela fica parada um tempo em cada linha. E normal.^)
echo.
cmake --build "%FORK%\loader\build" --config Release
if errorlevel 1 (
    echo.
    echo   [ERRO] A compilacao falhou.
    echo          Copie o texto do erro acima ^(do "error C" ate o fim^)
    echo          e mande para o agente. E assim que eu corrijo o codigo.
    goto :erro
)

set "BUILD=%FORK%\loader\build\Release"
if not exist "%BUILD%\black.exe" (
    echo   [ERRO] Compilou, mas nao achei o black.exe em %BUILD%
    goto :erro
)

rem ---------- instalar ----------
echo.
echo   Instalando na sua pasta ^(guardando o que ja existia^)...
set "BK=%ROOT%\_port-antigo"
if not exist "%BK%" mkdir "%BK%"
if exist "%ROOT%\port\black.exe"   copy /y "%ROOT%\port\black.exe"   "%BK%\" >nul
if exist "%ROOT%\port\blackrt.dll" copy /y "%ROOT%\port\blackrt.dll" "%BK%\" >nul
if exist "%ROOT%\black.exe"        copy /y "%ROOT%\black.exe"        "%BK%\" >nul
if exist "%ROOT%\blackrt.dll"      copy /y "%ROOT%\blackrt.dll"      "%BK%\" >nul
if exist "%GAME%\black.exe"        copy /y "%GAME%\black.exe"        "%BK%\" >nul
if exist "%GAME%\blackrt.dll"      copy /y "%GAME%\blackrt.dll"      "%BK%\" >nul
if exist "%GAME%\black.ini"        copy /y "%GAME%\black.ini"        "%BK%\" >nul
if exist "%ROOT%\black.ini"        copy /y "%ROOT%\black.ini"        "%BK%\" >nul

if not exist "%ROOT%\port" mkdir "%ROOT%\port"
copy /y "%BUILD%\black.exe"   "%ROOT%\port\black.exe"   >nul
copy /y "%BUILD%\blackrt.dll" "%ROOT%\port\blackrt.dll" >nul
echo   [OK] Programa novo instalado em: %ROOT%\port

rem leva os saves e as configuracoes para o lugar novo
if exist "%GAME%\savedata" if not exist "%ROOT%\port\savedata" (
    echo   Levando seus saves para o lugar novo...
    xcopy "%GAME%\savedata" "%ROOT%\port\savedata\" /E /I /Q /Y >nul
    echo   [OK] Saves preservados.
)
if exist "%GAME%\black.ini" if not exist "%ROOT%\port\black.ini" (
    copy /y "%GAME%\black.ini" "%ROOT%\port\black.ini" >nul
    echo   [OK] Suas configuracoes preservadas.
)

rem ---------- atalhos ----------
> "%ROOT%\JOGAR-VR.bat" echo @echo off
>>"%ROOT%\JOGAR-VR.bat" echo title BLACK em VR
>>"%ROOT%\JOGAR-VR.bat" echo cd /d "%ROOT%"
>>"%ROOT%\JOGAR-VR.bat" echo echo Coloque o oculos e espere o jogo abrir...
>>"%ROOT%\JOGAR-VR.bat" echo "%ROOT%\port\black.exe" --vr
>>"%ROOT%\JOGAR-VR.bat" echo if errorlevel 1 pause

> "%ROOT%\TESTAR-OCULOS.bat" echo @echo off
>>"%ROOT%\TESTAR-OCULOS.bat" echo title BLACK VR - teste do oculos
>>"%ROOT%\TESTAR-OCULOS.bat" echo cd /d "%ROOT%"
>>"%ROOT%\TESTAR-OCULOS.bat" echo echo Teste de 30 segundos: piso, regua e alvo a 2 metros.
>>"%ROOT%\TESTAR-OCULOS.bat" echo echo Se estiver tudo no lugar e do tamanho certo, feche a janela.
>>"%ROOT%\TESTAR-OCULOS.bat" echo "%ROOT%\port\black.exe" --vr-selftest 30
>>"%ROOT%\TESTAR-OCULOS.bat" echo pause

echo.
echo ============================================================
echo   PRONTO.
echo ============================================================
echo.
echo   Na sua pasta agora existem dois arquivos novos:
echo.
echo     1. TESTAR-OCULOS.bat   ^<- abra este PRIMEIRO
echo        Teste de 30 segundos. Voce deve ver o chao, uma regua
echo        de 10 cm e um alvo a 2 metros. Se parecer do tamanho
echo        certo, esta tudo funcionando.
echo.
echo     2. JOGAR-VR.bat        ^<- depois deste, sempre este
echo        Abre o BLACK nos oculos.
echo.
echo   O que ficou guardado, sem apagar nada:
echo     %BK%   ^(programa antigo^)
echo     %ROOT%\port\savedata   ^(seus saves^)
echo.
echo   Pasta: %ROOT%
echo.
start "" explorer "%ROOT%"
pause
exit /b 0

:erro
echo.
echo Nada foi perdido. Resolva o que esta escrito acima e rode de novo.
echo.
pause
exit /b 1

:sair_fim
echo.
pause
exit /b 0
