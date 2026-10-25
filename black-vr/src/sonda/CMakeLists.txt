# Este arquivo só é usado no Windows/MSVC (32 bits); no Linux ele não entra no build.
cmake_minimum_required(VERSION 3.20)
project(black-vr-sonda CXX)

# /MT: o vr-sonda.exe roda sozinho, sem precisar de nenhum "runtime do Visual Studio"
# instalado na máquina do usuário (a pasta dele não tem como instalar isso).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_executable(vr-sonda sonda.cpp)

target_include_directories(vr-sonda PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/openxr")
target_link_libraries(vr-sonda PRIVATE d3d11 dxgi advapi32 ole32)
target_compile_definitions(vr-sonda PRIVATE _CRT_SECURE_NO_WARNINGS)
