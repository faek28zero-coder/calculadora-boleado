/**
 * Efeitos visuais: faíscas de impacto, marcas de bala e traçantes.
 * Pool de objetos para não gerar lixo no loop de render.
 */
import * as THREE from 'three';
import { bulletHole, spark } from './textures.js';

export class Effects {
  constructor(scene) {
    this.scene = scene;
    this.sparkTex = spark();
    this.holeTex = bulletHole();

    // ---------------- marcas de bala (decals) ----------------
    this.holeMat = new THREE.MeshBasicMaterial({
      map: this.holeTex,
      transparent: true,
      depthWrite: false,
      polygonOffset: true,
      polygonOffsetFactor: -4,
      opacity: 0.95,
    });
    this.holeGeo = new THREE.PlaneGeometry(0.12, 0.12);
    this.holes = [];
    this.maxHoles = 60;

    // ---------------- faíscas ----------------
    this.sparkPool = [];
    this.sparkGeo = new THREE.PlaneGeometry(0.14, 0.14);
    for (let i = 0; i < 26; i++) {
      const mat = new THREE.SpriteMaterial({
        map: this.sparkTex,
        transparent: true,
        blending: THREE.AdditiveBlending,
        depthWrite: false,
        opacity: 0,
      });
      const sprite = new THREE.Sprite(mat);
      sprite.visible = false;
      scene.add(sprite);
      this.sparkPool.push({ sprite, life: 0, maxLife: 1, vel: new THREE.Vector3() });
    }
    this.sparkCursor = 0;

    // ---------------- traçantes ----------------
    this.tracers = [];
    this.tracerMat = new THREE.MeshBasicMaterial({
      color: 0xffd9a0,
      transparent: true,
      opacity: 0.85,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
      toneMapped: false,
    });
  }

  /** Marca de bala na superfície atingida. */
  decal(point, normal) {
    const mesh = new THREE.Mesh(this.holeGeo, this.holeMat);
    mesh.position.copy(point).addScaledVector(normal, 0.012);
    const q = new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 0, 1), normal.clone().normalize());
    mesh.quaternion.copy(q);
    mesh.rotateZ(Math.random() * Math.PI * 2);
    const s = 0.75 + Math.random() * 0.6;
    mesh.scale.setScalar(s);
    this.scene.add(mesh);
    this.holes.push(mesh);
    if (this.holes.length > this.maxHoles) {
      const old = this.holes.shift();
      this.scene.remove(old);
    }
  }

  /** Faíscas + leve fumaça no ponto de impacto. */
  sparks(point, normal, count = 3) {
    for (let i = 0; i < count; i++) {
      const p = this.sparkPool[this.sparkCursor];
      this.sparkCursor = (this.sparkCursor + 1) % this.sparkPool.length;
      p.sprite.position.copy(point);
      p.sprite.visible = true;
      p.sprite.material.opacity = 1;
      p.sprite.material.rotation = Math.random() * Math.PI;
      p.sprite.scale.setScalar(0.6 + Math.random() * 0.7);
      p.life = 0;
      p.maxLife = 0.16 + Math.random() * 0.14;
      p.vel
        .copy(normal)
        .multiplyScalar(1.6 + Math.random() * 2.4)
        .add(
          new THREE.Vector3(
            (Math.random() - 0.5) * 1.6,
            Math.random() * 1.4,
            (Math.random() - 0.5) * 1.6
          )
        );
    }
  }

  /** Traçante da boca do cano até o impacto. */
  tracer(from, to) {
    const dir = new THREE.Vector3().subVectors(to, from);
    const len = dir.length();
    if (len < 0.01) return;
    const geo = new THREE.CylinderGeometry(0.004, 0.0016, len, 5, 1, true);
    const mesh = new THREE.Mesh(geo, this.tracerMat.clone());
    mesh.position.copy(from).addScaledVector(dir, 0.5);
    mesh.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir.normalize());
    this.scene.add(mesh);
    this.tracers.push({ mesh, life: 0, maxLife: 0.07 });
  }

  update(dt) {
    // faíscas
    for (const p of this.sparkPool) {
      if (!p.sprite.visible) continue;
      p.life += dt;
      const k = p.life / p.maxLife;
      if (k >= 1) {
        p.sprite.visible = false;
        p.sprite.material.opacity = 0;
        continue;
      }
      p.vel.y -= 9.8 * dt;
      p.sprite.position.addScaledVector(p.vel, dt);
      p.sprite.material.opacity = 1 - k;
      p.sprite.scale.setScalar(0.7 * (1 - k * 0.5));
    }

    // traçantes
    for (let i = this.tracers.length - 1; i >= 0; i--) {
      const t = this.tracers[i];
      t.life += dt;
      t.mesh.material.opacity = Math.max(0, 1 - t.life / t.maxLife) * 0.85;
      if (t.life >= t.maxLife) {
        this.scene.remove(t.mesh);
        t.mesh.geometry.dispose();
        t.mesh.material.dispose();
        this.tracers.splice(i, 1);
      }
    }
  }

  clear() {
    this.holes.forEach((h) => this.scene.remove(h));
    this.holes.length = 0;
    this.sparkPool.forEach((p) => {
      p.sprite.visible = false;
      p.sprite.material.opacity = 0;
    });
    this.tracers.forEach((t) => {
      this.scene.remove(t.mesh);
      t.mesh.geometry.dispose();
      t.mesh.material.dispose();
    });
    this.tracers.length = 0;
  }
}
