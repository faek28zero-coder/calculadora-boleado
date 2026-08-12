const ITENS = [
  ["Utilidades", "Papel-alumínio", 4.39, 1, "rolo", "", 1],
  ["Utilidades", "Plástico filme", 5.59, 1, "rolo", "", 1],
  ["Utilidades", "Papel-toalha", 4.39, 2, "rolos", "", 1],
  ["Utilidades", "Saco de confeitar", 7.5, 5, "unidades", "", 1],
  ["Utilidades", "Touca Descartável", 5, 5, "unidades", "", 1],
  ["Utilidades", "Durex", 4.5, 1, "unidade", "", 1],
  ["Utilidades", "Grampeador com grampos", 12, 1, "conjunto", "", 1],
  ["Utilidades", "Peneira", 19.29, 1, "unidade", "", 1],
  ["Ingredientes", "Chocolate em pó", 39.8, 200, "gramas", "", 1],
  ["Ingredientes", "Margarina Qualy", 6.98, 250, "gramas", "", 1],
  ["Ingredientes", "Paçoca", 23.9, 700, "gramas", "", 1],
  ["Ingredientes", "Leite em pó", 8.99, 200, "gramas", "", 1],
  ["Ingredientes", "Biscoito Oreo", 8.58, 180, "gramas", "", 1],
  ["Ingredientes", "Leite condensado", 29.12, 1580, "gramas", "", 1],
  ["Ingredientes", "Creme de leite", 14.96, 800, "gramas", "", 1],
  ["Ingredientes", "Pacote de bala", 7.98, 500, "gramas", "", 1],
  ["Ingredientes", "Granulado", 19.98, 400, "gramas", "", 1],
  ["Ingredientes", "Pacotão de Bis", 12.98, 201.6, "gramas", "", 1],
  ["Ingredientes", "Morango", 10.9, 220, "gramas", "", 1],
  ["Ingredientes", "Barra de chocolate Fracionado", 29.9, 1000, "gramas", "", 1],
  ["Embalagens", "Adesivos", 19.34, 70, "unidades", "", 1],
  ["Embalagens", "Pote 250 ml", 15, 24, "unidades", "", 1],
  ["Embalagens", "Pote 140 ml", 15.9, 25, "unidades", "", 1],
  ["Embalagens", "Forminha de brigadeiro", 3.5, 100, "unidades", "", 1],
  ["Embalagens", "Colherzinhas", 5.99, 50, "unidades", "", 1],
  ["Embalagens", "Saco chup-chup", 2.99, 100, "unidades", "", 1],
  ["Embalagens", "Embalagem quadrada de brigadeiro", 4.99, 10, "unidades", "", 1],
  ["Embalagens", "Pote de brinde", 6.5, 10, "unidades", "", 1],
  ["Embalagens", "Saco Kraft", 29.9, 50, "unidades", "", 1],
  ["Embalagens", "Embalagem trufas", 30, 300, "unidades", "", 1],
  ["Embalagens", "Saco de confeitar GRANDE", 20, 10, "unidades", "Segunda compra", 2],
  ["Utilidades", "Forma pra fazer as trufas (não descartável)", 35.98, 2, "formas", "Segunda compra - INVESTIMENTO, não descartável", 2],
  ["Ingredientes", "Chocolate em pó", 39.9, 200, "gramas", "Terceira compra", 3],
  ["Ingredientes", "Margarina Qualy", 6.98, 250, "gramas", "Terceira compra", 3],
  ["Embalagens", "Caixa para cento de doces", 10.47, 3, "unidades", "Quarta compra", 4],
  ["Ingredientes", "Granulado", 19.99, 400, "gramas", "Quarta compra", 4],
  ["Ingredientes", "Chocolate em pó 50%", 38.99, 200, "gramas", "Quarta compra", 4],
  ["Ingredientes", "Leite condensado", 41.44, 2370, "gramas", "Quarta compra (6 latas de 395 g)", 4],
  ["Ingredientes", "Creme de leite", 21.52, 1600, "gramas", "Quarta compra (8 caixas de 200 g)", 4],
  ["Embalagens", "Forminha de brigadeiro", 6, 200, "unidades", "Quarta compra", 4],
  ["Utilidades", "Saco de confeitar", 13.99, 25, "unidades", "Quinta compra", 5],
  ["Ingredientes", "Coco em flocos", 25.99, 500, "gramas", "Quinta compra", 5],
  ["Ingredientes", "Biscoito Oreo", 11.99, 270, "gramas", "Quinta compra", 5],
  ["Embalagens", "Pote redondo 150 ml", 11.59, 25, "unidades", "Quinta compra", 5],
  ["Embalagens", "Saco Kraft", 14.99, 20, "unidades", "Quinta compra", 5],
  ["Utilidades", "Carimbo de brigadeiros", 15.99, 1, "unidade", "Quinta compra", 5],
  ["Ingredientes", "Leite condensado", 69.9, 3950, "gramas", "Quinta compra (10 latas de 395 g)", 5],
  ["Ingredientes", "Creme de leite", 18.83, 1400, "gramas", "Quinta compra (7 caixas de 200 g)", 5],
  ["Embalagens", "Embalagem quadrada de brigadeiro", 18, 15, "unidades", "Quinta compra", 5],
  ["Ingredientes", "Barra de chocolate", 30, 1, "unidade", "Sexta compra (peso não informado)", 6],
];

const ADICIONAIS = [
  ["Granulado", 5],
  ["Morango", 16],
  ["Biscoito Oreo", 9],
  ["Pacotão de Bis", 6],
  ["Paçoca", 5],
  ["Leite em pó", 9],
];

const brl = (n, d = 2) =>
  n.toLocaleString("pt-BR", { style: "currency", currency: "BRL", minimumFractionDigits: d, maximumFractionDigits: d });

const qtdFmt = (n) =>
  Number.isInteger(n) ? String(n) : String(n).replace(".", ",");

function render() {
  const filtro = document.getElementById("filtro-compra").value;
  const rows = ITENS.filter((i) => filtro === "todas" || String(i[6]) === filtro);
  document.getElementById("tbody").innerHTML = rows
    .map(([cat, prod, preco, qtd, unid, obs, g]) => {
      const unit = qtd ? preco / qtd : 0;
      return `<tr class="g${g}">
        <td>${cat}</td>
        <td>${prod}</td>
        <td class="num">${brl(preco)}</td>
        <td class="num">${qtdFmt(qtd)}</td>
        <td>${unid}</td>
        <td class="num">${brl(unit, 4)}</td>
        <td>${obs}</td>
      </tr>`;
    })
    .join("");
}

const subtotal = ITENS.reduce((s, i) => s + i[2], 0);
const novas = ITENS.filter((i) => i[6] >= 3).reduce((s, i) => s + i[2], 0);
const luz = subtotal * 0.2;
document.getElementById("subtotal").textContent = brl(subtotal);
document.getElementById("luz").textContent = brl(luz);
document.getElementById("total").textContent = brl(subtotal + luz);
document.getElementById("novas").textContent = brl(novas);
document.getElementById("adicionais").innerHTML = ADICIONAIS.map(
  ([n, p]) => `<tr><td>${n}</td><td>${p}</td></tr>`
).join("");
document.getElementById("filtro-compra").addEventListener("change", render);
render();
