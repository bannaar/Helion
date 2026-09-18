import * as THREE from "three";

const lineMats = new Map<number, THREE.LineBasicMaterial>();
const fillMats = new Map<number, THREE.MeshLambertMaterial>();
const detailMats = new Map<number, THREE.MeshBasicMaterial>();

export function lineMat(color: number, opacity = 1): THREE.LineBasicMaterial {
  const key = color * 10 + opacity;
  let m = lineMats.get(key);
  if (!m) {
    m = new THREE.LineBasicMaterial({
      color,
      transparent: opacity < 1,
      opacity,
      depthWrite: opacity >= 1,
    });
    lineMats.set(key, m);
  }
  return m;
}

export function fillMat(color: number, opacity = 0.78): THREE.MeshLambertMaterial {
  const solidOpacity = Math.max(opacity, 0.42);
  const key = color * 10 + solidOpacity;
  let m = fillMats.get(key);
  if (!m) {
    m = new THREE.MeshLambertMaterial({
      color,
      transparent: solidOpacity < 1,
      opacity: solidOpacity,
      side: THREE.DoubleSide,
      depthWrite: true,
    });
    fillMats.set(key, m);
  }

  return m;
}

function detailMat(color: number, opacity = 0.9): THREE.MeshBasicMaterial {
  const key = color * 10 + opacity;
  let material = detailMats.get(key);
  if (!material) {
    material = new THREE.MeshBasicMaterial({ color, transparent: opacity < 1, opacity });
    detailMats.set(key, material);
  }
  return material;
}

function detailBox(g: THREE.Group, color: number, size: [number, number, number], position: [number, number, number], rotation?: [number, number, number]) {
  const mesh = new THREE.Mesh(new THREE.BoxGeometry(...size), detailMat(color));
  mesh.position.set(...position);
  if (rotation) mesh.rotation.set(...rotation);
  g.add(mesh);
}

function enginePod(g: THREE.Group, color: number, position: [number, number, number], scale = 1) {
  const pod = new THREE.CylinderGeometry(0.28 * scale, 0.42 * scale, 1.8 * scale, 6);
  pod.rotateX(Math.PI / 2);
  const mesh = new THREE.Mesh(pod, detailMat(color, 0.8));
  mesh.position.set(...position);
  g.add(mesh);
  const glow = new THREE.Mesh(new THREE.CircleGeometry(0.22 * scale, 8), detailMat(0x5ee7ff));
  glow.position.set(position[0], position[1], position[2] + 0.92 * scale);
  glow.rotation.y = Math.PI;
  g.add(glow);
}

function edged(geo: THREE.BufferGeometry, color: number, fill = 0.06): THREE.Group {
  const g = new THREE.Group();
  g.add(new THREE.Mesh(geo, fillMat(color, fill)));
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(geo, 12), lineMat(color)));
  return g;
}

export function makeCobra(color: number): THREE.Group {
  const geo = new THREE.BufferGeometry();
  const v = new Float32Array([
    0, 0, -3.8, 2.8, 0, 1.0, 0.85, 0.72, 0.4, -0.85, 0.72, 0.4, -2.8, 0, 1.0, 1.5, 0, 2.6, -1.5,
    0, 2.6, 0, -0.5, 0.6, 0.4, 0.15, 2.2, -0.4, 0.15, 2.2,
  ]);
  const idx = [
    0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 1, 7, 0, 4, 7, 1, 5, 7, 4, 6, 7, 2, 3, 8, 3, 9, 8, 1, 2, 5, 4, 6,
    3, 5, 8, 9, 5, 9, 6, 5, 6, 7,
  ];
  geo.setAttribute("position", new THREE.BufferAttribute(v, 3));
  geo.setIndex(idx);
  geo.computeVertexNormals();
  const g = edged(geo, color, 0.08);
  detailBox(g, 0x8bd9e8, [1.2, 0.12, 1.1], [0, 0.72, -0.5], [-0.28, 0, 0]);
  detailBox(g, 0x344b63, [0.42, 0.3, 1.8], [-1.35, -0.1, 1.25], [0, 0, -0.12]);
  detailBox(g, 0x344b63, [0.42, 0.3, 1.8], [1.35, -0.1, 1.25], [0, 0, 0.12]);
  enginePod(g, color, [-1.45, -0.1, 2.9], 1.15);
  enginePod(g, color, [1.45, -0.1, 2.9], 1.15);
  g.userData.radius = 3.2;
  return g;
}

export function makeSidewinder(color: number): THREE.Group {
  const geo = new THREE.ConeGeometry(1.6, 3.6, 4);
  geo.rotateX(-Math.PI / 2);
  geo.translate(0, 0, -0.2);
  const g = edged(geo, color, 0.08);
  detailBox(g, 0xb7f3ff, [0.8, 0.12, 0.95], [0, 0.45, -0.65], [-0.35, 0, 0]);
  detailBox(g, 0x29384d, [0.28, 0.18, 1.4], [-1.35, -0.2, 0.55], [0, 0, -0.2]);
  detailBox(g, 0x29384d, [0.28, 0.18, 1.4], [1.35, -0.2, 0.55], [0, 0, 0.2]);
  enginePod(g, color, [-1.1, 0, 1.7]);
  enginePod(g, color, [1.1, 0, 1.7]);
  const fin = new THREE.BoxGeometry(3.4, 0.08, 1.1);
  g.add(new THREE.Mesh(fin, fillMat(color, 0.08)));
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(fin), lineMat(color)));
  g.userData.radius = 2.4;
  return g;
}

export function makeAsp(color: number): THREE.Group {
  const geo = new THREE.OctahedronGeometry(2.1, 0);
  geo.scale(1.15, 0.45, 2.1);
  const g = edged(geo, color, 0.07);
  detailBox(g, 0xb7f3ff, [1.1, 0.18, 1.25], [0, 0.55, -1.1], [-0.2, 0, 0]);
  detailBox(g, 0x384d60, [0.25, 0.38, 2.7], [-1.25, 0, 0.45], [0, 0, -0.08]);
  detailBox(g, 0x384d60, [0.25, 0.38, 2.7], [1.25, 0, 0.45], [0, 0, 0.08]);
  enginePod(g, color, [-1.05, -0.15, 2.2]);
  enginePod(g, color, [1.05, -0.15, 2.2]);
  g.userData.radius = 2.8;
  return g;
}

export function makeViper(color: number): THREE.Group {
  const body = new THREE.ConeGeometry(1.45, 4.8, 6);
  body.rotateX(-Math.PI / 2);
  const g = edged(body, color, 0.08);
  detailBox(g, 0xa8efff, [0.9, 0.12, 1.1], [0, 0.55, -1.05], [-0.32, 0, 0]);
  detailBox(g, 0x273b50, [0.5, 0.22, 1.8], [-1.55, -0.12, 1.1], [0, 0, -0.18]);
  detailBox(g, 0x273b50, [0.5, 0.22, 1.8], [1.55, -0.12, 1.1], [0, 0, 0.18]);
  enginePod(g, color, [-1.25, -0.05, 2.25]);
  enginePod(g, color, [1.25, -0.05, 2.25]);
  const wing = new THREE.BoxGeometry(4.5, 0.12, 1.2);
  wing.rotateY(-0.12);
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(wing), lineMat(color)));
  g.userData.radius = 2.6;
  return g;
}

export function makeAdder(color: number): THREE.Group {
  const body = new THREE.BoxGeometry(3.4, 1.5, 4.2);
  const g = edged(body, color, 0.08);
  detailBox(g, 0xb7f3ff, [1.2, 0.16, 1.25], [0, 0.78, -1.25], [-0.2, 0, 0]);
  detailBox(g, 0x334860, [0.32, 0.5, 2.1], [-1.7, 0, 0.8], [0, 0, -0.08]);
  detailBox(g, 0x334860, [0.32, 0.5, 2.1], [1.7, 0, 0.8], [0, 0, 0.08]);
  enginePod(g, color, [-1.25, -0.25, 2.4]);
  enginePod(g, color, [1.25, -0.25, 2.4]);
  const nose = new THREE.ConeGeometry(1.7, 2.8, 4);
  nose.rotateX(-Math.PI / 2);
  nose.translate(0, 0, -3.2);
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(nose), lineMat(color)));
  g.userData.radius = 3.4;
  return g;
}

export function makeHauler(color: number): THREE.Group {
  const body = new THREE.BoxGeometry(4.3, 2.2, 6.4);
  const g = edged(body, color, 0.07);
  detailBox(g, 0xc6f4ff, [1.8, 0.18, 1.25], [0, 1.15, -2.15], [-0.14, 0, 0]);
  detailBox(g, 0x27394d, [0.32, 1.2, 2.6], [-1.8, 0, 0.65], [0, 0, -0.04]);
  detailBox(g, 0x27394d, [0.32, 1.2, 2.6], [1.8, 0, 0.65], [0, 0, 0.04]);
  enginePod(g, color, [-1.35, -0.45, 3.35], 1.25);
  enginePod(g, color, [1.35, -0.45, 3.35], 1.25);
  const cockpit = new THREE.BoxGeometry(3.1, 1.2, 1.5);
  cockpit.translate(0, 0.5, -3.7);
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(cockpit), lineMat(color)));
  g.userData.radius = 4.3;
  return g;
}

export function makeDrone(color: number): THREE.Group {
  const core = new THREE.OctahedronGeometry(1.8, 0);
  const g = edged(core, color, 0.09);
  const ring = new THREE.TorusGeometry(2.6, 0.12, 4, 8);
  ring.rotateX(Math.PI / 2);
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(ring), lineMat(color)));
  for (let i = 0; i < 4; i += 1) {
    const strut = new THREE.Mesh(new THREE.BoxGeometry(0.16, 0.16, 2.4), detailMat(color));
    strut.rotation.z = (i / 4) * Math.PI * 2;
    strut.position.set(Math.cos((i / 4) * Math.PI * 2) * 1.15, Math.sin((i / 4) * Math.PI * 2) * 1.15, 0);
    g.add(strut);
  }
  g.userData.radius = 2.8;
  return g;
}

export function makeThargoid(color: number): THREE.Group {
  const core = new THREE.IcosahedronGeometry(2.9, 1);
  const g = edged(core, color, 0.1);
  for (let i = 0; i < 6; i += 1) {
    const arm = new THREE.ConeGeometry(0.34, 2.2, 5);
    arm.rotateZ(Math.PI / 2);
    arm.rotateY((i / 6) * Math.PI * 2);
    arm.translate(Math.cos((i / 6) * Math.PI * 2) * 2.6, Math.sin((i / 6) * Math.PI * 2) * 2.6, 0);
    g.add(new THREE.LineSegments(new THREE.EdgesGeometry(arm), lineMat(color)));
    const eye = new THREE.Mesh(new THREE.OctahedronGeometry(0.22, 0), detailMat(0x8affc1));
    eye.position.set(Math.cos((i / 6) * Math.PI * 2) * 3.45, Math.sin((i / 6) * Math.PI * 2) * 3.45, 0);
    g.add(eye);
  }
  g.userData.radius = 4;
  return g;
}

export function makeShip(
  kind: "cobra" | "sidewinder" | "asp" | "viper" | "adder" | "hauler" | "eagle" | "courier" | "marauder" | "unionMiner" | "drone",
  color: number,
): THREE.Group {
  if (kind === "sidewinder") return makeSidewinder(color);
  if (kind === "asp") return makeAsp(color);
  if (kind === "viper") return makeViper(color);
  if (kind === "adder") return makeAdder(color);
  if (kind === "hauler") return makeHauler(color);
  if (kind === "drone") return makeDrone(color);
  if (kind === "eagle" || kind === "courier") return makeViper(color);
  if (kind === "marauder" || kind === "unionMiner") return makeHauler(color);
  return makeCobra(color);
}

export function makeStation(color: number): THREE.Group {
  const g = new THREE.Group();
  const body = new THREE.OctahedronGeometry(16, 0);
  g.add(new THREE.Mesh(body, fillMat(color, 0.05)));
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(body), lineMat(color)));
  const ring = new THREE.TorusGeometry(17.5, 0.28, 5, 20);
  ring.rotateX(Math.PI / 2);
  g.add(new THREE.Mesh(ring, fillMat(color, 0.1)));
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(ring, 1), lineMat(color)));
  const slot = new THREE.BoxGeometry(6.5, 2.2, 8);
  const slotMesh = new THREE.LineSegments(new THREE.EdgesGeometry(slot), lineMat(color, 0.85));
  slotMesh.position.z = 12;
  g.add(slotMesh);
  for (let i = 0; i < 8; i += 1) {
    const module = new THREE.Mesh(new THREE.BoxGeometry(1.6, 0.5, 2.4), fillMat(0x24384b, 0.95));
    const angle = (i / 8) * Math.PI * 2;
    module.position.set(Math.cos(angle) * 12.8, Math.sin(angle) * 12.8, Math.sin(angle * 2) * 3);
    module.rotation.z = angle;
    g.add(module);
    const beacon = new THREE.Mesh(new THREE.BoxGeometry(0.18, 0.18, 1.1), detailMat(color));
    beacon.position.copy(module.position).multiplyScalar(1.08);
    g.add(beacon);
  }
  g.userData.radius = 18;
  return g;
}

export function makePlanet(color: number, radius: number): THREE.Group {
  const geo = new THREE.IcosahedronGeometry(radius, 1);
  return edged(geo, color, 0.045);
}

export function makeStar(color: number): THREE.Group {
  const g = new THREE.Group();
  const core = new THREE.IcosahedronGeometry(28, 1);
  g.add(new THREE.Mesh(core, new THREE.MeshBasicMaterial({ color })));
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(core), lineMat(0xffffff, 0.35)));
  const halo = new THREE.IcosahedronGeometry(42, 1);
  g.add(
    new THREE.Mesh(
      halo,
      new THREE.MeshBasicMaterial({
        color,
        transparent: true,
        opacity: 0.12,
        depthWrite: false,
      }),
    ),
  );
  g.userData.radius = 28;
  return g;
}

export function makeAsteroid(color: number, r: number): THREE.Group {
  const geo = new THREE.IcosahedronGeometry(r, 0);
  geo.scale(1, 0.7 + Math.random() * 0.5, 0.8 + Math.random() * 0.4);
  return edged(geo, color, 0.05);
}

export function makeCanister(color: number): THREE.Group {
  const geo = new THREE.CylinderGeometry(0.6, 0.6, 1.6, 6);
  geo.rotateZ(Math.PI / 2);
  return edged(geo, color, 0.14);
}

export function makeStarfield(n = 700): THREE.Points {
  const pos = new Float32Array(n * 3);
  for (let i = 0; i < n; i += 1) {
    const r = 420 + Math.random() * 1800;
    const theta = Math.random() * Math.PI * 2;
    const phi = Math.acos(2 * Math.random() - 1);
    pos[i * 3] = r * Math.sin(phi) * Math.cos(theta);
    pos[i * 3 + 1] = r * Math.sin(phi) * Math.sin(theta);
    pos[i * 3 + 2] = r * Math.cos(phi);
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute("position", new THREE.BufferAttribute(pos, 3));
  const mat = new THREE.PointsMaterial({
    color: 0xcfe4ea,
    size: 1.4,
    sizeAttenuation: false,
    depthWrite: false,
  });
  return new THREE.Points(geo, mat);
}

export function makeBeam(color: number): THREE.Line {
  const geo = new THREE.BufferGeometry();
  geo.setAttribute("position", new THREE.BufferAttribute(new Float32Array(6), 3));
  return new THREE.Line(geo, lineMat(color));
}

export function disposeSharedMaterials(): void {
  for (const m of lineMats.values()) m.dispose();
  for (const m of fillMats.values()) m.dispose();
  lineMats.clear();
  fillMats.clear();
}
