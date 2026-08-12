#!/usr/bin/env python3
"""Recria a planilha no modelo do PDF e acrescenta a 3ª–6ª compra."""

from openpyxl import Workbook
from openpyxl.chart.shapes import GraphicalProperties
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter
from openpyxl.worksheet.page import PageMargins

# Cores extraídas do PDF original
VINHO = "6B214A"
VINHO2 = "A13B69"
AZUL = "4F81BD"
AZUL2 = "B8CCE4"
AZUL3 = "DCE6F1"
CINZA1 = "2B2B2B"
CINZA2 = "383838"
ROSA = "F8E7EF"
ROSA2 = "F3D4E0"
BRANCO = "FFFFFF"
PRETO = "000000"
AMARELO = "FFFF99"
CREME = "FFE299"
CINZA_TXT = "888888"

# (categoria, produto, preco, qtd, unidade, observacao, grupo)
# grupo: 1 = compra inicial, 2 = segunda, 3–6 = novas
ITENS = [
    # —— compra inicial ——
    ("Utilidades", "Papel-alumínio", 4.39, 1, "rolo", "", 1),
    ("Utilidades", "Plástico filme", 5.59, 1, "rolo", "", 1),
    ("Utilidades", "Papel-toalha", 4.39, 2, "rolos", "", 1),
    ("Utilidades", "Saco de confeitar", 7.50, 5, "unidades", "", 1),
    ("Utilidades", "Touca Descartável", 5.00, 5, "unidades", "", 1),
    ("Utilidades", "Durex", 4.50, 1, "unidade", "", 1),
    ("Utilidades", "Grampeador com grampos", 12.00, 1, "conjunto", "", 1),
    ("Utilidades", "Peneira", 19.29, 1, "unidade", "", 1),
    ("Ingredientes", "Chocolate em pó", 39.80, 200, "gramas", "", 1),
    ("Ingredientes", "Margarina Qualy", 6.98, 250, "gramas", "", 1),
    ("Ingredientes", "Paçoca", 23.90, 700, "gramas", "", 1),
    ("Ingredientes", "Leite em pó", 8.99, 200, "gramas", "", 1),
    ("Ingredientes", "Biscoito Oreo", 8.58, 180, "gramas", "", 1),
    ("Ingredientes", "Leite condensado", 29.12, 1580, "gramas", "", 1),
    ("Ingredientes", "Creme de leite", 14.96, 800, "gramas", "", 1),
    ("Ingredientes", "Pacote de bala", 7.98, 500, "gramas", "", 1),
    ("Ingredientes", "Granulado", 19.98, 400, "gramas", "", 1),
    ("Ingredientes", "Pacotão de Bis", 12.98, 201.6, "gramas", "", 1),
    ("Ingredientes", "Morango", 10.90, 220, "gramas", "", 1),
    ("Ingredientes", "Barra de chocolate Fracionado", 29.90, 1000, "gramas", "", 1),
    ("Embalagens", "Adesivos", 19.34, 70, "unidades", "", 1),
    ("Embalagens", "Pote 250 ml", 15.00, 24, "unidades", "", 1),
    ("Embalagens", "Pote 140 ml", 15.90, 25, "unidades", "", 1),
    ("Embalagens", "Forminha de brigadeiro", 3.50, 100, "unidades", "", 1),
    ("Embalagens", "Colherzinhas", 5.99, 50, "unidades", "", 1),
    ("Embalagens", "Saco chup-chup", 2.99, 100, "unidades", "", 1),
    ("Embalagens", "Embalagem quadrada de brigadeiro", 4.99, 10, "unidades", "", 1),
    ("Embalagens", "Pote de brinde", 6.50, 10, "unidades", "", 1),
    ("Embalagens", "Saco Kraft", 29.90, 50, "unidades", "", 1),
    ("Embalagens", "Embalagem trufas", 30.00, 300, "unidades", "", 1),
    # —— segunda compra (já estava no PDF) ——
    ("Embalagens", "Saco de confeitar GRANDE", 20.00, 10, "unidades", "Segunda compra", 2),
    ("Utilidades", "Forma pra fazer as trufas (não descartável)", 35.98, 2, "formas",
     "Segunda compra - INVESTIMENTO, não descartável", 2),
    # —— terceira compra ——
    ("Ingredientes", "Chocolate em pó", 39.90, 200, "gramas", "Terceira compra", 3),
    ("Ingredientes", "Margarina Qualy", 6.98, 250, "gramas", "Terceira compra", 3),
    # —— quarta compra ——
    ("Embalagens", "Caixa para cento de doces", 10.47, 3, "unidades", "Quarta compra", 4),
    ("Ingredientes", "Granulado", 19.99, 1, "pacote", "Quarta compra (peso do pacote não informado)", 4),
    ("Ingredientes", "Chocolate em pó 50%", 38.99, 1, "pacote", "Quarta compra (peso do pacote não informado)", 4),
    ("Ingredientes", "Leite condensado", 41.44, 2370, "gramas", "Quarta compra (6 latas de 395 g)", 4),
    ("Ingredientes", "Creme de leite", 21.52, 1600, "gramas", "Quarta compra (8 caixas de 200 g)", 4),
    ("Embalagens", "Forminha de brigadeiro", 6.00, 200, "unidades", "Quarta compra", 4),
    # —— quinta compra ——
    ("Utilidades", "Saco de confeitar", 13.99, 25, "unidades", "Quinta compra", 5),
    ("Ingredientes", "Coco em flocos", 25.99, 500, "gramas", "Quinta compra", 5),
    ("Ingredientes", "Biscoito Oreo", 11.99, 270, "gramas", "Quinta compra", 5),
    ("Embalagens", "Pote redondo 150 ml", 11.59, 1, "pacote", "Quinta compra (qtd. do pacote não informada)", 5),
    ("Embalagens", "Saco Kraft", 14.99, 20, "unidades", "Quinta compra", 5),
    ("Utilidades", "Carimbo de brigadeiros", 15.99, 1, "unidade", "Quinta compra", 5),
    ("Ingredientes", "Leite condensado", 69.90, 3950, "gramas", "Quinta compra (10 latas de 395 g)", 5),
    ("Ingredientes", "Creme de leite", 18.83, 1400, "gramas", "Quinta compra (7 caixas de 200 g)", 5),
    ("Embalagens", "Embalagem quadrada de brigadeiro", 18.00, 15, "unidades", "Quinta compra", 5),
    # —— sexta compra ——
    ("Ingredientes", "Barra de chocolate", 30.00, 1, "unidade", "Sexta compra (peso não informado)", 6),
]

ADICIONAIS = [
    ("Granulado", 5),
    ("Morango", 16),
    ("Biscoito Oreo", 9),
    ("Pacotão de Bis", 6),
    ("Paçoca", 5),
    ("Leite em pó", 9),
]


def fill(hex_color):
    return PatternFill("solid", fgColor=hex_color)


def thin(color="B5B5B5"):
    s = Side(style="thin", color=color)
    return Border(left=s, right=s, top=s, bottom=s)


def money2(cell):
    cell.number_format = '"R$" #,##0.00'
    cell.alignment = Alignment(horizontal="right", vertical="center")


def money4(cell):
    cell.number_format = '"R$" #,##0.0000'
    cell.alignment = Alignment(horizontal="right", vertical="center")


def build():
    wb = Workbook()
    ws = wb.active
    ws.title = "Compras e Custos"
    ws.sheet_view.showGridLines = False
    ws.page_setup.orientation = "landscape"
    ws.page_setup.paperSize = ws.PAPERSIZE_A4
    ws.page_setup.fitToPage = True
    ws.page_setup.fitToWidth = 1
    ws.page_setup.fitToHeight = 1
    ws.page_setup.horizontalCentered = True
    ws.page_margins = PageMargins(left=0.4, right=0.4, top=0.4, bottom=0.4)
    ws.sheet_properties.pageSetUpPr.fitToPage = True
    ws.sheet_properties.tabColor = VINHO
    ws.print_title_rows = "1:4"

    # —— título ——
    ws.merge_cells("A1:G1")
    ws["A1"] = "CONTROLE DE COMPRAS — CONFEITARIA"
    ws["A1"].font = Font(name="Calibri", bold=True, size=16, color=BRANCO)
    ws["A1"].fill = fill(VINHO)
    ws["A1"].alignment = Alignment(horizontal="center", vertical="center")
    ws.row_dimensions[1].height = 24
    for col in range(1, 8):
        ws.cell(1, col).fill = fill(VINHO)

    ws.merge_cells("A2:G2")
    ws["A2"] = "Base de custos inicial • atualize os valores sempre que fizer novas compras"
    ws["A2"].font = Font(name="Calibri", italic=True, size=10, color=VINHO)
    ws["A2"].alignment = Alignment(horizontal="center", vertical="center")
    ws.row_dimensions[2].height = 16

    ws.row_dimensions[3].height = 8

    headers = [
        "Categoria",
        "Produto",
        "Preço pago (R$)",
        "Quantidade comprada",
        "Unidade / conteúdo",
        "Custo unitário (R$)",
        "Observações",
    ]
    for i, h in enumerate(headers, 1):
        cell = ws.cell(4, i, h)
        cell.font = Font(name="Calibri", bold=True, size=10, color=BRANCO)
        cell.fill = fill(VINHO2)
        cell.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True)
        cell.border = thin("8A2E5A")
    ws.row_dimensions[4].height = 22

    first = 5
    for idx, (cat, prod, preco, qtd, unid, obs, grupo) in enumerate(ITENS):
        r = first + idx
        ws.cell(r, 1, cat)
        ws.cell(r, 2, prod)
        ws.cell(r, 3, preco)
        ws.cell(r, 4, qtd)
        ws.cell(r, 5, unid)
        ws.cell(r, 6, f"=IF(D{r}=0,0,C{r}/D{r})")
        ws.cell(r, 7, obs)

        if grupo == 2:
            bg, fg, money_fg = (CINZA1, BRANCO, AMARELO) if idx % 2 == 0 else (CINZA2, BRANCO, AMARELO)
        elif grupo == 3:
            bg, fg, money_fg = ROSA, PRETO, PRETO
        elif grupo == 4:
            bg, fg, money_fg = ROSA2, PRETO, PRETO
        elif grupo == 5:
            bg, fg, money_fg = ROSA, PRETO, PRETO
        elif grupo == 6:
            bg, fg, money_fg = ROSA2, PRETO, PRETO
        else:
            bg, fg, money_fg = BRANCO, PRETO, PRETO

        for c in range(1, 8):
            cell = ws.cell(r, c)
            cell.fill = fill(bg)
            cell.font = Font(name="Calibri", size=10, color=fg)
            cell.border = thin("D0D0D0")
            cell.alignment = Alignment(vertical="center")
        ws.cell(r, 1).alignment = Alignment(horizontal="left", vertical="center")
        ws.cell(r, 3).font = Font(name="Calibri", size=10, color=money_fg)
        ws.cell(r, 6).font = Font(name="Calibri", size=10, color=money_fg)
        money2(ws.cell(r, 3))
        ws.cell(r, 4).alignment = Alignment(horizontal="right", vertical="center")
        if isinstance(qtd, float) and not qtd.is_integer():
            ws.cell(r, 4).number_format = "0.0"
        else:
            ws.cell(r, 4).number_format = "0"
        money4(ws.cell(r, 6))
        ws.row_dimensions[r].height = 16

    last = first + len(ITENS) - 1

    ws.auto_filter.ref = f"A4:G{last}"
    ws.freeze_panes = "A5"

    widths = {"A": 16, "B": 44, "C": 16, "D": 20, "E": 20, "F": 18, "G": 48, "H": 3, "I": 28, "J": 14}
    for col, w in widths.items():
        ws.column_dimensions[col].width = w

    # —— PARÂMETROS ——
    ws.merge_cells("I1:J1")
    ws["I1"] = "PARÂMETROS"
    ws["I1"].font = Font(name="Calibri", bold=True, size=11, color=BRANCO)
    ws["I1"].fill = fill(VINHO)
    ws["I1"].alignment = Alignment(horizontal="left", vertical="center")
    ws["J1"].fill = fill(VINHO)
    ws["I2"] = "Acréscimo luz e gás"
    ws["I2"].font = Font(name="Calibri", size=10)
    ws["J2"] = 0.20
    ws["J2"].number_format = "0%"
    ws["J2"].font = Font(name="Calibri", size=10)
    ws["J2"].alignment = Alignment(horizontal="right")

    # —— RESUMO ——
    ws.merge_cells("I4:J4")
    ws["I4"] = "RESUMO"
    ws["I4"].font = Font(name="Calibri", bold=True, size=11, color=BRANCO)
    ws["I4"].fill = fill(VINHO)
    ws["J4"].fill = fill(VINHO)

    ws["I5"] = "Subtotal das compras"
    ws["J5"] = f"=SUM(C{first}:C{last})"
    money2(ws["J5"])
    ws["J5"].font = Font(name="Calibri", size=10)

    ws["I6"] = "Luz e gás (20%)"
    ws["J6"] = "=J5*J2"
    money2(ws["J6"])
    ws["J6"].font = Font(name="Calibri", size=10)

    ws["I7"] = "Custo total c/ luz e gás"
    ws["J7"] = "=J5+J6"
    money2(ws["J7"])
    ws["I7"].font = Font(name="Calibri", bold=True, size=10)
    ws["J7"].font = Font(name="Calibri", bold=True, size=11)

    for r in range(5, 8):
        ws.cell(r, 9).font = Font(
            name="Calibri",
            size=10,
            bold=(r == 7),
        )
        ws.cell(r, 9).alignment = Alignment(vertical="center")

    # gasto só das compras novas (3ª–6ª), para conferência
    ws["I8"] = "Só 3ª a 6ª compra"
    ws["J8"] = f'=SUMIF(G{first}:G{last},"*Terceira*",C{first}:C{last})+SUMIF(G{first}:G{last},"*Quarta*",C{first}:C{last})+SUMIF(G{first}:G{last},"*Quinta*",C{first}:C{last})+SUMIF(G{first}:G{last},"*Sexta*",C{first}:C{last})'
    money2(ws["J8"])
    ws["I8"].font = Font(name="Calibri", italic=True, size=9, color=CINZA_TXT)
    ws["J8"].font = Font(name="Calibri", italic=True, size=9, color=CINZA_TXT)

    # —— Adicionais ——
    ws["I10"] = "Adicionais"
    ws["J10"] = "Peso (g)"
    for c in (9, 10):
        ws.cell(10, c).font = Font(name="Calibri", bold=True, size=10, color=BRANCO)
        ws.cell(10, c).fill = fill(AZUL)
        ws.cell(10, c).alignment = Alignment(horizontal="center", vertical="center")
    for i, (nome, peso) in enumerate(ADICIONAIS):
        r = 11 + i
        bg = AZUL3 if i % 2 == 0 else AZUL2
        ws.cell(r, 9, nome)
        ws.cell(r, 10, peso)
        for c in (9, 10):
            ws.cell(r, c).fill = fill(bg)
            ws.cell(r, c).font = Font(name="Calibri", size=10)
            ws.cell(r, c).border = thin("B8CCE4")
        ws.cell(r, 10).alignment = Alignment(horizontal="center", vertical="center")

    # nota
    nota_row = last + 2
    ws.merge_cells(f"A{nota_row}:G{nota_row}")
    ws[f"A{nota_row}"] = (
        "PRÓXIMO PASSO: envie as receitas para calcular o custo de cada doce e sugerir preço de venda."
    )
    ws[f"A{nota_row}"].font = Font(name="Calibri", italic=True, size=10, color=VINHO)
    ws[f"A{nota_row}"].fill = fill(ROSA)
    ws[f"A{nota_row}"].alignment = Alignment(horizontal="left", vertical="center")
    for c in range(1, 8):
        ws.cell(nota_row, c).fill = fill(ROSA)
    ws.row_dimensions[nota_row].height = 18

    # aba resumo por compra (extra, não muda o modelo principal)
    rs = wb.create_sheet("Por compra")
    rs.sheet_properties.tabColor = VINHO2
    rs["A1"] = "GASTO POR COMPRA"
    rs["A1"].font = Font(name="Calibri", bold=True, size=14, color=BRANCO)
    rs.merge_cells("A1:C1")
    rs["A1"].fill = fill(VINHO)
    rs["B1"].fill = fill(VINHO)
    rs["C1"].fill = fill(VINHO)
    for i, h in enumerate(["Compra", "Itens", "Total (R$)"], 1):
        cell = rs.cell(3, i, h)
        cell.font = Font(name="Calibri", bold=True, size=10, color=BRANCO)
        cell.fill = fill(VINHO2)
        cell.alignment = Alignment(horizontal="center")
    compras = [
        (1, "1ª — compra inicial", ""),
        (2, "2ª — segunda compra", "Segunda"),
        (3, "3ª — terceira compra", "Terceira"),
        (4, "4ª — quarta compra", "Quarta"),
        (5, "5ª — quinta compra", "Quinta"),
        (6, "6ª — sexta compra", "Sexta"),
    ]
    # inicial = linhas sem observação de compra nomeada e não segunda
    rs["A4"] = "1ª — compra inicial"
    rs["B4"] = f"=COUNTIFS('Compras e Custos'!G{first}:G{last},\"\")"
    rs["C4"] = f"=SUMIFS('Compras e Custos'!C{first}:C{last},'Compras e Custos'!G{first}:G{last},\"\")"
    money2(rs["C4"])
    for i, (n, nome, key) in enumerate(compras[1:], start=5):
        rs.cell(i, 1, nome)
        rs.cell(i, 2, f"=COUNTIF('Compras e Custos'!G{first}:G{last},\"*{key}*\")")
        rs.cell(i, 3, f"=SUMIF('Compras e Custos'!G{first}:G{last},\"*{key}*\",'Compras e Custos'!C{first}:C{last})")
        money2(rs.cell(i, 3))
    rs["A10"] = "TOTAL GERAL"
    rs["B10"] = "=SUM(B4:B9)"
    rs["C10"] = "=SUM(C4:C9)"
    money2(rs["C10"])
    for c in range(1, 4):
        rs.cell(10, c).fill = fill(VINHO)
        rs.cell(10, c).font = Font(name="Calibri", bold=True, size=11, color=BRANCO)
    for r in range(4, 10):
        for c in range(1, 4):
            rs.cell(r, c).font = Font(name="Calibri", size=11)
            rs.cell(r, c).border = thin()
            rs.cell(r, c).alignment = Alignment(vertical="center")
        rs.cell(r, 2).alignment = Alignment(horizontal="center")
    rs.column_dimensions["A"].width = 28
    rs.column_dimensions["B"].width = 10
    rs.column_dimensions["C"].width = 14

    out = "/home/user/calculadora-boleado/planilha_custos_confeitaria_atualizada.xlsx"
    wb.save(out)
    print("saved", out, "rows", first, last, "itens", len(ITENS))
    print("subtotal", round(sum(i[2] for i in ITENS), 2))
    print("novas", round(sum(i[2] for i in ITENS if i[6] >= 3), 2))
    return out


if __name__ == "__main__":
    build()
