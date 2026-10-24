/**
 * Alvos pop-up do estande.
 * Cada alvo é uma prancha com silhueta genérica que sobe/desce do chão.
 * Zona central vale mais (crítico).
 */
import * as THREE from 'three';
import { targetFace } from './textures.js';

const UP_TIME = 2.6; // segundos visível
const RISE_TIME = 0.22;
const FALL_TIME = 0.18;

export class TargetField {
  constructor(scene, shootable) {
    this.group = new THREE.Group();
    this.group.name = 'targets';
    scene.add(this.group);

    this.targets = [];
    this.shootable = shootable; // alvos entram na lista de raycast do mundo
    this.points = 0;

    const faceTex = targetFace();
    const frameMat = new THREE.MeshStandardMaterial({ color: 0x1a1f25, roughness: 0.8, metalness: 0.3 });
    const faceMat = new THREE.MeshStandardMaterial({
      map: faceTex,
      roughness: 0.7,
      metalness: 0.05,
      emissive: 0xffffff,
      emissiveIntensity: 0.06,
      emissiveMap: faceTex,
    });

    // ------------------------------------------------------ pontos de spawn
    // x, z, altura da base, escala
    const layout = [
      [-9.4, -8.5, 1.0, 1.0],
      [-6.0, -13.0, 2.4, 0.85],
      [-3.0, -7.5, 1.3, 0.9],
      [0.0, -16.0, 3.0, 0.8],
      [2.6, -9.0, 1.4, 1.0],
      [5.6, -12.5, 2.2, 0.9],
      [9.2, -7.8, 1.1, 1.0],
      [-11.0, -16.5, 1.8, 0.8],
      [11.4, -15.5, 2.6, 0.85],
      [-1.4, -12.0, 4.5, 0.75],
      [4.0, -6.4, 0.9, 0.75],
      [-7.6, -6.6, 0.9, 0.75],
    ];

    layout.forEach(([x, z, y, scale], i) => {
      const pivot = new THREE.Group();
      pivot.position.set(x, 0, z);

      const board = new THREE.Group();
      board.position.y = -0.95 * scale; // escondido atrás do chão/barreira

      const w = 1.05 * scale;
      const h = 1.35 * scale;

      // prancha
      const plate = new THREE.Mesh(new THREE.BoxGeometry(w, h, 0.06), faceMat.clone());
      plate.position.y = 0;
      plate.name = 'plate';
      board.add(plate);

      // moldura
      const frame = new THREE.Mesh(new THREE.BoxGeometry(w + 0.08, h + 0.08, 0.03), frameMat);
      frame.position.z = -0.045;
      board.add(frame);

      // haste
      const post = new THREE.Mesh(new THREE.CylinderGeometry(0.04, 0.04, 1.05 * scale, 8), frameMat);
      post.position.y = -h / 2 - 0.5 * scale;
      board.add(post);

      // base
      const base = new THREE.Mesh(new THREE.BoxGeometry(0.9 * scale, 0.12, 0.55 * scale), frameMat);
      base.position.y = -h / 2 - 0.98 * scale;
      board.add(base);

      board.position.y = y - h / 2 - 0.9 * scale;
      board.userData.restY = board.position.y;
      board.userData.topY = y - h / 2 - 0.9 * scale + 1.9 * scale;

      pivot.add(board);
      this.group.add(pivot);

      const target = {
        id: i,
        pivot,
        board,
        plate,
        restY: board.userData.restY,
        topY: board.userData.topY,
        state: 'down', // down | rising | up | falling | dead
        timer: 0,
        visibleFor: 0,
        cooldown: 0,
        scale,
        halfW: w / 2,
        halfH: h / 2,
        critRadius: 0.19 * scale,
      };

      plate.userData.target = target;
      this.targets.push(target);

      // posiciona escondido
      board.position.y = target.restY;
    });

    this.activeCount = 0;
    this.maxActive = 4;
    this.spawnTimer = 0.8;
  }

  /** Volume máximo de alvos simultâneos (dificuldade). */
  setMaxActive(n) {
    this.maxActive = n;
  }

  reset() {
    this.targets.forEach((t) => {
      t.state = 'down';
      t.board.position.y = t.restY;
      t.timer = 0;
      t.cooldown = 0;
      t.plate.visible = true;
      t.plate.scale.set(1, 1, 1);
      t.board.rotation.z = 0;
    });
    this.activeCount = 0;
    this.spawnTimer = 0.4;
  }

  update(dt, playing, difficulty = 1) {
    if (!playing) {
      // esconde tudo devagar
      this.targets.forEach((t) => {
        if (t.state !== 'down') {
          t.board.position.y = THREE.MathUtils.damp(t.board.position.y, t.restY, 8, dt);
        }
      });
      return;
    }

    this.spawnTimer -= dt;

    const upTargets = this.targets.filter((t) => t.state === 'up' || t.state === 'rising').length;
    if (this.spawnTimer <= 0 && upTargets < this.maxActive) {
      this.spawn(difficulty);
      this.spawnTimer = Math.max(0.28, 1.15 / difficulty);
    }

    for (const t of this.targets) {
      switch (t.state) {
        case 'rising': {
          t.timer += dt;
          const k = Math.min(1, t.timer / RISE_TIME);
          t.board.position.y = THREE.MathUtils.lerp(t.restY, t.topY, easeOutBack(k));
          if (k >= 1) {
            t.state = 'up';
            t.visibleFor = 0;
          }
          break;
        }
        case 'up': {
          t.visibleFor += dt;
          // pequena oscilação de "mola" do alvo
          t.board.rotation.z = Math.sin(t.visibleFor * 6 + t.id) * 0.012;
          if (t.visibleFor > UP_TIME / difficulty) t.state = 'falling';
          break;
        }
        case 'falling': {
          t.timer += dt;
          const k = Math.min(1, t.timer / FALL_TIME);
          t.board.position.y = THREE.MathUtils.lerp(t.topY, t.restY, k * k);
          if (k >= 1) {
            t.state = 'down';
            t.timer = 0;
          }
          break;
        }
        case 'dead': {
          t.timer += dt;
          t.board.position.y = THREE.MathUtils.lerp(t.board.position.y, t.restY, Math.min(1, dt * 10));
          if (t.timer > 0.55) {
            t.state = 'down';
            t.timer = 0;
          }
          break;
        }
        default:
          break;
      }
    }
  }

  spawn(difficulty) {
    const free = this.targets.filter((t) => t.state === 'down');
    if (!free.length) return;
    const t = free[Math.floor(Math.random() * free.length)];
    t.state = 'rising';
    t.timer = 0;
    t.visibleFor = 0;
    t.plate.visible = true;
    t.plate.scale.set(1, 1, 1);
    t.plate.material.emissiveIntensity = 0.06;
    t.board.rotation.z = 0;
    // sobe de um ponto um pouco aleatório para variar a silhueta
    t.topY = t.board.userData.topY + (Math.random() - 0.5) * 0.18;
  }

  /**
   * Registra um tiro em um alvo.
   * @returns {{points:number, crit:boolean}|null} null se o alvo já estava morto
   */
  hit(target, point) {
    if (!target || target.state === 'dead' || target.state === 'down') return null;

    // distância do impacto ao centro da prancha, em espaço local
    const local = target.plate.worldToLocal(point.clone());
    const dist = Math.hypot(local.x, local.y);
    const crit = dist < target.critRadius;

    target.state = 'dead';
    target.timer = 0;
    target.plate.visible = false;
    target.board.rotation.z = (Math.random() - 0.5) * 0.9;

    return { crit };
  }

  get active() {
    return this.targets.filter((t) => t.state === 'up' || t.state === 'rising');
  }
}

function easeOutBack(k) {
  const c1 = 1.70158;
  const c3 = c1 + 1;
  return 1 + c3 * Math.pow(k - 1, 3) + c1 * Math.pow(k - 1, 2);
}
