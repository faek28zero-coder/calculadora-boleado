const COMPRAS = [
  {
    n: 3,
    nome: "Terceira compra",
    itens: [
      { produto: "Chocolate em pó", detalhe: "200 g", categoria: "Ingrediente", qtd: 1, unidade: "pct", valor: 39.9 },
      { produto: "Margarina", detalhe: "250 g", categoria: "Ingrediente", qtd: 1, unidade: "pote", valor: 6.98 },
    ],
  },
  {
    n: 4,
    nome: "Quarta compra",
    itens: [
      { produto: "Caixa para cento de doces", detalhe: "3 unidades", categoria: "Embalagem", qtd: 3, unidade: "un", valor: 10.47 },
      { produto: "Granulado", detalhe: "—", categoria: "Ingrediente", qtd: 1, unidade: "pct", valor: 19.99 },
      { produto: "Chocolate em pó 50%", detalhe: "—", categoria: "Ingrediente", qtd: 1, unidade: "pct", valor: 38.99 },
      { produto: "Leite condensado", detalhe: "6 unidades", categoria: "Ingrediente", qtd: 6, unidade: "lata", valor: 41.44 },
      { produto: "Creme de leite", detalhe: "8 unidades", categoria: "Ingrediente", qtd: 8, unidade: "cx", valor: 21.52 },
      { produto: "Forminha de brigadeiro", detalhe: "200 unidades", categoria: "Embalagem", qtd: 200, unidade: "un", valor: 6.0 },
    ],
  },
  {
    n: 5,
    nome: "Quinta compra",
    itens: [
      { produto: "Saco de confeitar", detalhe: "25 unidades", categoria: "Utensílio", qtd: 25, unidade: "un", valor: 13.99 },
      { produto: "Coco em flocos", detalhe: "500 g", categoria: "Ingrediente", qtd: 1, unidade: "pct", valor: 25.99 },
      { produto: "Biscoito Oreo", detalhe: "270 g", categoria: "Ingrediente", qtd: 1, unidade: "pct", valor: 11.99 },
      { produto: "Pote redondo 150 ml", detalhe: "150 ml", categoria: "Embalagem", qtd: 1, unidade: "un", valor: 11.59 },
      { produto: "Saco Kraft", detalhe: "20 unidades", categoria: "Embalagem", qtd: 20, unidade: "un", valor: 14.99 },
      { produto: "Carimbo de brigadeiros", detalhe: "1 unidade", categoria: "Utensílio", qtd: 1, unidade: "un", valor: 15.99 },
      { produto: "Leite condensado", detalhe: "10 unidades", categoria: "Ingrediente", qtd: 10, unidade: "lata", valor: 69.9 },
      { produto: "Creme de leite", detalhe: "7 unidades", categoria: "Ingrediente", qtd: 7, unidade: "cx", valor: 18.83 },
      { produto: "Embalagem quadrada para brigadeiro", detalhe: "15 unidades", categoria: "Embalagem", qtd: 15, unidade: "un", valor: 18.0 },
    ],
  },
  {
    n: 6,
    nome: "Sexta compra",
    itens: [
      { produto: "Barra de chocolate", detalhe: "—", categoria: "Ingrediente", qtd: 1, unidade: "un", valor: 30.0 },
    ],
  },
];

const LINHAS = COMPRAS.flatMap((c) =>
  c.itens.map((item) => ({ ...item, n: c.n, compra: c.nome }))
);

const TOTAL = LINHAS.reduce((s, i) => s + i.valor, 0);

const brl = (n) =>
  n.toLocaleString("pt-BR", { style: "currency", currency: "BRL" });

const tagClass = (cat) =>
  cat === "Ingrediente" ? "ing" : cat === "Embalagem" ? "emb" : "ute";

function renderCards() {
  const el = document.getElementById("cards");
  el.innerHTML = COMPRAS.map((c) => {
    const t = c.itens.reduce((s, i) => s + i.valor, 0);
    return `<article class="card">
      <div class="n">${c.n}ª compra</div>
      <strong>${brl(t)}</strong>
      <span>${c.itens.length} ${c.itens.length === 1 ? "item" : "itens"}</span>
    </article>`;
  }).join("");
}

function linhasFiltradas() {
  const compra = document.getElementById("filtro-compra").value;
  const cat = document.getElementById("filtro-cat").value;
  return LINHAS.filter((l) => {
    if (compra !== "todas" && String(l.n) !== compra) return false;
    if (cat !== "todas" && l.categoria !== cat) return false;
    return true;
  });
}

function renderTabela() {
  const rows = linhasFiltradas();
  const tbody = document.getElementById("tbody");
  tbody.innerHTML = rows
    .map((l) => {
      const unit = l.valor / l.qtd;
      const pct = (l.valor / TOTAL) * 100;
      return `<tr>
        <td>${l.n}ª</td>
        <td><div class="prod">${l.produto}</div></td>
        <td class="detalhe">${l.detalhe}</td>
        <td><span class="tag ${tagClass(l.categoria)}">${l.categoria}</span></td>
        <td class="num">${l.qtd} ${l.unidade}</td>
        <td class="num">${brl(l.valor)}</td>
        <td class="num">${brl(unit)}</td>
        <td class="num">${pct.toFixed(1).replace(".", ",")}%</td>
      </tr>`;
    })
    .join("");

  const sub = rows.reduce((s, i) => s + i.valor, 0);
  document.getElementById("total-filtrado").textContent = brl(sub);
  document.getElementById("pct-filtrado").textContent =
    TOTAL === 0 ? "—" : `${((sub / TOTAL) * 100).toFixed(1).replace(".", ",")}%`;
}

function renderBars() {
  const maxCompra = Math.max(
    ...COMPRAS.map((c) => c.itens.reduce((s, i) => s + i.valor, 0))
  );
  document.getElementById("bars-compra").innerHTML = COMPRAS.map((c) => {
    const t = c.itens.reduce((s, i) => s + i.valor, 0);
    const w = (t / maxCompra) * 100;
    return `<li>
      <div class="bar-meta"><span>${c.nome}</span><strong>${brl(t)}</strong></div>
      <div class="track"><div class="fill" style="width:${w}%"></div></div>
    </li>`;
  }).join("");

  const cats = ["Ingrediente", "Embalagem", "Utensílio"].map((nome) => ({
    nome,
    t: LINHAS.filter((l) => l.categoria === nome).reduce((s, i) => s + i.valor, 0),
  }));
  document.getElementById("bars-cat").innerHTML = cats
    .map((c) => {
      const w = (c.t / TOTAL) * 100;
      return `<li>
        <div class="bar-meta"><span>${c.nome}</span><strong>${brl(c.t)} · ${w
          .toFixed(1)
          .replace(".", ",")}%</strong></div>
        <div class="track"><div class="fill" style="width:${w}%"></div></div>
      </li>`;
    })
    .join("");
}

document.getElementById("total-geral").textContent = brl(TOTAL);
document.getElementById("filtro-compra").addEventListener("change", renderTabela);
document.getElementById("filtro-cat").addEventListener("change", renderTabela);
document.getElementById("btn-print").addEventListener("click", () => window.print());

renderCards();
renderTabela();
renderBars();
