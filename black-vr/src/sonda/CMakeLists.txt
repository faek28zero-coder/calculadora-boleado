# Este arquivo só é usado no Windows/MSVC; no Linux ele não entra no build.
# O mesmo código compila em 32 bits (vr-sonda, usado pelo jogo de 32 bits) e em
# 64 bits (vr-sonda64, o teste que decide o caminho novo).
cmake_minimum_required(VERSION 3.20)
project(black-vr-sonda CXX)

# /MT: a sonda roda sozinha, sem precisar de nenhum "runtime do Visual Studio"
# instalado na máquina do usuário (a pasta dele não tem como instalar isso).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(VR_SONDA_NAME "vr-sonda" CACHE STRING "nome do executavel da sonda")

add_executable(${VR_SONDA_NAME} sonda.cpp)

target_include_directories(${VR_SONDA_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/openxr")
target_link_libraries(${VR_SONDA_NAME} PRIVATE d3d11 dxgi advapi32 ole32 psapi)
target_compile_definitions(${VR_SONDA_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
