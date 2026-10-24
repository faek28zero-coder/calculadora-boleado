@echo off
rem ============================================================
rem  BLACK VR - montar e compilar (Windows)
rem
rem  Duplo clique neste arquivo. Ele faz o trabalho todo:
rem    1. baixa o port do BLACK (black-pc, GPL-2.0)
rem    2. coloca o modulo de VR dentro dele
rem    3. aplica as emendas no codigo do port
rem    4. compila (precisa do Visual Studio com C++ e do CMake)
rem
rem  O que ele NAO faz: o jogo. O `default.xbe` e seu, e vai depois.
rem ============================================================
setlocal enabledelayedexpansion
chcp 65001 >nul
title BLACK VR - montar e compilar

set "HERE=%~dp0"
set "HERE=%HERE:~0,-1%"
set "FORK=%HERE%\_fork\black-pc"
set "BASE=Spark198rus/black-pc"
set "BASE_COMMIT=b826153"

echo.
echo ============================================================
echo   BLACK VR - montar e compilar
echo ============================================================
echo   pasta: %HERE%
echo.

rem ---------- 1. ferramentas ----------
where git >nul 2>&1
if errorlevel 1 (
    echo [FALTA] Git nao encontrado.
    echo         Instale em https://git-scm.com/download/win e rode de novo.
    goto :fim_erro
)
where cmake >nul 2>&1
if errorlevel 1 (
    echo [FALTA] CMake nao encontrado.
    echo         Instale em https://cmake.org/download/ (marque "Add CMake to the PATH").
    goto :fim_erro
)
echo [ok] git e cmake encontrados.

rem ---------- 2. o port ----------
if exist "%FORK%\.git" (
    echo [ok] port ja baixado, reaproveitando.
) else (
    echo [..] baixando o port do BLACK (%BASE%)...
    if not exist "%HERE%\_fork" mkdir "%HERE%\_fork"
    git clone https://github.com/%BASE%.git "%FORK%"
    if errorlevel 1 goto :fim_erro
)
git -C "%FORK%" checkout --quiet %BASE_COMMIT% 2>nul
git -C "%FORK%" reset --hard --quiet
echo [ok] port na versao de base (%BASE_COMMIT%).

rem ---------- 3. o modulo de VR ----------
echo [..] copiando o modulo de VR...
if not exist "%FORK%\loader\src\vr" mkdir "%FORK%\loader\src\vr"
if not exist "%FORK%\vendor\openxr" mkdir "%FORK%\vendor\openxr"
copy /y "%HERE%\src\vr\*.h"   "%FORK%\loader\src\vr\" >nul
copy /y "%HERE%\src\vr\*.cpp" "%FORK%\loader\src\vr\" >nul
copy /y "%HERE%\vendor\openxr\*.h" "%FORK%\vendor\openxr\" >nul
echo [ok] arquivos copiados.

echo [..] aplicando as emendas no port...
for %%P in ("%HERE%\patches\*.patch") do (
    echo      %%~nxP
    git -C "%FORK%" apply --whitespace=nowarn "%%~fP"
    if errorlevel 1 (
        echo.
        echo [ERRO] a emenda %%~nxP nao aplicou.
        echo        Isso acontece se o port mudou depois da versao de base.
        echo        Mande esta mensagem inteira para o agente resolver.
        goto :fim_erro
    )
)
echo [ok] emendas aplicadas.

rem ---------- 4. compilar ----------
echo.
echo [..] configurando o build (MSVC 32 bits)...
cmake -S "%FORK%\loader" -B "%FORK%\loader\build" -A Win32 -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo.
    echo [ERRO] o CMake nao conseguiu configurar.
    echo        Normalmente e o Visual Studio sem o "C++ para x86".
    echo        Abra o Visual Studio Installer e marque:
    echo          - Desenvolvimento para desktop com C++
    echo          - e, em "Detalhes", as ferramentas de build do MSVC v143 para x86/x64.
    goto :fim_erro
)

echo.
echo [..] compilando (pode levar alguns minutos)...
cmake --build "%FORK%\loader\build" --config Release
if errorlevel 1 (
    echo.
    echo [ERRO] a compilacao falhou.
    echo        Copie o texto do erro acima (do "error C..." ate o fim) e mande
    echo        para o agente: e assim que ele corrige o codigo.
    goto :fim_erro
)

echo.
echo ============================================================
echo   PRONTO. Os arquivos gerados:
echo     %FORK%\loader\build\Release\black.exe
echo     %FORK%\loader\build\Release\blackrt.dll
echo.
echo   Como jogar:
echo     1. copie os DOIS arquivos para uma pasta chamada `port`
echo     2. ao lado dela, uma pasta `game` com a SUA copia do BLACK
echo          BLACK\game\default.xbe ...
echo          BLACK\port\black.exe
echo     3. para o VR:  black.exe --vr
echo        teste do headset, sem o jogo:  black.exe --vr-selftest 20
echo ============================================================
echo.
pause
exit /b 0

:fim_erro
echo.
echo Nada foi perdido: rode este arquivo de novo depois de resolver.
echo.
pause
exit /b 1
