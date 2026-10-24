/**
 * Fuzil procedural (estilo carabina tática) montado com primitivas.
 * Nada de modelos externos: o "vibecoding" também vale para a arma.
 */
import * as THREE from 'three';
import { spark } from './textures.js';

export class Weapon {
  constructor() {
    this.group = new THREE.Group();
    this.group.name = 'weapon';

    const dark = new THREE.MeshStandardMaterial({ color: 0x14171b, roughness: 0.62, metalness: 0.72 });
    const mid = new THREE.MeshStandardMaterial({ color: 0x22272d, roughness: 0.5, metalness: 0.8 });
    const polymer = new THREE.MeshStandardMaterial({ color: 0x1b1e22, roughness: 0.92, metalness: 0.06 });
    const accent = new THREE.MeshStandardMaterial({
      color: 0xff6a13,
      emissive: 0xff4a00,
      emissiveIntensity: 1.2,
      roughness: 0.4,
      metalness: 0.2,
    });

    const add = (geo, mat, x, y, z, rx = 0, ry = 0, rz = 0) => {
      const m = new THREE.Mesh(geo, mat);
      m.position.set(x, y, z);
      m.rotation.set(rx, ry, rz);
      this.group.add(m);
      return m;
    };

    // corpo / receiver
    add(new THREE.BoxGeometry(0.06, 0.075, 0.34), dark, 0, 0, 0);
    // parte superior (trilho)
    add(new THREE.BoxGeometry(0.045, 0.022, 0.3), mid, 0, 0.048, 0.01);
    // cano
    add(new THREE.CylinderGeometry(0.011, 0.011, 0.34, 10), mid, 0, 0.008, -0.33, Math.PI / 2);
    // quebra-chama
    add(new THREE.CylinderGeometry(0.017, 0.014, 0.07, 8), dark, 0, 0.008, -0.51, Math.PI / 2);
    // guarda-mão
    add(new THREE.BoxGeometry(0.052, 0.05, 0.2), polymer, 0, -0.004, -0.24);
    // punho
    add(new THREE.BoxGeometry(0.042, 0.11, 0.05), polymer, 0, -0.09, 0.1, 0.24);
    // coronha
    add(new THREE.BoxGeometry(0.045, 0.075, 0.2), polymer, 0, -0.012, 0.24);
    add(new THREE.BoxGeometry(0.05, 0.11, 0.04), polymer, 0, -0.02, 0.35);
    // carregador
    this.magazine = add(new THREE.BoxGeometry(0.036, 0.13, 0.06), mid, 0, -0.09, -0.03, -0.18);
    // mira traseira + dianteira
    add(new THREE.BoxGeometry(0.032, 0.026, 0.014), dark, 0, 0.07, 0.09);
    add(new THREE.BoxGeometry(0.006, 0.02, 0.006), dark, 0, 0.078, 0.085);
    add(new THREE.BoxGeometry(0.03, 0.03, 0.012), dark, 0, 0.062, -0.42);
    add(new THREE.BoxGeometry(0.005, 0.022, 0.005), accent, 0, 0.075, -0.42);
    // detalhe laranja
    add(new THREE.BoxGeometry(0.062, 0.008, 0.06), accent, 0, -0.03, -0.12);
    // trava do gatilho
    add(new THREE.BoxGeometry(0.03, 0.03, 0.02), dark, 0, -0.05, 0.05);

    // ---------------- boca do cano + flash ----------------
    this.muzzle = new THREE.Object3D();
    this.muzzle.position.set(0, 0.008, -0.56);
    this.group.add(this.muzzle);

    const flashMat = new THREE.SpriteMaterial({
      map: spark(),
      color: 0xffcf8a,
      transparent: true,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
      opacity: 0,
    });
    this.flash = new THREE.Sprite(flashMat);
    this.flash.scale.setScalar(0.5);
    this.muzzle.add(this.flash);

    this.flashLight = new THREE.PointLight(0xffb060, 0, 6, 2);
    this.flashLight.position.copy(this.muzzle.position);
    this.group.add(this.flashLight);

    // ---------------- animação ----------------
    this.basePos = new THREE.Vector3(0, 0, 0);
    this.recoil = 0;
    this.recoilRot = 0;
    this.flashTimer = 0;
    this.reloadT = 0;
    this.reloading = false;
    this.swayTarget = new THREE.Vector2();

    // pose inicial (desktop)
    this.group.position.set(0.16, -0.17, -0.36);
    this.group.rotation.set(0, 0.03, 0);
    this.homePos = this.group.position.clone();
    this.homeRot = this.group.rotation.clone();
  }

  /** Dispara a animação de tiro (o raycast fica no main). */
  kick() {
    this.recoil = 1;
    this.recoilRot = 1;
    this.flashTimer = 0.055;
    this.flash.material.opacity = 1;
    this.flash.material.rotation = Math.random() * Math.PI;
    this.flash.scale.setScalar(0.42 + Math.random() * 0.22);
    this.flashLight.intensity = 26;
    this.flashLight.position.copy(this.muzzle.position);
  }

  startReload(duration = 1.35) {
    this.reloading = true;
    this.reloadT = 0;
    this.reloadDuration = duration;
  }

  cancelReload() {
    this.reloading = false;
    this.reloadT = 0;
  }

  /** Posição da boca do cano no mundo (para traçantes). */
  muzzleWorld(target = new THREE.Vector3()) {
    return this.muzzle.getWorldPosition(target);
  }

  /**
   * @param {number} dt
   * @param {THREE.Vector2} lookDelta movimento do mouse/analógico para o "sway"
   */
  update(dt, lookDelta) {
    // flash decai
    if (this.flashTimer > 0) {
      this.flashTimer -= dt;
      this.flash.material.opacity = Math.max(0, this.flashTimer / 0.055);
      this.flashLight.intensity = Math.max(0, this.flashLight.intensity - dt * 500);
    } else {
      this.flash.material.opacity = 0;
      this.flashLight.intensity = 0;
    }

    // recuo
    this.recoil = THREE.MathUtils.damp(this.recoil, 0, 12, dt);
    this.recoilRot = THREE.MathUtils.damp(this.recoilRot, 0, 10, dt);

    // sway suave (respiração + mouse)
    const t = performance.now() * 0.001;
    const swayX = Math.sin(t * 1.3) * 0.004;
    const swayY = Math.cos(t * 1.7) * 0.003;
    if (lookDelta) {
      this.swayTarget.lerp(lookDelta, 0.12);
      this.swayTarget.multiplyScalar(0.94);
    }

    this.group.position.copy(this.homePos);
    this.group.rotation.copy(this.homeRot);

    this.group.position.x += swayX + this.swayTarget.x * 0.02;
    this.group.position.y += swayY + this.swayTarget.y * 0.02;
    this.group.position.z += this.recoil * 0.075;
    this.group.rotation.x += this.recoilRot * 0.11;
    this.group.rotation.z += this.swayTarget.x * 0.03;

    // recarga
    if (this.reloading) {
      this.reloadT += dt;
      const k = Math.min(1, this.reloadT / this.reloadDuration);
      // curva: desce, gira, volta
      const dip = Math.sin(k * Math.PI);
      this.group.position.y -= dip * 0.16;
      this.group.position.z += dip * 0.05;
      this.group.rotation.x -= dip * 0.5;
      this.group.rotation.z += dip * 0.28;

      // carregador sai e entra
      const magK = k < 0.5 ? k * 2 : (1 - k) * 2;
      this.magazine.position.y = -0.09 - magK * 0.22;
      this.magazine.rotation.x = -0.18 - magK * 0.5;

      if (k >= 1) {
        this.reloading = false;
        this.magazine.position.y = -0.09;
        this.magazine.rotation.x = -0.18;
      }
    }
  }

  setVisible(v) {
    this.group.visible = v;
  }
}
