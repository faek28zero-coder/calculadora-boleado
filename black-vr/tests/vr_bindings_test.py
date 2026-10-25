#!/usr/bin/env python3
"""Toda função OpenXR que o módulo chama tem de ter sido ligada antes.

O erro de 25/10: `p_xrEnumerateInstanceExtensionProperties` era chamada e nunca
resolvida. Pon- teiro nulo, falha de acesso em 0x0, e nenhum teste daqui pegava —
a sintaxe compila, o link passa (o símbolo é uma variável, não uma função) e só
o Windows real estourava. Este teste lê o fonte e confere, para cada `p_xrX`
CHAMADA, se `xrX` aparece em algum `bind(...)` ou no bloco Early (funções
resolvidas antes da instância existir, com instância nula).
"""
import re
import sys

fonte = "src/vr/vr_openxr.cpp"
texto = open(fonte, encoding="utf-8").read()

chamadas = set(re.findall(r"\b(p_xr[A-Za-z0-9_]+)\s*\(", texto))
ligadas = set(re.findall(r'bind\(\s*(p_xr[A-Za-z0-9_]+)\s*,', texto))
# bloco Early: pares {(PFN_... *)&p_xrNome, "xrNome"}
ligadas |= set(re.findall(r"&(p_xr[A-Za-z0-9_]+)\s*,\s*\"", texto))
# funções que o conector entrega direto por GetProcAddress (xrGetInstanceProcAddr é a raiz de tudo)
ligadas.add("p_xrGetInstanceProcAddr")
# ponteiros de DLL carregados por GetProcAddress (fora do OpenXR)
ligadas |= set(re.findall(r"(p_[A-Za-z0-9_]+)\s*=\s*\(?PFN_", texto))

faltando = sorted(chamadas - ligadas)
if faltando:
    print(f"  FALHOU: {len(faltando)} função(ões) chamada(s) sem ter sido ligada(s):")
    for f in faltando:
        print(f"    {f}")
    sys.exit(1)

print(f"  {len(chamadas)} funções OpenXR chamadas, todas ligadas antes do uso")
