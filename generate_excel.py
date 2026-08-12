#!/usr/bin/env python3
"""Gera a planilha Compras e Custos da Calculadora Boleado."""

from openpyxl import Workbook
from openpyxl.styles import (
    Alignment,
    Border,
    Font,
    NamedStyle,
    PatternFill,
    Side,
    numbers,
)
from openpyxl.chart import BarChart, PieChart, Reference
from openpyxl.chart.label import DataLabelList
from openpyxl.chart.series import DataPoint
from openpyxl.drawing.fill import PatternFillProperties, ColorChoice
from openpyxl.utils import get_column_letter
from openpyxl.formatting.rule import FormulaRule
from openpyxl.workbook.child import INVALID_TITLE_REGEX
from openpyxl.chart.shapes import GraphicalProperties
from openpyxl.drawing.line import LineProperties
from openpyxl.chart.legend import Legend
from copy import copy

COMPRAS = [
    {
        "compra": 3,
        "nome": "Terceira compra",
        "itens": [
            {
                "produto": "Chocolate em pó",
                "detalhe": "200 g",
                "categoria": "Ingrediente",
                "qtd": 1,
                "unidade": "pct",
                "conteudo": "200 g",
                "valor": 39.90,
            },
            {
                "produto": "Margarina",
                "detalhe": "250 g",
                "categoria": "Ingrediente",
                "qtd": 1,
                "unidade": "pote",
                "conteudo": "250 g",
                "valor": 6.98,
            },
        ],
    },
    {
        "compra": 4,
        "nome": "Quarta compra",
        "itens": [
            {
                "produto": "Caixa para cento de doces",
                "detalhe": "3 unidades",
                "categoria": "Embalagem",
                "qtd": 3,
                "unidade": "un",
                "conteudo": "3 un",
                "valor": 10.47,
            },
            {
                "produto": "Granulado",
                "detalhe": "",
                "categoria": "Ingrediente",
                "qtd": 1,
                "unidade": "pct",
                "conteudo": "—",
                "valor": 19.99,
            },
            {
                "produto": "Chocolate em pó 50%",
                "detalhe": "",
                "categoria": "Ingrediente",
                "qtd": 1,
                "unidade": "pct",
                "conteudo": "—",
                "valor": 38.99,
            },
            {
                "produto": "Leite condensado",
                "detalhe": "6 unidades",
                "categoria": "Ingrediente",
                "qtd": 6,
                "unidade": "lata",
                "conteudo": "6 un",
                "valor": 41.44,
            },
            {
                "produto": "Creme de leite",
                "detalhe": "8 unidades",
                "categoria": "Ingrediente",
                "qtd": 8,
                "unidade": "cx",
                "conteudo": "8 un",
                "valor": 21.52,
            },
            {
                "produto": "Forminha de brigadeiro",
                "detalhe": "200 unidades",
                "categoria": "Embalagem",
                "qtd": 200,
                "unidade": "un",
                "conteudo": "200 un",
                "valor": 6.00,
            },
        ],
    },
    {
        "compra": 5,
        "nome": "Quinta compra",
        "itens": [
            {
                "produto": "Saco de confeitar",
                "detalhe": "25 unidades",
                "categoria": "Utensílio",
                "qtd": 25,
                "unidade": "un",
                "conteudo": "25 un",
                "valor": 13.99,
            },
            {
                "produto": "Coco em flocos",
                "detalhe": "500 g",
                "categoria": "Ingrediente",
                "qtd": 1,
                "unidade": "pct",
                "conteudo": "500 g",
                "valor": 25.99,
            },
            {
                "produto": "Biscoito Oreo",
                "detalhe": "270 g",
                "categoria": "Ingrediente",
                "qtd": 1,
                "unidade": "pct",
                "conteudo": "270 g",
                "valor": 11.99,
            },
            {
                "produto": "Pote redondo 150 ml",
                "detalhe": "",
                "categoria": "Embalagem",
                "qtd": 1,
                "unidade": "un",
                "conteudo": "150 ml",
                "valor": 11.59,
            },
            {
                "produto": "Saco Kraft",
                "detalhe": "20 unidades",
                "categoria": "Embalagem",
                "qtd": 20,
                "unidade": "un",
                "conteudo": "20 un",
                "valor": 14.99,
            },
            {
                "produto": "Carimbo de brigadeiros",
                "detalhe": "",
                "categoria": "Utensílio",
                "qtd": 1,
                "unidade": "un",
                "conteudo": "1 un",
                "valor": 15.99,
            },
            {
                "produto": "Leite condensado",
                "detalhe": "10 unidades",
                "categoria": "Ingrediente",
                "qtd": 10,
                "unidade": "lata",
                "conteudo": "10 un",
                "valor": 69.90,
            },
            {
                "produto": "Creme de leite",
                "detalhe": "7 unidades",
                "categoria": "Ingrediente",
                "qtd": 7,
                "unidade": "cx",
                "conteudo": "7 un",
                "valor": 18.83,
            },
            {
                "produto": "Embalagem quadrada para brigadeiro",
                "detalhe": "15 unidades",
                "categoria": "Embalagem",
                "qtd": 15,
                "unidade": "un",
                "conteudo": "15 un",
                "valor": 18.00,
            },
        ],
    },
    {
        "compra": 6,
        "nome": "Sexta compra",
        "itens": [
            {
                "produto": "Barra de chocolate",
                "detalhe": "",
                "categoria": "Ingrediente",
                "qtd": 1,
                "unidade": "un",
                "conteudo": "—",
                "valor": 30.00,
            },
        ],
    },
]


CHOCOLATE = "3B2414"
CACAU = "5C3317"
CARAMELO = "C4A35A"
CREME = "F7F0E4"
CREME2 = "EFE4D0"
OFFWHITE = "FFFBF4"
VINHO = "7A2E1F"
VERDE = "2F5D3A"
TEXTO = "2A2118"
MUTED = "7A6A58"
LINHA = "D9CBB4"
BRANCO = "FFFFFF"


def money(cell):
    cell.number_format = '"R$" #,##0.00'
    cell.alignment = Alignment(horizontal="right", vertical="center")


def qty(cell):
    cell.number_format = "#,##0"
    cell.alignment = Alignment(horizontal="center", vertical="center")


def thin_border():
    s = Side(style="thin", color=LINHA)
    return Border(left=s, right=s, top=s, bottom=s)


def thick_bottom():
    s = Side(style="thin", color=LINHA)
    b = Side(style="medium", color=CHOCOLATE)
    return Border(left=s, right=s, top=s, bottom=b)


def fill(hex_color):
    return PatternFill("solid", fgColor=hex_color)


def apply_header(ws, row, cols, bg=CHOCOLATE, fg=CREME):
    for col in range(1, cols + 1):
        cell = ws.cell(row, col)
        cell.fill = fill(bg)
        cell.font = Font(name="Calibri", bold=True, color=fg, size=11)
        cell.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True)
        cell.border = thin_border()


def build():
    wb = Workbook()

    # ---------- Compras e Custos ----------
    ws = wb.active
    ws.title = "Compras e Custos"
    ws.sheet_properties.tabColor = CHOCOLATE
    ws.page_setup.orientation = "landscape"
    ws.page_setup.fitToPage = True
    ws.page_setup.fitToWidth = 1
    ws.page_setup.fitToHeight = 1
    ws.page_setup.paperSize = ws.PAPERSIZE_A4
    ws.print_title_rows = "1:5"
    ws.page_setup.horizontalCentered = True
    ws.oddHeader.left.text = "Calculadora Boleado"
    ws.oddFooter.center.text = "Página &P de &N"

    ws.merge_cells("A1:J1")
    ws["A1"] = "CALCULADORA BOLEADO  ·  COMPRAS E CUSTOS"
    ws["A1"].font = Font(name="Calibri", bold=True, size=20, color=CHOCOLATE)
    ws["A1"].alignment = Alignment(horizontal="left", vertical="center")
    ws["A1"].fill = fill(CREME)

    ws.merge_cells("A2:J2")
    ws["A2"] = "Terceira à sexta compra  ·  Ingredientes, embalagens e utensílios"
    ws["A2"].font = Font(name="Calibri", italic=True, size=11, color=MUTED)
    ws["A2"].fill = fill(CREME)
    ws["A2"].alignment = Alignment(horizontal="left", vertical="center")

    headers = [
        "Nº",
        "Compra",
        "Produto",
        "Detalhe / conteúdo",
        "Categoria",
        "Qtd",
        "Unidade",
        "Valor total (R$)",
        "Custo unitário (R$)",
        "% do total",
    ]
    for i, h in enumerate(headers, 1):
        ws.cell(4, i, h)
    apply_header(ws, 4, 10)
    ws.row_dimensions[1].height = 28
    ws.row_dimensions[2].height = 18
    ws.row_dimensions[3].height = 8
    ws.row_dimensions[4].height = 22

    row = 5
    first_data = 5
    zebra = [OFFWHITE, CREME]
    for compra in COMPRAS:
        for item in compra["itens"]:
            bg = zebra[(row - first_data) % 2]
            values = [
                compra["compra"],
                compra["nome"],
                item["produto"],
                item["detalhe"] or item["conteudo"],
                item["categoria"],
                item["qtd"],
                item["unidade"],
                item["valor"],
                item["valor"] / item["qtd"] if item["qtd"] else item["valor"],
            ]
            for col, val in enumerate(values, 1):
                cell = ws.cell(row, col, val)
                cell.fill = fill(bg)
                cell.font = Font(name="Calibri", size=11, color=TEXTO)
                cell.alignment = Alignment(vertical="center")
                cell.border = thin_border()
            ws.cell(row, 1).alignment = Alignment(horizontal="center", vertical="center")
            ws.cell(row, 5).alignment = Alignment(horizontal="center", vertical="center")
            ws.cell(row, 6).alignment = Alignment(horizontal="center", vertical="center")
            ws.cell(row, 7).alignment = Alignment(horizontal="center", vertical="center")
            money(ws.cell(row, 8))
            money(ws.cell(row, 9))
            # % do total — fórmula para o Excel recalcular se editar
            pct = ws.cell(row, 10)
            pct.value = f"=IF($H$24=0,0,H{row}/$H$24)"
            pct.number_format = "0.0%"
            pct.alignment = Alignment(horizontal="center", vertical="center")
            pct.fill = fill(bg)
            pct.font = Font(name="Calibri", size=11, color=TEXTO)
            pct.border = thin_border()
            row += 1

    last_data = row - 1  # 22

    # Totals row
    ws.merge_cells(f"A{row}:G{row}")
    total_label = ws.cell(row, 1, "TOTAL GERAL GASTO")
    total_label.font = Font(name="Calibri", bold=True, size=12, color=CREME)
    total_label.fill = fill(CHOCOLATE)
    total_label.alignment = Alignment(horizontal="right", vertical="center")
    for c in range(1, 11):
        ws.cell(row, c).fill = fill(CHOCOLATE)
        ws.cell(row, c).border = thin_border()
        ws.cell(row, c).font = Font(name="Calibri", bold=True, size=12, color=CREME)
    ws.cell(row, 8, f"=SUM(H{first_data}:H{last_data})")
    money(ws.cell(row, 8))
    ws.cell(row, 8).font = Font(name="Calibri", bold=True, size=13, color=CARAMELO)
    ws.cell(row, 8).fill = fill(CHOCOLATE)
    ws.cell(row, 10, f"=SUM(J{first_data}:J{last_data})")
    ws.cell(row, 10).number_format = "0%"
    ws.cell(row, 10).font = Font(name="Calibri", bold=True, size=12, color=CREME)
    ws.cell(row, 10).fill = fill(CHOCOLATE)
    ws.cell(row, 10).alignment = Alignment(horizontal="center", vertical="center")
    ws.row_dimensions[row].height = 24
    total_row = row  # 23 — wait I said H24 in formulas

    # I used $H$24 in percent formulas. Let me recalculate:
    # headers at 4, data starts 5
    # items: 2 + 6 + 9 + 1 = 18 items
    # last_data = 5+18-1 = 22
    # total_row = 23
    # So formula should be $H$23 not $H$24!

    for r in range(first_data, last_data + 1):
        ws.cell(r, 10).value = f"=IF($H${total_row}=0,0,H{r}/$H${total_row})"

    # freeze
    ws.freeze_panes = "A5"
    ws.auto_filter.ref = f"A4:J{last_data}"

    widths = {
        "A": 6,
        "B": 18,
        "C": 38,
        "D": 20,
        "E": 14,
        "F": 8,
        "G": 10,
        "H": 18,
        "I": 20,
        "J": 12,
    }
    for col, w in widths.items():
        ws.column_dimensions[col].width = w

    ws.auto_filter.ref = f"A4:J{last_data}"

    # Note
    note_row = total_row + 2
    ws.merge_cells(f"A{note_row}:J{note_row}")
    ws[f"A{note_row}"] = (
        "Observação: 1ª e 2ª compras ainda não foram lançadas. "
        "Edite os valores na coluna H — o total e os percentuais atualizam sozinhos. "
        "Use o filtro da linha 4 para ver só uma compra ou categoria."
    )
    ws[f"A{note_row}"].font = Font(name="Calibri", italic=True, size=9, color=MUTED)

    # Print area
    ws.print_area = f"A1:J{note_row}"

    # ---------- Resumo por compra ----------
    rs = wb.create_sheet("Resumo por compra")
    rs.sheet_properties.tabColor = CARAMELO
    rs.merge_cells("A1:D1")
    rs["A1"] = "GASTO POR COMPRA"
    rs["A1"].font = Font(name="Calibri", bold=True, size=18, color=CHOCOLATE)
    rs["A1"].fill = fill(CREME)
    rs.row_dimensions[1].height = 26
    rs["A2"] = "Consolidado da terceira à sexta compra"
    rs["A2"].font = Font(name="Calibri", italic=True, size=11, color=MUTED)
    rs.merge_cells("A2:D2")
    rs["A2"].fill = fill(CREME)

    for i, h in enumerate(["Nº", "Compra", "Itens", "Total (R$)"], 1):
        rs.cell(4, i, h)
    apply_header(rs, 4, 4)

    # Use SUMIF against the detail sheet so Excel stays live
    resumo_start = 5
    for idx, compra in enumerate(COMPRAS):
        r = resumo_start + idx
        rs.cell(r, 1, compra["compra"]).alignment = Alignment(horizontal="center", vertical="center")
        rs.cell(r, 2, compra["nome"])
        rs.cell(r, 3, f"=COUNTIF('Compras e Custos'!A:A,A{r})")
        rs.cell(r, 3).alignment = Alignment(horizontal="center", vertical="center")
        rs.cell(r, 4, f"=SUMIF('Compras e Custos'!A:A,A{r},'Compras e Custos'!H:H)")
        money(rs.cell(r, 4))
        bg = OFFWHITE if idx % 2 == 0 else CREME
        for c in range(1, 5):
            rs.cell(r, c).fill = fill(bg)
            rs.cell(r, c).font = Font(name="Calibri", size=12, color=TEXTO)
            rs.cell(r, c).border = thin_border()
            rs.cell(r, c).alignment = Alignment(vertical="center")
        rs.cell(r, 1).alignment = Alignment(horizontal="center", vertical="center")
        rs.cell(r, 3).alignment = Alignment(horizontal="center", vertical="center")
        rs.row_dimensions[r].height = 20

    resumo_end = resumo_start + len(COMPRAS) - 1
    tot = resumo_end + 1
    rs.merge_cells(f"A{tot}:C{tot}")
    rs.cell(tot, 1, "TOTAL GERAL")
    rs.cell(tot, 1).font = Font(name="Calibri", bold=True, size=12, color=CREME)
    rs.cell(tot, 1).fill = fill(CHOCOLATE)
    rs.cell(tot, 1).alignment = Alignment(horizontal="right", vertical="center")
    for c in range(1, 5):
        rs.cell(tot, c).fill = fill(CHOCOLATE)
        rs.cell(tot, c).border = thin_border()
        rs.cell(tot, c).font = Font(name="Calibri", bold=True, size=12, color=CREME)
    rs.cell(tot, 4, f"=SUM(D{resumo_start}:D{resumo_end})")
    money(rs.cell(tot, 4))
    rs.cell(tot, 4).font = Font(name="Calibri", bold=True, size=13, color=CARAMELO)
    rs.cell(tot, 4).fill = fill(CHOCOLATE)

    rs.column_dimensions["A"].width = 8
    rs.column_dimensions["B"].width = 22
    rs.column_dimensions["C"].width = 12
    rs.column_dimensions["D"].width = 16

    chart = BarChart()
    chart.type = "col"
    chart.title = "Gasto por compra (R$)"
    chart.y_axis.title = "Reais"
    chart.style = 10
    data = Reference(rs, min_col=4, min_row=4, max_row=resumo_end)
    cats = Reference(rs, min_col=2, min_row=resumo_start, max_row=resumo_end)
    chart.add_data(data, titles_from_data=True)
    chart.set_categories(cats)
    chart.shape = 4
    chart.legend = None
    chart.y_axis.numFmt = '"R$" #,##0'
    chart.width = 15
    chart.height = 8
    rs.add_chart(chart, "F3")

    # ---------- Resumo por categoria ----------
    cs = wb.create_sheet("Resumo por categoria")
    cs.sheet_properties.tabColor = VINHO
    cs.merge_cells("A1:C1")
    cs["A1"] = "GASTO POR CATEGORIA"
    cs["A1"].font = Font(name="Calibri", bold=True, size=18, color=CHOCOLATE)
    cs["A1"].fill = fill(CREME)
    cs.row_dimensions[1].height = 26
    cs.merge_cells("A2:C2")
    cs["A2"] = "Como o dinheiro se divide entre ingredientes, embalagens e utensílios"
    cs["A2"].font = Font(name="Calibri", italic=True, size=11, color=MUTED)
    cs["A2"].fill = fill(CREME)

    for i, h in enumerate(["Categoria", "Total (R$)", "% do gasto"], 1):
        cs.cell(4, i, h)
    apply_header(cs, 4, 3)

    categorias = ["Ingrediente", "Embalagem", "Utensílio"]
    for idx, cat in enumerate(categorias):
        r = 5 + idx
        cs.cell(r, 1, cat)
        cs.cell(r, 2, f"=SUMIF('Compras e Custos'!E:E,A{r},'Compras e Custos'!H:H)")
        money(cs.cell(r, 2))
        cs.cell(r, 3, f"=IF($B$8=0,0,B{r}/$B$8)")
        cs.cell(r, 3).number_format = "0.0%"
        cs.cell(r, 3).alignment = Alignment(horizontal="center", vertical="center")
        bg = OFFWHITE if idx % 2 == 0 else CREME
        for c in range(1, 4):
            cs.cell(r, c).fill = fill(bg)
            cs.cell(r, c).font = Font(name="Calibri", size=12, color=TEXTO)
            cs.cell(r, c).border = thin_border()
            cs.cell(r, c).alignment = Alignment(vertical="center")
        cs.cell(r, 3).alignment = Alignment(horizontal="center", vertical="center")

    cs.merge_cells("A8:A8")
    cs.cell(8, 1, "TOTAL")
    cs.cell(8, 2, "=SUM(B5:B7)")
    money(cs.cell(8, 2))
    cs.cell(8, 3, "=SUM(C5:C7)")
    cs.cell(8, 3).number_format = "0%"
    for c in range(1, 4):
        cs.cell(8, c).fill = fill(CHOCOLATE)
        cs.cell(8, c).font = Font(name="Calibri", bold=True, size=12, color=CREME)
        cs.cell(8, c).border = thin_border()
        cs.cell(8, c).alignment = Alignment(vertical="center")
    cs.cell(8, 2).font = Font(name="Calibri", bold=True, size=13, color=CARAMELO)
    cs.cell(8, 2).fill = fill(CHOCOLATE)
    cs.cell(8, 3).alignment = Alignment(horizontal="center", vertical="center")

    cs.column_dimensions["A"].width = 18
    cs.column_dimensions["B"].width = 16
    cs.column_dimensions["C"].width = 14

    pie = PieChart()
    pie.title = "Participação no gasto"
    labels = Reference(cs, min_col=1, min_row=5, max_row=7)
    pdata = Reference(cs, min_col=2, min_row=4, max_row=7)
    pie.add_data(pdata, titles_from_data=True)
    pie.set_categories(labels)
    pie.dataLabels = DataLabelList()
    pie.dataLabels.showPercent = True
    pie.dataLabels.showVal = False
    pie.dataLabels.showCatName = True
    pie.width = 14
    pie.height = 9
    cs.add_chart(pie, "E3")

    # ---------- Estoque consolidado ----------
    es = wb.create_sheet("Estoque consolidado")
    es.sheet_properties.tabColor = VERDE
    es.merge_cells("A1:E1")
    es["A1"] = "MESMOS PRODUTOS SOMADOS ENTRE COMPRAS"
    es["A1"].font = Font(name="Calibri", bold=True, size=18, color=CHOCOLATE)
    es["A1"].fill = fill(CREME)
    es.row_dimensions[1].height = 26
    es.merge_cells("A2:E2")
    es["A2"] = "Útil para ver quanto já entrou de leite condensado, creme de leite etc."
    es["A2"].font = Font(name="Calibri", italic=True, size=11, color=MUTED)
    es["A2"].fill = fill(CREME)

    for i, h in enumerate(
        ["Produto", "Categoria", "Qtd total", "Gasto total (R$)", "Custo médio unit. (R$)"], 1
    ):
        es.cell(4, i, h)
    apply_header(es, 4, 5)

    # Unique products in order of first appearance
    seen = []
    for compra in COMPRAS:
        for item in compra["itens"]:
            if item["produto"] not in seen:
                seen.append(item["produto"])

    for idx, nome in enumerate(seen):
        r = 5 + idx
        # category from first occurrence
        cat = next(
            item["categoria"]
            for compra in COMPRAS
            for item in compra["itens"]
            if item["produto"] == nome
        )
        es.cell(r, 1, nome)
        es.cell(r, 2, cat)
        es.cell(r, 3, f"=SUMIF('Compras e Custos'!C:C,A{r},'Compras e Custos'!F:F)")
        es.cell(r, 4, f"=SUMIF('Compras e Custos'!C:C,A{r},'Compras e Custos'!H:H)")
        es.cell(r, 5, f"=IF(C{r}=0,0,D{r}/C{r})")
        money(es.cell(r, 4))
        money(es.cell(r, 5))
        es.cell(r, 3).alignment = Alignment(horizontal="center", vertical="center")
        es.cell(r, 2).alignment = Alignment(horizontal="center", vertical="center")
        bg = OFFWHITE if idx % 2 == 0 else CREME
        for c in range(1, 6):
            es.cell(r, c).fill = fill(bg)
            es.cell(r, c).font = Font(name="Calibri", size=11, color=TEXTO)
            es.cell(r, c).border = thin_border()
            es.cell(r, c).alignment = Alignment(vertical="center")
        es.cell(r, 2).alignment = Alignment(horizontal="center", vertical="center")
        es.cell(r, 3).alignment = Alignment(horizontal="center", vertical="center")

    last_est = 4 + len(seen)
    tot_e = last_est + 1
    es.merge_cells(f"A{tot_e}:C{tot_e}")
    es.cell(tot_e, 1, "TOTAL GERAL")
    es.cell(tot_e, 4, f"=SUM(D5:D{last_est})")
    money(es.cell(tot_e, 4))
    for c in range(1, 6):
        es.cell(tot_e, c).fill = fill(CHOCOLATE)
        es.cell(tot_e, c).font = Font(name="Calibri", bold=True, size=12, color=CREME)
        es.cell(tot_e, c).border = thin_border()
    es.cell(tot_e, 1).alignment = Alignment(horizontal="right", vertical="center")
    es.cell(tot_e, 4).font = Font(name="Calibri", bold=True, size=13, color=CARAMELO)
    es.cell(tot_e, 4).fill = fill(CHOCOLATE)

    es.column_dimensions["A"].width = 40
    es.column_dimensions["B"].width = 14
    es.column_dimensions["C"].width = 12
    es.column_dimensions["D"].width = 20
    es.column_dimensions["E"].width = 24

    # ---------- Instruções ----------
    ins = wb.create_sheet("Como usar")
    ins.sheet_properties.tabColor = MUTED
    ins.merge_cells("A1:B1")
    ins["A1"] = "COMO USAR ESTA PLANILHA"
    ins["A1"].font = Font(name="Calibri", bold=True, size=18, color=CHOCOLATE)
    ins["A1"].fill = fill(CREME)
    ins.row_dimensions[1].height = 26
    instrucoes = [
        ("Abrir no Excel", "Arquivo → Abrir, ou dê dois cliques neste .xlsx. No celular, use Excel ou Google Planilhas."),
        ("Abrir no Google Planilhas", "Drive → Novo → Upload → este arquivo. Ou Planilhas → Arquivo → Importar."),
        ("Lançar 1ª e 2ª compras", "Na aba Compras e Custos, insira linhas acima do TOTAL (antes da linha 23). Copie uma linha existente para manter fórmulas de %."),
        ("Corrigir um preço", "Altere só a coluna H (Valor total). Custo unitário e % recalculam sozinhos."),
        ("Filtrar", "Use as setinhas da linha 4 para ver só uma compra, só ingredientes, etc."),
        ("Resumos", "As abas Resumo por compra, Resumo por categoria e Estoque consolidado leem a aba principal com fórmulas — não edite os totais à mão."),
        ("Total atual", "Terceira + quarta + quinta + sexta compra = R$ 416,56."),
    ]
    ins["A3"] = "Tópico"
    ins["B3"] = "O que fazer"
    apply_header(ins, 3, 2)
    for i, (t, d) in enumerate(instrucoes):
        r = 4 + i
        ins.cell(r, 1, t).font = Font(name="Calibri", bold=True, size=11, color=CHOCOLATE)
        ins.cell(r, 2, d).font = Font(name="Calibri", size=11, color=TEXTO)
        ins.cell(r, 2).alignment = Alignment(wrap_text=True, vertical="center")
        bg = OFFWHITE if i % 2 == 0 else CREME
        for c in range(1, 3):
            ins.cell(r, c).fill = fill(bg)
            ins.cell(r, c).border = thin_border()
            ins.cell(r, c).alignment = Alignment(vertical="center", wrap_text=True)
        ins.row_dimensions[r].height = 36
    ins.column_dimensions["A"].width = 28
    ins.column_dimensions["B"].width = 110

    # freeze panes already set
    # Named range for total
    from openpyxl.workbook.defined_name import DefinedName

    wb.defined_names.add(DefinedName(name="TotalGeral", attr_text=f"'Compras e Custos'!$H${total_row}"))

    out = "/home/user/calculadora-boleado/Compras_e_Custos_Boleado.xlsx"
    wb.save(out)
    print(f"saved {out} total_row={total_row} last_data={last_data}")
    return out


if __name__ == "__main__":
    build()
