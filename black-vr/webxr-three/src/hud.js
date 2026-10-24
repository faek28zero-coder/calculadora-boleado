/** HUD em DOM: pontos, tempo, munição, hitmarker e avisos. */
export class Hud {
  constructor() {
    this.el = {
      hud: document.getElementById('hud'),
      score: document.getElementById('hud-score'),
      combo: document.getElementById('hud-combo'),
      time: document.getElementById('hud-time'),
      acc: document.getElementById('hud-acc'),
      ammo: document.getElementById('hud-ammo'),
      reload: document.getElementById('hud-reload'),
      hits: document.getElementById('hud-hits'),
      hitmarker: document.getElementById('hitmarker'),
      toast: document.getElementById('toast'),
      crosshair: document.getElementById('crosshair'),
      lastShot: 0,
      toastTimer: null,
    };
  }

  show() {
    this.el.hud.classList.remove('hidden');
    this.el.hud.setAttribute('aria-hidden', 'false');
  }

  hide() {
    this.el.hud.classList.add('hidden');
    this.el.hud.setAttribute('aria-hidden', 'true');
  }

  reset() {
    this.setScore(0);
    this.setCombo(0);
    this.setTime(75);
    this.setAccuracy(0, 0);
    this.setAmmo(12, 12, false);
    this.setHits(0);
  }

  setScore(n) {
    this.el.score.textContent = n.toLocaleString('pt-BR');
  }

  setCombo(mult) {
    this.el.combo.textContent = mult > 1 ? `COMBO x${mult}` : '';
  }

  setTime(seconds) {
    const s = Math.max(0, Math.ceil(seconds));
    this.el.time.textContent = s;
    this.el.time.classList.toggle('warn', s <= 10);
  }

  setAccuracy(hits, shots) {
    this.el.acc.textContent = shots ? `${Math.round((hits / shots) * 100)}%` : '—';
  }

  setHits(n) {
    this.el.hits.textContent = n;
  }

  setAmmo(current, max, reloading) {
    this.el.ammo.innerHTML = `${current}<span class="dim">/${max}</span>`;
    this.el.reload.classList.toggle('hidden', !reloading);
  }

  hitmarker(crit = false) {
    const el = this.el.hitmarker;
    el.classList.toggle('head', crit);
    el.classList.add('on');
    clearTimeout(this._hitTimer);
    this._hitTimer = setTimeout(() => el.classList.remove('on'), 110);
  }

  toast(text, ms = 900) {
    const el = this.el.toast;
    el.textContent = text;
    el.classList.add('on');
    clearTimeout(this.el.toastTimer);
    this.el.toastTimer = setTimeout(() => el.classList.remove('on'), ms);
  }

  setCrosshairVisible(v) {
    this.el.crosshair.style.display = v ? '' : 'none';
  }
}
