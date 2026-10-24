/**
 * Áudio 100% sintetizado com WebAudio — nenhum arquivo de som do jogo original.
 * Tiro, recarga, impacto, alvo e "headshot" são gerados em runtime.
 */

export class SynthAudio {
  constructor() {
    this.ctx = null;
    this.master = null;
    this.enabled = true;
    this.noiseBuffer = null;
  }

  /** Precisa ser chamado a partir de um gesto do usuário. */
  init() {
    if (this.ctx) {
      if (this.ctx.state === 'suspended') this.ctx.resume();
      return;
    }
    const Ctx = window.AudioContext || window.webkitAudioContext;
    if (!Ctx) {
      this.enabled = false;
      return;
    }
    this.ctx = new Ctx();
    this.master = this.ctx.createGain();
    this.master.gain.value = 0.55;
    this.master.connect(this.ctx.destination);

    // buffer de ruído reaproveitado por todos os efeitos
    const len = Math.floor(this.ctx.sampleRate * 0.7);
    const buf = this.ctx.createBuffer(1, len, this.ctx.sampleRate);
    const data = buf.getChannelData(0);
    for (let i = 0; i < len; i++) data[i] = Math.random() * 2 - 1;
    this.noiseBuffer = buf;
  }

  now() {
    return this.ctx ? this.ctx.currentTime : 0;
  }

  _env(node, t0, peak, attack, decay) {
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(0.0001, t0);
    g.gain.exponentialRampToValueAtTime(peak, t0 + attack);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + attack + decay);
    node.connect(g);
    g.connect(this.master);
    return g;
  }

  _noise(t0, dur) {
    const src = this.ctx.createBufferSource();
    src.buffer = this.noiseBuffer;
    src.loop = true;
    src.start(t0);
    src.stop(t0 + dur + 0.05);
    return src;
  }

  /** Tiro de fuzil: estalo curto + corpo grave + cauda de sala. */
  shot() {
    if (!this.ctx || !this.enabled) return;
    const t = this.now();

    const crack = this._noise(t, 0.16);
    const hp = this.ctx.createBiquadFilter();
    hp.type = 'highpass';
    hp.frequency.value = 900;
    crack.connect(hp);
    this._env(hp, t, 0.9, 0.002, 0.11);

    const body = this.ctx.createOscillator();
    body.type = 'triangle';
    body.frequency.setValueAtTime(180, t);
    body.frequency.exponentialRampToValueAtTime(42, t + 0.13);
    this._env(body, t, 0.85, 0.003, 0.16);
    body.start(t);
    body.stop(t + 0.22);

    // cauda (reflexo da sala)
    const tail = this._noise(t + 0.05, 0.5);
    const lp = this.ctx.createBiquadFilter();
    lp.type = 'lowpass';
    lp.frequency.setValueAtTime(1600, t);
    lp.frequency.exponentialRampToValueAtTime(320, t + 0.5);
    tail.connect(lp);
    this._env(lp, t + 0.05, 0.14, 0.02, 0.45);
  }

  /** Seco (sem munição). */
  dry() {
    if (!this.ctx || !this.enabled) return;
    const t = this.now();
    const src = this._noise(t, 0.04);
    const bp = this.ctx.createBiquadFilter();
    bp.type = 'bandpass';
    bp.frequency.value = 2400;
    bp.Q.value = 2;
    src.connect(bp);
    this._env(bp, t, 0.25, 0.001, 0.05);
  }

  /** Recarga: dois cliques metálicos. */
  reload() {
    if (!this.ctx || !this.enabled) return;
    const t = this.now();
    [0, 0.34].forEach((delay, i) => {
      const src = this._noise(t + delay, 0.12);
      const bp = this.ctx.createBiquadFilter();
      bp.type = 'bandpass';
      bp.frequency.value = i === 0 ? 1800 : 3200;
      bp.Q.value = 6;
      src.connect(bp);
      this._env(bp, t + delay, 0.35, 0.002, 0.09);
    });
  }

  /** Impacto no cenário. */
  impact() {
    if (!this.ctx || !this.enabled) return;
    const t = this.now();
    const src = this._noise(t, 0.09);
    const bp = this.ctx.createBiquadFilter();
    bp.type = 'bandpass';
    bp.frequency.value = 1400 + Math.random() * 900;
    bp.Q.value = 1.4;
    src.connect(bp);
    this._env(bp, t, 0.22, 0.001, 0.08);
  }

  /** Alvo destruído: confirm tonal em duas vozes (estilo arcade). */
  hit(isCrit = false) {
    if (!this.ctx || !this.enabled) return;
    const t = this.now();
    const base = isCrit ? 1180 : 880;
    [1, 1.5].forEach((mult, i) => {
      const osc = this.ctx.createOscillator();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(base * mult, t);
      osc.frequency.exponentialRampToValueAtTime(base * mult * 0.72, t + 0.16);
      this._env(osc, t + i * 0.03, isCrit ? 0.35 : 0.24, 0.004, 0.18);
      osc.start(t + i * 0.03);
      osc.stop(t + 0.3);
    });
  }

  /** Fim de treino. */
  finish() {
    if (!this.ctx || !this.enabled) return;
    const t = this.now();
    [330, 440, 660].forEach((f, i) => {
      const osc = this.ctx.createOscillator();
      osc.type = 'triangle';
      osc.frequency.value = f;
      this._env(osc, t + i * 0.13, 0.22, 0.01, 0.5);
      osc.start(t + i * 0.13);
      osc.stop(t + i * 0.13 + 0.6);
    });
  }
}
