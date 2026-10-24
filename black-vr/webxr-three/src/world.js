/**
 * Cenário: estande de tiro industrial subterrâneo ("Setor 7").
 * Tudo construído com geometria procedural + texturas em canvas.
 */
import * as THREE from 'three';
import { concrete, metal, hazard, floorTiles, label } from './textures.js';

export const ROOM = {
  width: 28,
  depth: 32,
  height: 8,
  // jogador começa aqui (o piso vai de z=-26 a z=6)
  spawn: new THREE.Vector3(0, 0, 2.5),
};

export function buildWorld(scene) {
  const group = new THREE.Group();
  group.name = 'world';
  scene.add(group);

  /** malhas que recebem tiro (impacto) */
  const shootable = [];
  /** caixas de colisão (AABB) para o jogador não atravessar paredes */
  const colliders = [];

  const matConcrete = new THREE.MeshStandardMaterial({
    map: concrete(3),
    color: 0x9aa0a6,
    roughness: 0.94,
    metalness: 0.04,
  });
  const matFloor = new THREE.MeshStandardMaterial({
    map: floorTiles(6),
    color: 0x9ea4a8,
    roughness: 0.82,
    metalness: 0.08,
  });
  const matMetal = new THREE.MeshStandardMaterial({
    map: metal(2),
    color: 0x8f979e,
    roughness: 0.55,
    metalness: 0.65,
  });
  const matHazard = new THREE.MeshStandardMaterial({ map: hazard(6), roughness: 0.7, metalness: 0.25 });
  const matDark = new THREE.MeshStandardMaterial({ color: 0x0d1014, roughness: 0.9, metalness: 0.1 });
  const matBeam = new THREE.MeshStandardMaterial({ color: 0x151a20, roughness: 0.75, metalness: 0.4 });

  // ---------------------------------------------------------------- piso
  const floor = new THREE.Mesh(new THREE.PlaneGeometry(ROOM.width, ROOM.depth), matFloor);
  floor.rotation.x = -Math.PI / 2;
  floor.position.z = -ROOM.depth / 2 + 6;
  floor.receiveShadow = true;
  group.add(floor);
  shootable.push(floor);
  addCollider(colliders, floor);

  // ---------------------------------------------------------------- teto
  const ceiling = new THREE.Mesh(new THREE.PlaneGeometry(ROOM.width, ROOM.depth), matDark);
  ceiling.rotation.x = Math.PI / 2;
  ceiling.position.set(0, ROOM.height, floor.position.z);
  group.add(ceiling);
  shootable.push(ceiling);

  // ---------------------------------------------------------------- paredes
  const wallZ = floor.position.z;
  const mkWall = (w, h, pos, rotY = 0, mat = matConcrete) => {
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), mat);
    m.position.copy(pos);
    m.rotation.y = rotY;
    m.receiveShadow = true;
    group.add(m);
    shootable.push(m);
    addCollider(colliders, m);
    return m;
  };

  // fundo (onde ficam os alvos)
  const back = mkWall(ROOM.width, ROOM.height, new THREE.Vector3(0, ROOM.height / 2, wallZ - ROOM.depth / 2));
  // frente (atrás do jogador)
  mkWall(ROOM.width, ROOM.height, new THREE.Vector3(0, ROOM.height / 2, wallZ + ROOM.depth / 2), Math.PI);
  // laterais
  mkWall(ROOM.depth, ROOM.height, new THREE.Vector3(-ROOM.width / 2, ROOM.height / 2, wallZ), Math.PI / 2);
  mkWall(ROOM.depth, ROOM.height, new THREE.Vector3(ROOM.width / 2, ROOM.height / 2, wallZ), -Math.PI / 2);

  // ---------------------------------------------------------------- pilares
  const pillarGeo = new THREE.BoxGeometry(1.1, ROOM.height, 1.1);
  [-9.5, -4.75, 0, 4.75, 9.5].forEach((x) => {
    [-2, -10].forEach((z) => {
      const p = new THREE.Mesh(pillarGeo, matConcrete);
      p.position.set(x, ROOM.height / 2, z);
      p.castShadow = true;
      p.receiveShadow = true;
      group.add(p);
      shootable.push(p);
      addCollider(colliders, p);

      const band = new THREE.Mesh(new THREE.BoxGeometry(1.24, 0.5, 1.24), matHazard);
      band.position.set(x, 1.1, z);
      group.add(band);
    });
  });

  // ---------------------------------------------------------------- vigas do teto
  const beamGeo = new THREE.BoxGeometry(ROOM.width, 0.42, 0.55);
  for (let z = 6; z > -22; z -= 4) {
    const beam = new THREE.Mesh(beamGeo, matBeam);
    beam.position.set(0, ROOM.height - 0.3, z);
    group.add(beam);
  }

  // ---------------------------------------------------------------- passarela de serviço
  const catwalk = new THREE.Mesh(new THREE.BoxGeometry(ROOM.width - 2, 0.2, 2.4), matMetal);
  catwalk.position.set(0, 5.1, -13.4);
  group.add(catwalk);
  shootable.push(catwalk);

  const railingMat = new THREE.MeshStandardMaterial({ color: 0x3b444c, roughness: 0.6, metalness: 0.7 });
  for (let x = -13; x <= 13; x += 1.6) {
    const post = new THREE.Mesh(new THREE.BoxGeometry(0.07, 1.0, 0.07), railingMat);
    post.position.set(x, 5.7, -12.25);
    group.add(post);
  }
  const topRail = new THREE.Mesh(new THREE.BoxGeometry(ROOM.width - 2, 0.07, 0.07), railingMat);
  topRail.position.set(0, 6.2, -12.25);
  group.add(topRail);

  // escada de serviço (visual)
  const stepMat = matMetal;
  for (let i = 0; i < 12; i++) {
    const step = new THREE.Mesh(new THREE.BoxGeometry(1.4, 0.1, 0.4), stepMat);
    step.position.set(12.2, 0.42 + i * 0.42, -9 + i * 0.42);
    group.add(step);
    shootable.push(step);
  }

  // ---------------------------------------------------------------- barricadas / caixas
  const crateGeo = new THREE.BoxGeometry(1, 1, 1);
  const crateSpots = [
    [-6.4, 0.5, 2.2, 0.5],
    [-7.1, 1.4, 1.6, 0.9],
    [6.9, 0.5, 3.0, -0.4],
    [7.7, 1.3, 2.2, 0.2],
    [-11.2, 0.5, -3.4, 0.8],
    [11.0, 0.5, -4.2, -0.6],
  ];
  crateSpots.forEach(([x, y, z, rot], i) => {
    const s = 0.85 + (i % 3) * 0.12;
    const crate = new THREE.Mesh(crateGeo, i % 2 ? matMetal : matBeam);
    crate.position.set(x, y, z);
    crate.rotation.y = rot;
    crate.scale.set(s, s, s);
    crate.castShadow = true;
    crate.receiveShadow = true;
    group.add(crate);
    shootable.push(crate);
    addCollider(colliders, crate);
  });

  // ---------------------------------------------------------------- barreira de tiro (linha do jogador)
  const barrier = new THREE.Mesh(new THREE.BoxGeometry(9, 1.1, 0.5), matConcrete);
  barrier.position.set(0, 0.55, -1.2);
  barrier.receiveShadow = true;
  group.add(barrier);
  shootable.push(barrier);
  addCollider(colliders, barrier);

  const barrierTop = new THREE.Mesh(new THREE.BoxGeometry(9.1, 0.12, 0.62), matHazard);
  barrierTop.position.set(0, 1.16, -1.2);
  group.add(barrierTop);

  // ---------------------------------------------------------------- sinalização
  const sign = new THREE.Mesh(
    new THREE.PlaneGeometry(8, 2),
    new THREE.MeshBasicMaterial({
      map: label([line('SETOR 7 // ESTANDE 03'), '*PROTOCOLO ZERO'], {
        width: 1024,
        height: 256,
        bg: '#080b0f',
        accent: '#ff6a13',
        size: 78,
      }),
      toneMapped: false,
    })
  );
  sign.position.set(0, 4.4, wallZ - ROOM.depth / 2 + 0.06);
  group.add(sign);

  const sideSign = new THREE.Mesh(
    new THREE.PlaneGeometry(4.5, 2.2),
    new THREE.MeshBasicMaterial({
      map: label(['*ALVOS', 'ATIVOS'], {
        width: 512,
        height: 256,
        bg: '#0a0d11',
        accent: '#ff6a13',
        size: 92,
      }),
      transparent: true,
      toneMapped: false,
    })
  );
  sideSign.position.set(-ROOM.width / 2 + 0.06, 3.2, wallZ - 8);
  sideSign.rotation.y = Math.PI / 2;
  group.add(sideSign);

  // faixas pintadas no chão
  for (let i = 0; i < 6; i++) {
    const stripe = new THREE.Mesh(new THREE.PlaneGeometry(0.35, 20), matHazard);
    stripe.rotation.x = -Math.PI / 2;
    stripe.position.set(-11 + i * 4.4, 0.012, wallZ - 10);
    group.add(stripe);
  }

  // ---------------------------------------------------------------- iluminação
  scene.add(new THREE.HemisphereLight(0x5c6b7a, 0x0a0c10, 0.55));

  const key = new THREE.DirectionalLight(0xfff0dd, 1.05);
  key.position.set(6, 12, 8);
  key.castShadow = false;
  scene.add(key);

  const fill = new THREE.DirectionalLight(0x3f6f9f, 0.7);
  fill.position.set(-8, 6, -14);
  scene.add(fill);

  // lâmpadas suspensas
  const lampMat = new THREE.MeshBasicMaterial({ color: 0xffd9a0, toneMapped: false });
  const lampSpots = [
    [-8, -4],
    [0, -4],
    [8, -4],
    [-8, -12],
    [0, -12],
    [8, -12],
  ];
  lampSpots.forEach(([x, z]) => {
    const shade = new THREE.Mesh(new THREE.ConeGeometry(0.55, 0.42, 12, 1, true), matMetal);
    shade.position.set(x, ROOM.height - 1.05, z);
    shade.rotation.x = Math.PI;
    group.add(shade);

    const bulb = new THREE.Mesh(new THREE.CircleGeometry(0.45, 12), lampMat);
    bulb.position.set(x, ROOM.height - 1.27, z);
    bulb.rotation.x = Math.PI / 2;
    group.add(bulb);

    const lamp = new THREE.PointLight(0xffc98a, 22, 16, 2);
    lamp.position.set(x, ROOM.height - 1.5, z);
    scene.add(lamp);

    const cable = new THREE.Mesh(new THREE.CylinderGeometry(0.02, 0.02, 1.05, 4), matDark);
    cable.position.set(x, ROOM.height - 0.5, z);
    group.add(cable);
  });

  // luz vermelha de emergência no fundo (entra no ritmo da cena)
  const emergency = new THREE.PointLight(0xff2a10, 14, 22, 2);
  emergency.position.set(0, 6.2, wallZ - ROOM.depth / 2 + 1.5);
  scene.add(emergency);

  // neblina volumétrica barata
  scene.fog = new THREE.FogExp2(0x05070a, 0.021);

  // ---------------------------------------------------------------- partículas de poeira
  const dustGeo = new THREE.BufferGeometry();
  const dustCount = 420;
  const positions = new Float32Array(dustCount * 3);
  for (let i = 0; i < dustCount; i++) {
    positions[i * 3 + 0] = (Math.random() - 0.5) * ROOM.width;
    positions[i * 3 + 1] = Math.random() * ROOM.height;
    positions[i * 3 + 2] = wallZ - Math.random() * ROOM.depth;
  }
  dustGeo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
  const dust = new THREE.Points(
    dustGeo,
    new THREE.PointsMaterial({
      color: 0xd8cbb6,
      size: 0.035,
      transparent: true,
      opacity: 0.5,
      depthWrite: false,
    })
  );
  group.add(dust);

  return {
    group,
    shootable,
    colliders,
    backWall: back,
    dust,
    emergency,
    update(dt, t) {
      // deriva lenta da poeira
      const pos = dust.geometry.attributes.position;
      for (let i = 0; i < pos.count; i++) {
        let y = pos.getY(i) + dt * 0.05;
        if (y > ROOM.height) y = 0.1;
        pos.setY(i, y);
      }
      pos.needsUpdate = true;

      // pulso da luz de emergência
      emergency.intensity = 10 + Math.sin(t * 2.2) * 4;
    },
  };
}

function line(text) {
  return text;
}

function addCollider(list, mesh) {
  mesh.geometry.computeBoundingBox();
  const box = new THREE.Box3().setFromObject(mesh);
  // superfícies finas (planos) viram caixas com espessura para bloquear o jogador
  const min = box.min.clone();
  const max = box.max.clone();
  const eps = 0.25;
  if (max.x - min.x < eps) {
    min.x -= eps;
    max.x += eps;
  }
  if (max.z - min.z < eps) {
    min.z -= eps;
    max.z += eps;
  }
  list.push(new THREE.Box3(min, max));
}
