/**
 * BLACK VR — Protocolo Zero
 * Ponto de entrada: monta a cena, liga os sistemas e roda o loop.
 */
import * as THREE from 'three';
import { VRButton } from 'three/addons/webxr/VRButton.js';
import { buildWorld, ROOM } from './world.js';
import { TargetField } from './targets.js';
import { Effects } from './effects.js';
import { Weapon } from './weapon.js';
import { Player } from './player.js';
import { Hud } from './hud.js';
import { SynthAudio } from './audio.js';

const ROUND_SECONDS = 75;
const MAG_SIZE = 12;
const RELOAD_TIME = 1.35;

// ------------------------------------------------------------------ cena
const container = document.getElementById('app');
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x04060a);

const camera = new THREE.PerspectiveCamera(70, window.innerWidth / window.innerHeight, 0.05, 220);

const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.06;
renderer.xr.enabled = true;
renderer.xr.setReferenceSpaceType('local-floor');
container.appendChild(renderer.domElement);

// ------------------------------------------------------------------ sistemas
const world = buildWorld(scene);
const targets = new TargetField(scene, world.shootable);
const effects = new Effects(scene);
const weapon = new Weapon();
const audio = new SynthAudio();
const hud = new Hud();

scene.add(weapon.group);
camera.add(weapon.group);
weapon.group.position.set(0.165, -0.17, -0.36);
weapon.homePos.copy(weapon.group.position);
weapon.homeRot.copy(weapon.group.rotation);

const player = new Player({ renderer, camera, colliders: world.colliders, weapon });
scene.add(player.rig);
player.setupXR();

// alvos entram no raycast
const raycastTargets = [...world.shootable, ...targets.targets.map((t) => t.plate)];

// ------------------------------------------------------------------ estado
const state = {
  mode: 'menu', // menu | playing | results
  timeLeft: ROUND_SECONDS,
  score: 0,
  shots: 0,
  hits: 0,
  streak: 0,
  bestStreak: 0,
  mult: 1,
  streakTimer: 0,
  ammo: MAG_SIZE,
  reloading: false,
  reloadTimer: 0,
  paused: false,
  startedAt: 0,
};

const ui = {
  menu: document.getElementById('menu'),
  results: document.getElementById('results'),
  boot: document.getElementById('boot'),
  btnStart: document.getElementById('btn-start'),
  btnAgain: document.getElementById('btn-again'),
  vrSlot: document.getElementById('vr-slot'),
  resScore: document.getElementById('res-score'),
  resHits: document.getElementById('res-hits'),
  resAcc: document.getElementById('res-acc'),
  resCombo: document.getElementById('res-combo'),
};

// botão oficial de entrada em VR (aparece só se o navegador suportar)
try {
  const vrButton = VRButton.createButton(renderer, {
    optionalFeatures: ['local-floor', 'bounded-floor', 'hand-tracking'],
  });
  if (vrButton) ui.vrSlot.appendChild(vrButton);
} catch (err) {
  console.warn('[black-vr] WebXR indisponível:', err);
}

// ------------------------------------------------------------------ fluxo
function startRound() {
  audio.init();

  state.mode = 'playing';
  state.timeLeft = ROUND_SECONDS;
  state.score = 0;
  state.shots = 0;
  state.hits = 0;
  state.streak = 0;
  state.bestStreak = 0;
  state.mult = 1;
  state.streakTimer = 0;
  state.ammo = MAG_SIZE;
  state.reloading = false;
  state.reloadTimer = 0;
  state.paused = false;
  state.startedAt = performance.now();

  targets.reset();
  targets.setMaxActive(3);
  effects.clear();
  weapon.cancelReload();
  hud.reset();
  hud.show();
  hud.setCrosshairVisible(!player.inXR);

  ui.menu.classList.add('hidden');
  ui.results.classList.add('hidden');

  player.rig.position.copy(ROOM.spawn);
  player.yaw = 0;
  player.pitch = 0;
  player.velocity.set(0, 0, 0);
  player.setDesktopEnabled(true);
  player.requestPointer();
  if (player.inXR) hud.toast('APONTE E APERTE O GATILHO', 2200);
}

function endRound() {
  state.mode = 'results';
  player.setDesktopEnabled(false);
  weapon.cancelReload();
  audio.finish();

  const acc = state.shots ? Math.round((state.hits / state.shots) * 100) : 0;
  ui.resScore.textContent = state.score.toLocaleString('pt-BR');
  ui.resHits.textContent = state.hits;
  ui.resAcc.textContent = `${acc}%`;
  ui.resCombo.textContent = `x${Math.max(1, 1 + Math.floor(state.bestStreak / 2))}`;
  ui.results.classList.remove('hidden');
  hud.hide();
  player.releasePointer();
}

ui.btnStart.addEventListener('click', startRound);
ui.btnAgain.addEventListener('click', startRound);

// recomenda humanidade: no headset, o gatilho reinicia
player.onSelect = () => {
  if (state.mode === 'results') startRound();
};

// ------------------------------------------------------------------ tiro
function currentMult() {
  return Math.min(5, 1 + Math.floor(state.streak / 2));
}

function fire() {
  if (state.mode !== 'playing' || state.paused) return;

  if (state.reloading) return;
  if (state.ammo <= 0) {
    audio.dry();
    beginReload();
    return;
  }

  state.ammo -= 1;
  state.shots += 1;
  weapon.kick();
  audio.shot();
  hud.setAmmo(state.ammo, MAG_SIZE, state.reloading);

  const { origin, dir } = player.getAimRay();
  const hits = player.look(origin, dir, raycastTargets, 120);
  const from = weapon.muzzleWorld(new THREE.Vector3());

  let hitTarget = null;
  let hitPoint = null;
  let hitNormal = null;

  // primeira superfície válida ganha. Alvos escondidos (visible=false / já destruídos)
  // são ignorados para não bloquear o tiro.
  for (const h of hits) {
    const target = h.object.userData.target;
    if (target) {
      if (!isVisible(h.object)) continue;
      if (target.state === 'down' || target.state === 'dead') continue;
      hitTarget = target;
    } else if (!isVisible(h.object)) {
      continue;
    }
    hitPoint = h.point.clone();
    hitNormal = h.face
      ? h.face.normal.clone().transformDirection(h.object.matrixWorld)
      : dir.clone().negate();
    break;
  }

  if (hitPoint) {
    effects.tracer(from, hitPoint);
  }

  if (hitTarget) {
    const res = targets.hit(hitTarget, hitPoint);
    if (res) {
      state.hits += 1;
      state.streak += 1;
      state.bestStreak = Math.max(state.bestStreak, state.streak);
      state.streakTimer = 3.0;
      state.mult = currentMult();

      const base = res.crit ? 250 : 100;
      const gained = base * state.mult;
      state.score += gained;

      hud.hitmarker(res.crit);
      hud.setScore(state.score);
      hud.setHits(state.hits);
      hud.setCombo(state.mult);
      hud.setAccuracy(state.hits, state.shots);
      audio.hit(res.crit);
      if (res.crit) hud.toast(`CRÍTICO +${gained}`, 700);
      effects.sparks(hitPoint, hitNormal, 6);
    }
  } else if (hitPoint) {
    // impacto no cenário
    effects.decal(hitPoint, hitNormal);
    effects.sparks(hitPoint, hitNormal, 3);
    audio.impact();
    // errar zera o combo
    state.streak = 0;
    state.mult = 1;
    hud.setCombo(1);
    hud.setAccuracy(state.hits, state.shots);
  } else {
    state.streak = 0;
    state.mult = 1;
    hud.setCombo(1);
    hud.setAccuracy(state.hits, state.shots);
  }
}

function beginReload() {
  if (state.reloading || state.ammo === MAG_SIZE || state.mode !== 'playing') return;
  state.reloading = true;
  state.reloadTimer = RELOAD_TIME;
  weapon.startReload(RELOAD_TIME);
  audio.reload();
  hud.setAmmo(state.ammo, MAG_SIZE, true);
}

function finishReload() {
  state.reloading = false;
  state.ammo = MAG_SIZE;
  weapon.cancelReload();
  hud.setAmmo(state.ammo, MAG_SIZE, false);
}

// ------------------------------------------------------------------ entradas
// clique curto atira; arrastar apenas olha (funciona mesmo sem pointer lock,
// o que é essencial quando o demo roda dentro de um iframe)
let pressPos = null;

window.addEventListener('mousedown', (e) => {
  if (state.mode !== 'playing' || player.inXR || e.button !== 0) return;
  pressPos = { x: e.clientX, y: e.clientY };
  if (document.pointerLockElement !== renderer.domElement) player.requestPointer();
});

window.addEventListener('mouseup', (e) => {
  if (!pressPos) return;
  const moved = Math.hypot(e.clientX - pressPos.x, e.clientY - pressPos.y);
  pressPos = null;
  if (state.mode !== 'playing' || player.inXR) return;
  if (moved > 8) return; // foi um arrasto para olhar, não um clique
  fire();
});

window.addEventListener('keydown', (e) => {
  if (e.code === 'KeyR') beginReload();
  if (e.code === 'Enter' && state.mode !== 'playing') startRound();
});

player.onReload = beginReload;

// ouvintes XR de gatilho
document.addEventListener('visibilitychange', () => {
  if (document.hidden) player.releasePointer();
});

window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// ------------------------------------------------------------------ loop
const clock = new THREE.Clock();
let difficulty = 1;
let elapsed = 0;

renderer.setAnimationLoop(() => {
  const dt = Math.min(clock.getDelta(), 0.05);
  elapsed += dt;

  // pausa quando a aba perde o foco (evita perder o round ao trocar de janela)
  state.paused = document.hidden && state.mode === 'playing';

  const playing = state.mode === 'playing' && !state.paused;

  if (playing) {
    state.timeLeft -= dt;

    // dificuldade cresce com o tempo
    const progress = 1 - state.timeLeft / ROUND_SECONDS;
    difficulty = 1 + progress * 1.35;
    targets.setMaxActive(progress > 0.55 ? 5 : progress > 0.25 ? 4 : 3);

    if (state.streakTimer > 0) {
      state.streakTimer -= dt;
      if (state.streakTimer <= 0) {
        state.streak = 0;
        state.mult = 1;
        hud.setCombo(1);
      }
    }

    if (state.reloading) {
      state.reloadTimer -= dt;
      if (state.reloadTimer <= 0) finishReload();
    }

    hud.setTime(state.timeLeft);
    if (state.timeLeft <= 0) endRound();
  }

  targets.update(dt, playing, difficulty);
  effects.update(dt);
  world.update(dt, elapsed);
  player.update(dt, { playing });
  hud.setCrosshairVisible(!player.inXR && state.mode === 'playing');

  renderer.render(scene, camera);
});

// ------------------------------------------------------------------ boot
window.addEventListener('load', () => {
  ui.boot.classList.add('done');
  setTimeout(() => ui.boot.classList.add('hidden'), 500);
});

// pequena dica caso o WebGL falhe
if (!renderer.getContext()) {
  const el = document.getElementById('error');
  el.textContent = 'WebGL indisponível neste dispositivo.';
  el.classList.remove('hidden');
}

/** Objeto e todos os ancestrais visíveis? (raycaster do three não checa visible sozinho) */
function isVisible(object) {
  let node = object;
  while (node) {
    if (node.visible === false) return false;
    node = node.parent;
  }
  return true;
}
