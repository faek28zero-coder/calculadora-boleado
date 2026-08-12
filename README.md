# Calculadora Boleado — Controle de Compras

Planilha no modelo **CONTROLE DE COMPRAS — CONFEITARIA** (PDF enviado), com a 3ª à 6ª compra lançadas.

## Gasto em dinheiro

| | Valor |
|---|---:|
| Compras 1ª + 2ª (já estavam no modelo) | R$ 466,82 |
| Compras 3ª a 6ª (novas) | R$ 416,56 |
| **Subtotal das compras** | **R$ 883,38** |
| Luz e gás (20%) | R$ 176,68 |
| **Custo total c/ luz e gás** | **R$ 1.060,06** |

## Excel

[`planilha_custos_confeitaria_atualizada.xlsx`](./planilha_custos_confeitaria_atualizada.xlsx)

Abas: **Compras e Custos** (mesmo layout do PDF) e **Por compra**.

Para regenerar:

```bash
python3 -m venv .venv && .venv/bin/pip install openpyxl
.venv/bin/python generate_excel.py
```
