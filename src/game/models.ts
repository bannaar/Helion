import * as THREE from "three";

const lineMats = new Map<number, THREE.LineBasicMaterial>();
const fillMats = new Map<number, THREE.MeshBasicMaterial>();

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

export function fillMat(color: number, opacity = 0.07): THREE.MeshBasicMaterial {
  const key = color * 10 + opacity;
  let m = fillMats.get(key);
  if (!m) {
    m = new THREE.MeshBasicMaterial({
      color,
      transparent: true,
      opacity,
      side: THREE.DoubleSide,
      depthWrite: false,
    });
    fillMats.set(key, m);
  }
  return m;
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
  g.userData.radius = 3.2;
  return g;
}

export function makeSidewinder(color: number): THREE.Group {
  const geo = new THREE.ConeGeometry(1.6, 3.6, 4);
  geo.rotateX(-Math.PI / 2);
  geo.translate(0, 0, -0.2);
  const g = edged(geo, color, 0.08);
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
  g.userData.radius = 2.8;
  return g;
}

export function makeShip(kind: "cobra" | "sidewinder" | "asp" | "viper", color: number): THREE.Group {
  if (kind === "sidewinder") return makeSidewinder(color);
  if (kind === "asp") return makeAsp(color);
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
