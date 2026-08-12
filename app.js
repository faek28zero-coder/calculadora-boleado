const LUZ_GAS = 0.2;

const compras = [
  { cat: "Utilidades", nome: "Papel-alumínio", preco: 4.39, qtd: 1, unidade: "rolo" },
  { cat: "Utilidades", nome: "Plástico filme", preco: 5.59, qtd: 1, unidade: "rolo" },
  { cat: "Utilidades", nome: "Papel-toalha", preco: 4.39, qtd: 2, unidade: "rolos" },
  { cat: "Utilidades", nome: "Saco de confeitar", preco: 7.5, qtd: 5, unidade: "un" },
  { cat: "Utilidades", nome: "Touca Descartável", preco: 5, qtd: 5, unidade: "un" },
  { cat: "Utilidades", nome: "Durex", preco: 4.5, qtd: 1, unidade: "un" },
  { cat: "Utilidades", nome: "Grampeador com grampos", preco: 12, qtd: 1, unidade: "conjunto" },
  { cat: "Utilidades", nome: "Peneira", preco: 19.29, qtd: 1, unidade: "un" },
  { cat: "Ingredientes", nome: "Chocolate em pó", preco: 39.8, qtd: 200, unidade: "g" },
  { cat: "Ingredientes", nome: "Margarina Qualy", preco: 6.98, qtd: 250, unidade: "g" },
  { cat: "Ingredientes", nome: "Paçoca", preco: 23.9, qtd: 700, unidade: "g" },
  { cat: "Ingredientes", nome: "Leite em pó", preco: 8.99, qtd: 200, unidade: "g" },
  { cat: "Ingredientes", nome: "Biscoito Oreo", preco: 8.58, qtd: 180, unidade: "g" },
  { cat: "Ingredientes", nome: "Leite condensado", preco: 29.12, qtd: 1580, unidade: "g" },
  { cat: "Ingredientes", nome: "Creme de leite", preco: 14.96, qtd: 800, unidade: "g" },
  { cat: "Ingredientes", nome: "Pacote de bala", preco: 7.98, qtd: 500, unidade: "g" },
  { cat: "Ingredientes", nome: "Granulado", preco: 19.98, qtd: 400, unidade: "g" },
  { cat: "Ingredientes", nome: "Pacotão de Bis", preco: 12.98, qtd: 201.6, unidade: "g" },
  { cat: "Ingredientes", nome: "Morango", preco: 10.9, qtd: 220, unidade: "g" },
  { cat: "Ingredientes", nome: "Barra de chocolate Fracionado", preco: 29.9, qtd: 1000, unidade: "g" },
  { cat: "Embalagens", nome: "Adesivos", preco: 19.34, qtd: 70, unidade: "un" },
  { cat: "Embalagens", nome: "Pote 250 ml", preco: 15, qtd: 24, unidade: "un" },
  { cat: "Embalagens", nome: "Pote 140 ml", preco: 15.9, qtd: 25, unidade: "un" },
  { cat: "Embalagens", nome: "Forminha de brigadeiro", preco: 3.5, qtd: 100, unidade: "un" },
  { cat: "Embalagens", nome: "Colherzinhas", preco: 5.99, qtd: 50, unidade: "un" },
  { cat: "Embalagens", nome: "Saco chup-chup", preco: 2.99, qtd: 100, unidade: "un" },
  { cat: "Embalagens", nome: "Embalagem quadrada de brigadeiro", preco: 4.99, qtd: 10, unidade: "un" },
  { cat: "Embalagens", nome: "Pote de brinde", preco: 6.5, qtd: 10, unidade: "un" },
  { cat: "Embalagens", nome: "Saco Kraft", preco: 29.9, qtd: 50, unidade: "un" },
  { cat: "Embalagens", nome: "Embalagem trufas", preco: 30, qtd: 300, unidade: "un" },
  { cat: "Embalagens", nome: "Saco de confeitar GRANDE", preco: 20, qtd: 10, unidade: "un" },
  { cat: "Utilidades", nome: "Forma pra trufas", preco: 35.98, qtd: 2, unidade: "formas" },
];

function custoUn(item) {
  return item.qtd > 0 ? item.preco / item.qtd : 0;
}

function custoIng(nome, qtd) {
  const item = compras.find((c) => c.nome === nome);
  if (!item) return 0;
  return custoUn(item) * qtd;
}

const receitas250 = {
  "Brigadeiro clássico": {
    base: [
      ["Leite condensado", 70],
      ["Chocolate em pó", 12],
      ["Margarina Qualy", 8],
      ["Creme de leite", 30],
    ],
    extra: [["Granulado", 5]],
  },
  "Ninho": {
    base: [
      ["Leite condensado", 70],
      ["Leite em pó", 18],
      ["Margarina Qualy", 8],
      ["Creme de leite", 30],
    ],
    extra: [["Leite em pó", 9]],
  },
  "Oreo": {
    base: [
      ["Leite condensado", 65],
      ["Creme de leite", 35],
      ["Margarina Qualy", 6],
      ["Leite em pó", 10],
      ["Biscoito Oreo", 18],
    ],
    extra: [["Biscoito Oreo", 9]],
  },
  "Paçoca": {
    base: [
      ["Leite condensado", 70],
      ["Paçoca", 25],
      ["Creme de leite", 30],
      ["Margarina Qualy", 6],
    ],
    extra: [["Paçoca", 5]],
  },
  "Morango": {
    base: [
      ["Leite condensado", 60],
      ["Creme de leite", 35],
      ["Leite em pó", 12],
      ["Margarina Qualy", 6],
    ],
    extra: [["Morango", 16]],
  },
  "Bis": {
    base: [
      ["Leite condensado", 65],
      ["Chocolate em pó", 10],
      ["Creme de leite", 30],
      ["Margarina Qualy", 6],
    ],
    extra: [["Pacotão de Bis", 6]],
  },
  "Ninho com Nutella (choc. fracionado)": {
    base: [
      ["Leite condensado", 60],
      ["Leite em pó", 15],
      ["Creme de leite", 30],
      ["Barra de chocolate Fracionado", 20],
      ["Margarina Qualy", 5],
    ],
    extra: [["Leite em pó", 9]],
  },
};

function brl(n) {
  return n.toLocaleString("pt-BR", { style: "currency", currency: "BRL" });
}

function subtotalCompras() {
  return compras.reduce((s, c) => s + c.preco, 0);
}

function renderStats() {
  const sub = subtotalCompras();
  document.getElementById("statCompras").textContent = brl(sub);
  document.getElementById("statTotal").textContent = brl(sub * (1 + LUZ_GAS));
}

function renderCompras() {
  const tb = document.getElementById("comprasBody");
  tb.innerHTML = compras
    .map(
      (c, i) => `<tr>
      <td>${c.cat}</td>
      <td>${c.nome}</td>
      <td><input type="number" step="0.01" min="0" value="${c.preco}" data-i="${i}" data-f="preco" /></td>
      <td><input type="number" step="0.1" min="0" value="${c.qtd}" data-i="${i}" data-f="qtd" /></td>
      <td>${c.unidade}</td>
      <td>${custoUn(c).toLocaleString("pt-BR", { style: "currency", currency: "BRL", maximumFractionDigits: 4 })}</td>
    </tr>`
    )
    .join("");
  tb.querySelectorAll("input").forEach((inp) => {
    inp.addEventListener("change", () => {
      const i = +inp.dataset.i;
      compras[i][inp.dataset.f] = Number(inp.value);
      renderCompras();
      renderStats();
      calc();
    });
  });
}

function renderReceitas() {
  const el = document.getElementById("receitasList");
  el.innerHTML = Object.entries(receitas250)
    .map(([nome, r]) => {
      const linhas = [...r.base, ...r.extra].map(([n, g]) => `${n} ${g}g`).join(" · ");
      return `<div class="recipe"><h3>${nome}</h3><p>${linhas}</p></div>`;
    })
    .join("");
}

function fillSabores() {
  const sel = document.getElementById("sabor");
  sel.innerHTML = Object.keys(receitas250)
    .map((n) => `<option>${n}</option>`)
    .join("");
}

function calc() {
  const sabor = document.getElementById("sabor").value;
  const tam = Number(document.getElementById("tamanho").value);
  const qtd = Math.max(1, Number(document.getElementById("qtd").value) || 1);
  const margem = Number(document.getElementById("margem").value);
  document.getElementById("margemLbl").textContent = margem + "%";
  document.getElementById("flavorName").textContent = `${sabor} · ${tam} ml`;

  const rec = receitas250[sabor];
  const fator = tam === 140 ? 0.56 : 1;
  const bom = [];
  let cIng = 0;
  for (const [nome, g] of [...rec.base, ...rec.extra]) {
    const uso = +(g * fator).toFixed(1);
    const custo = custoIng(nome, uso);
    cIng += custo;
    bom.push({ nome, uso, un: "g", custo });
  }

  const poteNome = tam === 140 ? "Pote 140 ml" : "Pote 250 ml";
  let cEmb = custoIng(poteNome, 1);
  bom.push({ nome: poteNome, uso: 1, un: "un", custo: custoIng(poteNome, 1) });

  if (document.getElementById("incAdesivo").checked) {
    const c = custoIng("Adesivos", 1);
    cEmb += c;
    bom.push({ nome: "Adesivo", uso: 1, un: "un", custo: c });
  }
  if (document.getElementById("incColher").checked) {
    const c = custoIng("Colherzinhas", 1);
    cEmb += c;
    bom.push({ nome: "Colherzinha", uso: 1, un: "un", custo: c });
  }
  if (document.getElementById("incSaco").checked) {
    const c = custoIng("Saco Kraft", 1);
    cEmb += c;
    bom.push({ nome: "Saco Kraft", uso: 1, un: "un", custo: c });
  }

  const base = cIng + cEmb;
  const luz = base * LUZ_GAS;
  const unit = base + luz;
  const preco = unit * (1 + margem / 100);
  const lucro = preco - unit;

  document.getElementById("cIng").textContent = brl(cIng);
  document.getElementById("cEmb").textContent = brl(cEmb);
  document.getElementById("cLuz").textContent = brl(luz);
  document.getElementById("cUnit").textContent = brl(unit);
  document.getElementById("pSug").textContent = brl(preco);
  document.getElementById("lucro").textContent = brl(lucro);
  document.getElementById("lote").textContent = `${qtd} potes · custo ${brl(unit * qtd)} · venda ${brl(preco * qtd)}`;

  document.getElementById("bom").innerHTML = bom
    .map(
      (b) =>
        `<li><span>${b.nome} (${b.uso} ${b.un})</span><span>${brl(b.custo)}</span></li>`
    )
    .join("");
}

document.querySelectorAll(".tab").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach((b) => b.classList.remove("on"));
    document.querySelectorAll(".panel").forEach((p) => p.classList.remove("on"));
    btn.classList.add("on");
    document.getElementById(btn.dataset.tab).classList.add("on");
  });
});

["sabor", "tamanho", "qtd", "margem", "incAdesivo", "incColher", "incSaco"].forEach((id) => {
  document.getElementById(id).addEventListener("input", calc);
});

fillSabores();
renderCompras();
renderReceitas();
renderStats();
calc();
