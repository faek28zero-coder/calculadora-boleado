/**
 * Texturas procedurais em canvas — zero dependência de assets externos.
 * Tudo é gerado em runtime, então o repo fica leve e o visual é original.
 */
import * as THREE from 'three';

const cache = new Map();

function canvas(size = 512) {
  const c = document.createElement('canvas');
  c.width = size;
  c.height = size;
  return { c, ctx: c.getContext('2d') };
}

function noise(ctx, size, amount = 14, alpha = 0.5) {
  const img = ctx.getImageData(0, 0, size, size);
  const d = img.data;
  for (let i = 0; i < d.length; i += 4) {
    const n = (Math.random() - 0.5) * amount;
    d[i] = Math.max(0, Math.min(255, d[i] + n));
    d[i + 1] = Math.max(0, Math.min(255, d[i + 1] + n));
    d[i + 2] = Math.max(0, Math.min(255, d[i + 2] + n));
  }
  ctx.putImageData(img, 0, 0);
  if (alpha > 0) {
    ctx.globalAlpha = alpha;
    ctx.globalCompositeOperation = 'overlay';
    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, size, size);
    ctx.globalCompositeOperation = 'source-over';
    ctx.globalAlpha = 1;
  }
}

function finish(c, repeat = 1) {
  const tex = new THREE.CanvasTexture(c);
  tex.wrapS = tex.wrapT = THREE.RepeatWrapping;
  tex.repeat.set(repeat, repeat);
  tex.anisotropy = 4;
  tex.colorSpace = THREE.SRGBColorSpace;
  return tex;
}

function cached(key, build, repeat = 1) {
  const k = `${key}:${repeat}`;
  if (cache.has(k)) return cache.get(k);
  const tex = finish(build(), repeat);
  cache.set(k, tex);
  return tex;
}

/** Concreto sujo com manchas e rachaduras. */
export function concrete(repeat = 2) {
  return cached('concrete', () => {
    const size = 512;
    const { c, ctx } = canvas(size);
    ctx.fillStyle = '#23262b';
    ctx.fillRect(0, 0, size, size);
    noise(ctx, size, 18, 0.35);

    // manchas
    for (let i = 0; i < 26; i++) {
      const x = Math.random() * size;
      const y = Math.random() * size;
      const r = 18 + Math.random() * 90;
      const g = ctx.createRadialGradient(x, y, 0, x, y, r);
      const dark = Math.random() > 0.45;
      g.addColorStop(0, dark ? 'rgba(8,10,12,0.42)' : 'rgba(120,124,120,0.14)');
      g.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = g;
      ctx.beginPath();
      ctx.arc(x, y, r, 0, Math.PI * 2);
      ctx.fill();
    }

    // rachaduras
    ctx.strokeStyle = 'rgba(6,7,9,0.55)';
    ctx.lineWidth = 1;
    for (let i = 0; i < 12; i++) {
      ctx.beginPath();
      let x = Math.random() * size;
      let y = Math.random() * size;
      ctx.moveTo(x, y);
      for (let s = 0; s < 9; s++) {
        x += (Math.random() - 0.5) * 46;
        y += (Math.random() - 0.5) * 46;
        ctx.lineTo(x, y);
      }
      ctx.stroke();
    }

    // emendas de laje
    ctx.strokeStyle = 'rgba(0,0,0,0.5)';
    ctx.lineWidth = 3;
    ctx.strokeRect(0, 0, size, size);
    ctx.beginPath();
    ctx.moveTo(size / 2, 0);
    ctx.lineTo(size / 2, size);
    ctx.stroke();
    return c;
  }, repeat);
}

/** Painel metálico industrial com rebites. */
export function metal(repeat = 2) {
  return cached('metal', () => {
    const size = 512;
    const { c, ctx } = canvas(size);
    const g = ctx.createLinearGradient(0, 0, size, size);
    g.addColorStop(0, '#2b3138');
    g.addColorStop(0.5, '#1b2026');
    g.addColorStop(1, '#242a31');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, size, size);
    noise(ctx, size, 10, 0.2);

    // faixas de chapa
    ctx.strokeStyle = 'rgba(0,0,0,0.55)';
    ctx.lineWidth = 4;
    for (let i = 1; i < 4; i++) {
      ctx.beginPath();
      ctx.moveTo(0, (size / 4) * i);
      ctx.lineTo(size, (size / 4) * i);
      ctx.stroke();
    }

    // rebites
    for (let x = 26; x < size; x += 64) {
      for (let y = 26; y < size; y += 128) {
        ctx.fillStyle = 'rgba(160,168,176,0.22)';
        ctx.beginPath();
        ctx.arc(x, y, 3, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = 'rgba(0,0,0,0.4)';
        ctx.beginPath();
        ctx.arc(x + 1, y + 1.5, 3, 0, Math.PI * 2);
        ctx.fill();
      }
    }
    return c;
  }, repeat);
}

/** Listras de perigo (amarelo/preto), para bordas de plataforma. */
export function hazard(repeat = 4) {
  return cached('hazard', () => {
    const size = 256;
    const { c, ctx } = canvas(size);
    ctx.fillStyle = '#0c0d0f';
    ctx.fillRect(0, 0, size, size);
    ctx.save();
    ctx.translate(size / 2, size / 2);
    ctx.rotate(-Math.PI / 4);
    ctx.fillStyle = '#e0a021';
    for (let i = -size; i < size; i += 64) ctx.fillRect(i, -size, 32, size * 2);
    ctx.restore();
    noise(ctx, size, 16, 0.25);
    // desgaste
    ctx.fillStyle = 'rgba(20,20,20,0.35)';
    for (let i = 0; i < 40; i++) {
      ctx.fillRect(Math.random() * size, Math.random() * size, Math.random() * 24, Math.random() * 8);
    }
    return c;
  }, repeat);
}

/** Piso xadrez gasto (estande de tiro / galpão). */
export function floorTiles(repeat = 8) {
  return cached('floor', () => {
    const size = 512;
    const { c, ctx } = canvas(size);
    const n = 4;
    const t = size / n;
    for (let x = 0; x < n; x++) {
      for (let y = 0; y < n; y++) {
        ctx.fillStyle = (x + y) % 2 ? '#1a1e23' : '#14181c';
        ctx.fillRect(x * t, y * t, t, t);
      }
    }
    // grade
    ctx.strokeStyle = 'rgba(70,78,86,0.35)';
    ctx.lineWidth = 2;
    for (let i = 0; i <= n; i++) {
      ctx.beginPath();
      ctx.moveTo(i * t, 0);
      ctx.lineTo(i * t, size);
      ctx.moveTo(0, i * t);
      ctx.lineTo(size, i * t);
      ctx.stroke();
    }
    noise(ctx, size, 22, 0.3);
    for (let i = 0; i < 18; i++) {
      const x = Math.random() * size;
      const y = Math.random() * size;
      const r = 20 + Math.random() * 70;
      const g = ctx.createRadialGradient(x, y, 0, x, y, r);
      g.addColorStop(0, 'rgba(0,0,0,0.35)');
      g.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = g;
      ctx.fillRect(x - r, y - r, r * 2, r * 2);
    }
    return c;
  }, repeat);
}

/** Alvo de tiro (silhueta genérica, sem marca de terceiros). */
export function targetFace() {
  return cached('target', () => {
    const size = 256;
    const { c, ctx } = canvas(size);
    ctx.fillStyle = '#e8e2d4';
    ctx.fillRect(0, 0, size, size);
    noise(ctx, size, 8, 0.12);

    // anéis
    const rings = [
      { r: 108, color: '#d8d1c0' },
      { r: 86, color: '#c6bfae' },
      { r: 62, color: '#b0a898' },
      { r: 40, color: '#9a9282' },
    ];
    rings.forEach(({ r, color }) => {
      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.arc(size / 2, size / 2, r, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = 'rgba(30,30,30,0.45)';
      ctx.lineWidth = 2;
      ctx.stroke();
    });

    // centro
    ctx.fillStyle = '#1b1d20';
    ctx.beginPath();
    ctx.arc(size / 2, size / 2, 16, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = '#ff6a13';
    ctx.beginPath();
    ctx.arc(size / 2, size / 2, 7, 0, Math.PI * 2);
    ctx.fill();

    // cantos
    ctx.strokeStyle = 'rgba(20,20,20,0.5)';
    ctx.lineWidth = 3;
    ctx.strokeRect(10, 10, size - 20, size - 20);
    return c;
  });
}

/** Etiqueta com texto (para fachadas, placas, telas). */
export function label(lines, opts = {}) {
  const {
    width = 512,
    height = 256,
    bg = '#0b0e12',
    fg = '#e8e4dc',
    accent = '#ff6a13',
    size = 64,
    align = 'center',
    border = true,
  } = opts;
  const c = document.createElement('canvas');
  c.width = width;
  c.height = height;
  const ctx = c.getContext('2d');
  ctx.fillStyle = bg;
  ctx.fillRect(0, 0, width, height);
  noise(ctx, Math.min(width, height), 8, 0.06);

  if (border) {
    ctx.strokeStyle = accent;
    ctx.lineWidth = 6;
    ctx.strokeRect(12, 12, width - 24, height - 24);
  }

  const list = Array.isArray(lines) ? lines : [lines];
  ctx.textAlign = align;
  ctx.textBaseline = 'middle';
  const total = list.length;
  list.forEach((line, i) => {
    const isAccent = line.startsWith('*');
    const text = isAccent ? line.slice(1) : line;
    ctx.fillStyle = isAccent ? accent : fg;
    ctx.font = `700 ${isAccent ? size : size * 0.62}px ui-monospace, Menlo, monospace`;
    const x = align === 'center' ? width / 2 : align === 'left' ? 40 : width - 40;
    const y = height / 2 + (i - (total - 1) / 2) * size * 0.95;
    ctx.fillText(text, x, y);
  });

  const tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 4;
  return tex;
}

/** Marca de tiro (decal) reutilizável. */
export function bulletHole() {
  return cached('hole', () => {
    const size = 64;
    const { c, ctx } = canvas(size);
    ctx.clearRect(0, 0, size, size);
    const g = ctx.createRadialGradient(size / 2, size / 2, 2, size / 2, size / 2, size / 2);
    g.addColorStop(0, 'rgba(0,0,0,1)');
    g.addColorStop(0.34, 'rgba(6,6,6,0.92)');
    g.addColorStop(0.55, 'rgba(60,56,50,0.5)');
    g.addColorStop(1, 'rgba(60,56,50,0)');
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.arc(size / 2, size / 2, size / 2, 0, Math.PI * 2);
    ctx.fill();
    return c;
  });
}

/** Poeira/partícula suave. */
export function spark() {
  return cached('spark', () => {
    const size = 64;
    const { c, ctx } = canvas(size);
    const g = ctx.createRadialGradient(size / 2, size / 2, 0, size / 2, size / 2, size / 2);
    g.addColorStop(0, 'rgba(255,236,190,1)');
    g.addColorStop(0.35, 'rgba(255,150,40,0.85)');
    g.addColorStop(1, 'rgba(255,90,0,0)');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, size, size);
    return c;
  });
}

export function disposeAll() {
  cache.forEach((tex) => tex.dispose());
  cache.clear();
}
