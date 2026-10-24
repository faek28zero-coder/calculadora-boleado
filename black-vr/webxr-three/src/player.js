/**
 * Jogador: rig de câmera + controles desktop (WASD/mouse) e XR
 * (locomoção por analógico, snap turn e mira física com o controle).
 */
import * as THREE from 'three';
import { ROOM } from './world.js';

const EYE_HEIGHT = 1.68;
const RADIUS = 0.38;

export class Player {
  constructor({ renderer, camera, colliders, weapon }) {
    this.renderer = renderer;
    this.camera = camera;
    this.colliders = colliders;
    this.weapon = weapon;

    this.rig = new THREE.Group();
    this.rig.name = 'player-rig';
    this.rig.add(camera);
    camera.position.set(0, EYE_HEIGHT, 0);
    this.rig.position.copy(ROOM.spawn);
    this.camera.rotation.set(0, 0, 0);

    this.yaw = 0;
    this.pitch = 0;
    this.velocity = new THREE.Vector3();
    this.keys = new Set();
    this.lookDelta = new THREE.Vector2();
    this.enabled = false;
    this.inXR = false;
    this.locked = false;
    this.turnCooldown = 0;
    this.bobTime = 0;
    this.dragging = false;
    this.dragDistance = 0;

    this.right = null;
    this.left = null;
    this.gripRight = null;
    this.gripLeft = null;
    this.controllerRay = new THREE.Raycaster();

    this._onKeyDown = (e) => {
      this.keys.add(e.code);
      if (e.code === 'Escape') this.releasePointer();
    };
    this._onKeyUp = (e) => this.keys.delete(e.code);
    this._onMouseMove = (e) => {
      // com pointer lock: sempre. sem lock: arrastando (funciona dentro de iframes)
      if (!this.locked && !this.dragging) return;
      if (this.dragging) this.dragDistance += Math.hypot(e.movementX || 0, e.movementY || 0);
      const sens = 0.0022;
      this.yaw -= e.movementX * sens;
      this.pitch -= e.movementY * sens;
      this.pitch = THREE.MathUtils.clamp(this.pitch, -Math.PI / 2 + 0.05, Math.PI / 2 - 0.05);
      this.lookDelta.set(
        THREE.MathUtils.clamp(e.movementX * 0.02, -1, 1),
        THREE.MathUtils.clamp(e.movementY * 0.02, -1, 1)
      );
    };

    this._onMouseDown = (e) => {
      if (e.button !== 0 || this.inXR) return;
      this.dragging = true;
      this.dragDistance = 0;
    };
    this._onMouseUp = () => { this.dragging = false; };

    window.addEventListener('keydown', this._onKeyDown);
    window.addEventListener('keyup', this._onKeyUp);
    document.addEventListener('mousemove', this._onMouseMove);
    window.addEventListener('mousedown', this._onMouseDown);
    window.addEventListener('mouseup', this._onMouseUp);
  }

  // ------------------------------------------------------------ XR
  setupXR() {
    const r = this.renderer;

    this.right = r.xr.getController(0);
    this.left = r.xr.getController(1);
    this.rig.add(this.right, this.left);

    // modelos de controle simples (cápsulas) — sem dependência de perfis remotos
    const ctrlMat = new THREE.MeshStandardMaterial({ color: 0x1a1e23, roughness: 0.5, metalness: 0.6 });
    const glowMat = new THREE.MeshStandardMaterial({
      color: 0xff6a13,
      emissive: 0xff4a00,
      emissiveIntensity: 1.4,
      roughness: 0.4,
    });

    const build = (ctrl) => {
      const body = new THREE.Mesh(new THREE.CapsuleGeometry(0.021, 0.075, 4, 8), ctrlMat);
      body.position.set(0, 0, -0.02);
      body.rotation.x = Math.PI / 2.4;
      ctrl.add(body);
      const dot = new THREE.Mesh(new THREE.SphereGeometry(0.008, 8, 8), glowMat);
      dot.position.set(0, 0.012, -0.075);
      ctrl.add(dot);

      const ray = new THREE.Line(
        new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(0, 0, 0), new THREE.Vector3(0, 0, -1)]),
        new THREE.LineBasicMaterial({ color: 0xff6a13, transparent: true, opacity: 0.35 })
      );
      ray.scale.z = 3;
      ctrl.add(ray);
    };
    build(this.right);
    build(this.left);

    // a arma vive na mão direita (ou na esquerda, se o controle for canhoto)
    this.weapon.group.position.set(0.02, -0.045, -0.14);
    this.weapon.group.rotation.set(0.06, 0, 0);
    this.weapon.group.scale.setScalar(1);
    this.weapon.homePos.copy(this.weapon.group.position);
    this.weapon.homeRot.copy(this.weapon.group.rotation);
    this.right.add(this.weapon.group);

    // seleção de handedness
    r.xr.addEventListener('sessionstart', () => {
      this.inXR = true;
      this.enabled = false;
      document.body.classList.add('in-xr');
      this.releasePointer();
      this.applyHandedness();
    });

    r.xr.addEventListener('sessionend', () => {
      this.inXR = false;
      document.body.classList.remove('in-xr');
      // devolve a arma para a câmera
      this.camera.add(this.weapon.group);
      this.weapon.group.position.set(0.16, -0.17, -0.36);
      this.weapon.group.rotation.set(0, 0.03, 0);
      this.weapon.homePos.copy(this.weapon.group.position);
      this.weapon.homeRot.copy(this.weapon.group.rotation);
      this.camera.position.set(0, EYE_HEIGHT, 0);
      this.rig.position.copy(ROOM.spawn);
      this.yaw = 0;
      this.pitch = 0;
    });

    this.attachInputSources();
    r.xr.addEventListener('sessionstart', () => this.attachInputSources());
  }

  applyHandedness() {
    const src = this.renderer.xr.getSession()?.inputSources || [];
    const right = src.find((s) => s.handedness === 'right');
    const target = right ? this.right : this.left;
    if (this.weapon.group.parent !== target) {
      target.add(this.weapon.group);
      this.weapon.group.position.set(0.02, -0.045, -0.14);
      this.weapon.group.rotation.set(0.06, 0, 0);
      this.weapon.homePos.copy(this.weapon.group.position);
      this.weapon.homeRot.copy(this.weapon.group.rotation);
    }
  }

  attachInputSources() {
    const session = this.renderer.xr.getSession();
    if (!session) return;

    const bind = (source, controller) => {
      // gatilho / select
      controller.addEventListener('selectstart', () => {
        if (this.onSelect) this.onSelect(source.handedness);
      });
      // botão de recarga (squeeze ou A/X)
      controller.addEventListener('squeezestart', () => {
        if (this.onReload) this.onReload();
      });
      source.addEventListener('inputsourceschange', () => {});
    };

    session.addEventListener('inputsourceschange', () => {
      this.applyHandedness();
      const sources = session.inputSources;
      sources.forEach((src) => {
        if (src._bound) return;
        src._bound = true;
        const ctrl = src.handedness === 'left' ? this.left : this.right;
        bind(src, ctrl);
      });
    });

    // ligação inicial
    session.inputSources.forEach((src) => {
      if (src._bound) return;
      src._bound = true;
      const ctrl = src.handedness === 'left' ? this.left : this.right;
      bind(src, ctrl);
    });
  }

  // ------------------------------------------------------------ desktop
  requestPointer() {
    if (this.inXR) return;
    const canvas = this.renderer.domElement;
    canvas.requestPointerLock?.();
  }

  releasePointer() {
    if (document.pointerLockElement) document.exitPointerLock();
  }

  setDesktopEnabled(v) {
    this.enabled = v;
    if (!v) {
      this.keys.clear();
      this.releasePointer();
      this.lookDelta.set(0, 0);
    }
  }

  // ------------------------------------------------------------ loop
  update(dt, { playing }) {
    this.turnCooldown = Math.max(0, this.turnCooldown - dt);
    this.lookDelta.multiplyScalar(Math.max(0, 1 - dt * 6));

    if (this.inXR) {
      this.updateXR(dt, playing);
    } else {
      this.updateDesktop(dt, playing);
    }

    this.resolveCollisions();

    // arma
    const look = this.inXR ? null : this.lookDelta;
    this.weapon.update(dt, look);
  }

  updateDesktop(dt, playing) {
    this.locked = document.pointerLockElement === this.renderer.domElement;
    this.camera.rotation.set(this.pitch, 0, 0);
    this.rig.rotation.y = this.yaw;

    if (!playing || !this.enabled) {
      this.velocity.multiplyScalar(Math.max(0, 1 - dt * 8));
      this.applyVelocity(dt, false);
      this.bob(dt, 0);
      return;
    }

    const k = this.keys;
    let fwd = 0;
    let strafe = 0;
    if (k.has('KeyW') || k.has('ArrowUp')) fwd += 1;
    if (k.has('KeyS') || k.has('ArrowDown')) fwd -= 1;
    if (k.has('KeyD') || k.has('ArrowRight')) strafe += 1;
    if (k.has('KeyA') || k.has('ArrowLeft')) strafe -= 1;

    this.applyMove(fwd, strafe, dt);
    this.bob(dt, Math.hypot(fwd, strafe));
  }

  updateXR(dt, playing) {
    const session = this.renderer.xr.getSession();
    if (!session) return;

    let fwd = 0;
    let strafe = 0;
    let turn = 0;

    for (const src of session.inputSources) {
      const gp = src.gamepad;
      if (!gp) continue;
      const axes = gp.axes;
      // eixo 3 (Y) / 2 (X) do thumbstick nos controles padrão
      const x = axes[2] ?? 0;
      const y = axes[3] ?? 0;
      const dead = 0.18;
      if (Math.abs(x) > dead || Math.abs(y) > dead) {
        if (src.handedness === 'left') {
          strafe += Math.abs(x) > dead ? x : 0;
          fwd += Math.abs(y) > dead ? -y : 0;
        } else if (Math.abs(x) > 0.7 && this.turnCooldown === 0) {
          // snap turn de 30°
          this.yaw -= Math.sign(x) * (Math.PI / 6);
          this.turnCooldown = 0.32;
        }
      }
    }

    if (playing) this.applyMove(clamp(fwd), clamp(strafe), dt, 2.4);
    this.rig.rotation.y = this.yaw;
    this.bob(dt, Math.hypot(fwd, strafe));
  }

  applyMove(fwd, strafe, dt, speed = 3.6) {
    const dir = new THREE.Vector3();
    // frente no plano XZ, relativa ao yaw
    const forward = new THREE.Vector3(-Math.sin(this.yaw), 0, -Math.cos(this.yaw));
    const right = new THREE.Vector3(Math.cos(this.yaw), 0, -Math.sin(this.yaw));
    dir.addScaledVector(forward, fwd).addScaledVector(right, strafe);

    if (dir.lengthSq() > 1) dir.normalize();
    const target = dir.multiplyScalar(speed);

    this.velocity.x = THREE.MathUtils.damp(this.velocity.x, target.x, 9, dt);
    this.velocity.z = THREE.MathUtils.damp(this.velocity.z, target.z, 9, dt);
    this.applyVelocity(dt, true);
  }

  applyVelocity(dt, allow) {
    if (!allow) {
      this.rig.position.x += this.velocity.x * dt;
      this.rig.position.z += this.velocity.z * dt;
      return;
    }
    this.rig.position.x += this.velocity.x * dt;
    this.rig.position.z += this.velocity.z * dt;
  }

  bob(dt, intensity) {
    if (this.inXR) return;
    this.bobTime += dt * (4 + intensity * 6);
    const amt = Math.min(1, intensity) * 0.018;
    this.camera.position.y = EYE_HEIGHT + Math.sin(this.bobTime) * amt;
    this.camera.position.x = Math.cos(this.bobTime * 0.5) * amt * 0.5;
  }

  resolveCollisions() {
    const p = this.rig.position;
    const halfW = ROOM.width / 2 - RADIUS;
    const halfD = ROOM.depth / 2 - RADIUS;
    const cz = -ROOM.depth / 2 + 6; // centro do piso
    p.x = THREE.MathUtils.clamp(p.x, -halfW, halfW);
    p.z = THREE.MathUtils.clamp(p.z, cz - halfD, cz + halfD);

    // AABBs do cenário
    for (const box of this.colliders) {
      // ignora se o jogador está acima da caixa
      if (p.y > box.max.y) continue;
      const cx = THREE.MathUtils.clamp(p.x, box.min.x, box.max.x);
      const czp = THREE.MathUtils.clamp(p.z, box.min.z, box.max.z);
      const dx = p.x - cx;
      const dz = p.z - czp;
      const d2 = dx * dx + dz * dz;
      if (d2 < RADIUS * RADIUS) {
        if (d2 > 1e-6) {
          const d = Math.sqrt(d2);
          p.x = cx + (dx / d) * RADIUS;
          p.z = czp + (dz / d) * RADIUS;
        } else {
          // centro dentro da caixa: empurra pelo eixo de menor penetração
          const toMinX = p.x - box.min.x;
          const toMaxX = box.max.x - p.x;
          const toMinZ = p.z - box.min.z;
          const toMaxZ = box.max.z - p.z;
          const m = Math.min(toMinX, toMaxX, toMinZ, toMaxZ);
          if (m === toMinX) p.x = box.min.x - RADIUS;
          else if (m === toMaxX) p.x = box.max.x + RADIUS;
          else if (m === toMinZ) p.z = box.min.z - RADIUS;
          else p.z = box.max.z + RADIUS;
        }
      }
    }
  }

  /** Raio de mira: do olho da câmera (desktop) ou do controle direito (XR). */
  getAimRay() {
    const origin = new THREE.Vector3();
    const dir = new THREE.Vector3();

    if (this.inXR) {
      const ctrl = this.weapon.group.parent === this.left ? this.left : this.right;
      ctrl.getWorldPosition(origin);
      dir.set(0, 0, -1).applyQuaternion(ctrl.getWorldQuaternion(new THREE.Quaternion())).normalize();
      // sobe um pouco para compensar o offset da arma na mão
      origin.addScaledVector(dir, 0.16);
    } else {
      this.camera.getWorldPosition(origin);
      this.camera.getWorldDirection(dir);
    }
    return { origin, dir };
  }

  look(origin, dir, objects, far = 200) {
    this.controllerRay.set(origin, dir);
    this.controllerRay.far = far;
    return this.controllerRay.intersectObjects(objects, true);
  }
}

function clamp(v) {
  return Math.max(-1, Math.min(1, v));
}
