# Calculadora Boleado

Controle de **Compras e Custos** da confeitaria.

## Total gasto agora

**R$ 416,56** — terceira à sexta compra (1ª e 2ª ainda não lançadas).

| Compra | Itens | Total |
|---|---:|---:|
| 3ª | 2 | R$ 46,88 |
| 4ª | 6 | R$ 138,41 |
| 5ª | 9 | R$ 201,27 |
| 6ª | 1 | R$ 30,00 |
| **Geral** | **18** | **R$ 416,56** |

## Excel

Baixe e abra no Excel ou no Google Planilhas:

- [`Compras_e_Custos_Boleado.xlsx`](./Compras_e_Custos_Boleado.xlsx)

Abas: Compras e Custos · Resumo por compra · Resumo por categoria · Estoque consolidado · Como usar.

Para regenerar a planilha:

```bash
python3 -m venv .venv
.venv/bin/pip install openpyxl
.venv/bin/python generate_excel.py
```

## Painel

Abra `index.html` no navegador (ou sirva a pasta com qualquer servidor estático).
