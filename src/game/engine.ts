import * as THREE from "three";
import { setEngineLevel, sfxPlay } from "./audio";
import { getSystem, jumpFuelCost, systemDistance, systemFaction } from "./galaxy";
import { Input } from "./input";
import {
  makeAsp,
  makeAdder,
  makeAsteroid,
  makeCanister,
  makeCobra,
  makeDrone,
  makeHauler,
  makeThargoid,
  makePlanet,
  makeSidewinder,
  makeViper,
  makeStar,
  makeStarfield,
  makeStation,
  disposeSharedMaterials,
} from "./models";
import { cargoCapacity, cargoUsed, fittedWeapon, hasModule, moduleCount, moduleScanStrength, shipStats } from "./ships";
import { useGameStore } from "./store";
import type { CommodityId, Contact, GameMode, ShipId } from "./types";
import { logTrafficFn } from "./universe.functions";

const FIXED = 1 / 60;
const PLAYER_COLOR = 0x8fd4c8;
const PIRATE_COLOR = 0xd07a6a;
const POLICE_COLOR = 0x8aa4d0;
const STATION_COLOR = 0xc5d0d4;
const AST_COLOR = 0x8a8478;
const CAN_COLOR = 0xd4c4a0;

const _fwd = new THREE.Vector3();
const _up = new THREE.Vector3();
const _desired = new THREE.Vector3();
const _look = new THREE.Vector3();
const _right = new THREE.Vector3();
const _tmp = new THREE.Vector3();

type Npc = {
  id: string;
  mesh: THREE.Group;
  pos: THREE.Vector3;
  yaw: number;
  pitch: number;
  speed: number;
  hull: number;
  fireCd: number;
  police: boolean;
  kind: "pirate" | "police" | "trader" | "thargoid";
};

type Shot = {
  pos: THREE.Vector3;
  vel: THREE.Vector3;
  life: number;
  friendly: boolean;
  power: number;
  explosive: boolean;
  mesh: THREE.Mesh;
};

type Loot = {
  mesh: THREE.Group;
  pos: THREE.Vector3;
  life: number;
  good: CommodityId;
};

type AsteroidField = {
  mesh: THREE.Group;
  spin: THREE.Vector3;
  ore: CommodityId;
  remaining: number;
};

function shipMesh(id: ShipId, color: number): THREE.Group {
  if (id === "sidewinder") return makeSidewinder(color);
  if (id === "asp") return makeAsp(color);
  if (id === "viper") return makeViper(color);
  if (id === "adder") return makeAdder(color);
  if (id === "hauler") return makeHauler(color);
  if (id === "drone") return makeDrone(color);
  if (id === "eagle" || id === "courier") return makeViper(color);
  if (id === "marauder" || id === "unionMiner") return makeHauler(color);
  return makeCobra(color);
}

export class HelionEngine {
  private canvas: HTMLCanvasElement;
  private renderer: THREE.WebGLRenderer;
  private scene = new THREE.Scene();
  private camera = new THREE.PerspectiveCamera(62, 1, 0.4, 6000);
  readonly input = new Input();
  private running = false;
  private acc = 0;
  private last = 0;
  private hudAcc = 0;
  private mode: GameMode = "title";
  private frozen = true;

  yaw = 0;
  pitch = 0;
  roll = 0;
  speed = 0;
  throttle = 0;
  private inertiaDampeners = true;
  pos = new THREE.Vector3();

  private player = new THREE.Group();
  private starfield: THREE.Points;
  private titleRoot = new THREE.Group();
  private spaceRoot = new THREE.Group();
  private station = new THREE.Group();
  private planet = new THREE.Group();
  private star = new THREE.Group();
  private stationPos = new THREE.Vector3(420, 18, 160);
  private planetPos = new THREE.Vector3(1680, -140, 980);
  private planetR = 190;
  private npcs: Npc[] = [];
  private shots: Shot[] = [];
  private loot: Loot[] = [];
  private asteroids: AsteroidField[] = [];
  private shotGeo = new THREE.BoxGeometry(0.18, 0.18, 1.8);
  private shotMatP = new THREE.MeshBasicMaterial({ color: PLAYER_COLOR });
  private shotMatE = new THREE.MeshBasicMaterial({ color: PIRATE_COLOR });
  private fireCd = 0;
  private bombCd = 0;
  private chaffCd = 0;
  private chaffTimer = 0;
  private miningCd = 0;
  private scanCd = 0;
  private salvageCd = 0;
  private laserHeat = 0;
  private hitCd = 0;
  private shake = 0;
  private titleT = 0;
  private jumpCharge = 0;
  private charging = false;
  private targetId: string | null = null;
  private injectSteer: number | null = null;
  private ro: ResizeObserver | null = null;
  private shieldRegen = 0;

  constructor(canvas: HTMLCanvasElement) {
    this.canvas = canvas;
    this.renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: false,
      powerPreference: "low-power",
      alpha: false,
    });
    this.renderer.setClearColor(0x07090c, 1);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 1.25));
    this.scene.fog = new THREE.FogExp2(0x07090c, 0.00055);
    this.scene.add(new THREE.AmbientLight(0x8fa8c4, 1.8));
    const keyLight = new THREE.DirectionalLight(0xd7e8ff, 2.4);
    keyLight.position.set(300, 500, 220);
    this.scene.add(keyLight);
    this.starfield = makeStarfield(640);
    this.scene.add(this.starfield);
    this.scene.add(this.titleRoot);
    this.scene.add(this.spaceRoot);
    this.spaceRoot.visible = false;
    this.buildTitle();
    this.buildSpaceScaffold();
    this.input.attach();
    this.resize();
    this.ro = new ResizeObserver(() => this.resize());
    this.ro.observe(canvas.parentElement ?? canvas);
    this.wireProbe();
  }

  private wireProbe() {
    const qa =
      import.meta.env.DEV ||
      (typeof window !== "undefined" && /(?:\?|&)qa=1\b/.test(window.location.search));
    if (!qa && typeof window !== "undefined") {
      // Always expose in this product so in-preview QA can prove A/D.
    }
    window.__controlsTest = {
      getYaw: () => this.yaw,
      getSpeed: () => this.speed,
      setKeys: (codes) => {
        this.input.setKeys(codes);
        if (codes.length === 0) this.injectSteer = null;
      },
      setSteer: (v) => {
        this.injectSteer = v;
      },
    };
  }

  private buildTitle() {
    const cobra = makeCobra(PLAYER_COLOR);
    cobra.position.set(0, 0, 0);
    const st = makeStation(STATION_COLOR);
    st.position.set(28, -6, -36);
    st.scale.setScalar(0.45);
    const pl = makePlanet(0x6a8a9a, 22);
    pl.position.set(-40, -8, -70);
    this.titleRoot.add(cobra, st, pl);
    this.titleRoot.userData.cobra = cobra;
    this.titleRoot.userData.station = st;
  }

  private buildSpaceScaffold() {
    this.station = makeStation(STATION_COLOR);
    this.station.position.copy(this.stationPos);
    this.planet = makePlanet(0x6a8a9a, this.planetR);
    this.planet.position.copy(this.planetPos);
    this.star = makeStar(0xf2e6c4);
    this.player = new THREE.Group();
    this.spaceRoot.add(this.star, this.planet, this.station, this.player);
  }

  private rebuildPlayerMesh() {
    while (this.player.children.length) {
      const ch = this.player.children[0]!;
      this.player.remove(ch);
    }
    const id = useGameStore.getState().save.shipId;
    this.player.add(shipMesh(id, PLAYER_COLOR));
  }

  private clearNpcs() {
    for (const n of this.npcs) this.spaceRoot.remove(n.mesh);
    this.npcs = [];
    for (const s of this.shots) this.spaceRoot.remove(s.mesh);
    this.shots = [];
    for (const l of this.loot) this.spaceRoot.remove(l.mesh);
    this.loot = [];
    for (const a of this.asteroids) this.spaceRoot.remove(a.mesh);
    this.asteroids = [];
  }

  enterSystem(systemId: string, kind: "spawn" | "undock" | "jump") {
    const sys = getSystem(systemId);
    if (!sys) return;
    this.clearNpcs();
    this.spaceRoot.remove(this.planet);
    this.spaceRoot.remove(this.star);
    this.planet = makePlanet(sys.planetColor, this.planetR);
    this.planet.position.copy(this.planetPos);
    this.star = makeStar(sys.starColor);
    this.spaceRoot.add(this.star, this.planet);
    this.rebuildPlayerMesh();

    if (kind === "undock") {
      this.pos.copy(this.stationPos).add(_tmp.set(70, 12, 32));
      this.yaw = Math.atan2(
        -(this.stationPos.x - this.pos.x),
        -(this.stationPos.z - this.pos.z),
      );
      this.pitch = 0;
      this.roll = 0;
      this.speed = 10;
      this.throttle = 0.16;
    } else if (kind === "jump") {
      this.pos.set(980, 60, -280);
      this.yaw = Math.atan2(
        -(this.stationPos.x - this.pos.x),
        -(this.stationPos.z - this.pos.z),
      );
      this.pitch = -0.04;
      this.speed = 22;
      this.throttle = 0.3;
    } else {
      this.pos.copy(this.stationPos).add(_tmp.set(44, 8, 16));
      this.yaw = Math.atan2(
        -(this.stationPos.x - this.pos.x),
        -(this.stationPos.z - this.pos.z),
      );
      this.pitch = 0.02;
      this.speed = 0;
      this.throttle = 0;
    }

    const threat = sys.pirateThreat + (useGameStore.getState().save.wanted ? 2 : 0);
    const count = Math.min(5, threat + (sys.government === "anarchy" ? 1 : 0));
    for (let i = 0; i < count; i += 1) this.spawnNpc("pirate");
    for (let i = 0; i < 2; i += 1) this.spawnNpc("trader");
    if (useGameStore.getState().save.wanted && sys.government !== "anarchy") this.spawnNpc("police");
    if (sys.alienThreat > 0 && Math.random() < Math.min(0.65, sys.alienThreat * 0.16)) this.spawnNpc("thargoid");

    for (let i = 0; i < 6; i += 1) {
      const a = makeAsteroid(AST_COLOR, 4 + Math.random() * 7);
      a.position.set(
        (Math.random() - 0.5) * 900,
        (Math.random() - 0.5) * 220,
        (Math.random() - 0.5) * 900,
      );
      this.spaceRoot.add(a);
      const ores: CommodityId[] = ["minerals", "alloys", "gold"];
      this.asteroids.push({
        mesh: a,
        spin: new THREE.Vector3(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).multiplyScalar(0.4),
        ore: ores[Math.floor(Math.random() * ores.length)]!,
        remaining: 2 + Math.floor(Math.random() * 3),
      });
    }

    this.mode = "space";
    this.frozen = false;
    this.titleRoot.visible = false;
    this.spaceRoot.visible = true;
    this.charging = false;
    this.jumpCharge = 0;
    this.publishHud(true);
  }

  launch() {
    const id = useGameStore.getState().save.systemId;
    this.enterSystem(id, "undock");
    useGameStore.getState().patchSave({ docked: false });
    useGameStore.getState().setMode("space");
    useGameStore.getState().setFlash("GEAR UP  —  CLEAR THE SLOT");
    void logTrafficFn({
      data: { systemId: id, kind: "undock", detail: "A trader undocked." },
    }).catch(() => undefined);
  }

  start() {
    if (this.running) return;
    this.running = true;
    this.last = performance.now();
    this.renderer.setAnimationLoop((t) => this.frame(t));
  }

  stop() {
    this.running = false;
    this.renderer.setAnimationLoop(null);
  }

  setPaused(v: boolean) {
    this.frozen = v || this.mode !== "space";
  }

  setMode(mode: GameMode) {
    this.mode = mode;
    if (mode === "title") {
      this.frozen = true;
      this.titleRoot.visible = true;
      this.spaceRoot.visible = false;
    } else if (mode === "space") {
      this.frozen = false;
      this.titleRoot.visible = false;
      this.spaceRoot.visible = true;
    } else {
      this.frozen = true;
      this.spaceRoot.visible = true;
      this.titleRoot.visible = false;
    }
  }

  lockJump(id: string | null) {
    useGameStore.getState().setJump(id);
    this.charging = false;
    this.jumpCharge = 0;
  }

  dispose() {
    this.stop();
    this.input.detach();
    this.ro?.disconnect();
    this.clearNpcs();
    this.shotGeo.dispose();
    this.shotMatP.dispose();
    this.shotMatE.dispose();
    this.renderer.dispose();
    disposeSharedMaterials();
    if (window.__controlsTest) delete window.__controlsTest;
  }

  private resize() {
    const parent = this.canvas.parentElement ?? this.canvas;
    const w = Math.max(1, parent.clientWidth);
    const h = Math.max(1, parent.clientHeight);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(w, h, false);
  }

  private frame(t: number) {
    const raw = Math.min(0.1, (t - this.last) / 1000 || 0);
    this.last = t;
    this.acc += raw;
    while (this.acc >= FIXED) {
      this.step(FIXED);
      this.acc -= FIXED;
    }
    this.draw(raw);
    this.renderer.render(this.scene, this.camera);
  }

  private step(dt: number) {
    const actions = this.input.poll();
    this.mode = useGameStore.getState().mode;
    this.titleRoot.visible = this.mode === "title";
    this.spaceRoot.visible = this.mode !== "title";

    if (this.mode === "title") {
      this.titleT += dt;
      return;
    }

    if (this.input.edges.map && (this.mode === "space" || this.mode === "map")) {
      const next = this.mode === "map" ? "space" : "map";
      useGameStore.getState().setMode(next);
      this.mode = next;
    }
    if (this.input.edges.pause && this.mode === "space") {
      useGameStore.getState().setPaused(!useGameStore.getState().paused);
    }

    const paused = useGameStore.getState().paused;
    if (paused || this.mode !== "space") {
      if (this.mode === "station" || this.mode === "dead") this.station.rotation.y += dt * 0.12;
      return;
    }

    const save = useGameStore.getState().save;
    const def = shipStats(save.shipId, save.cargoUpgrade, save.loadout);
    let steer = actions.yaw;
    if (this.injectSteer !== null) steer = this.injectSteer;
    this.yaw += steer * def.turnRate * dt;
    this.pitch += actions.pitch * 1.15 * dt;
    this.pitch = Math.max(-1.15, Math.min(1.15, this.pitch));
    this.roll += actions.roll * 1.8 * dt;
    this.roll *= Math.max(0, 1 - 1.6 * dt);
    this.roll = Math.max(-0.9, Math.min(0.9, this.roll));
    this.throttle = Math.max(0, Math.min(1, this.throttle + actions.thrust * 0.7 * dt));
    if (this.input.edges.dampeners) {
      this.inertiaDampeners = !this.inertiaDampeners;
      useGameStore.getState().setFlash(this.inertiaDampeners ? "INERTIA DAMPENERS ON" : "INERTIA DAMPENERS OFF");
    }
    if (this.inertiaDampeners) {
      const target = this.throttle * def.maxSpeed;
      this.speed += (target - this.speed) * Math.min(1, 2.1 * dt);
    } else {
      this.speed += actions.thrust * def.maxSpeed * 0.42 * dt;
      this.speed *= Math.max(0, 1 - 0.035 * dt);
      this.speed = Math.max(0, Math.min(def.maxSpeed * 1.25, this.speed));
    }

    _fwd.set(
      -Math.sin(this.yaw) * Math.cos(this.pitch),
      Math.sin(this.pitch),
      -Math.cos(this.yaw) * Math.cos(this.pitch),
    );
    this.pos.addScaledVector(_fwd, this.speed * dt);
    setEngineLevel(this.throttle);

    this.collideWorld(dt);
    this.fireCd = Math.max(0, this.fireCd - dt);
    this.bombCd = Math.max(0, this.bombCd - dt);
    this.chaffCd = Math.max(0, this.chaffCd - dt);
    this.chaffTimer = Math.max(0, this.chaffTimer - dt);
    this.miningCd = Math.max(0, this.miningCd - dt);
    this.scanCd = Math.max(0, this.scanCd - dt);
    this.salvageCd = Math.max(0, this.salvageCd - dt);
    this.laserHeat = Math.max(0, this.laserHeat - dt * 0.35);
    this.hitCd = Math.max(0, this.hitCd - dt);
    this.shake *= Math.max(0, 1 - 6 * dt);
    this.shieldRegen += dt;
    if (this.hitCd <= 0 && this.shieldRegen > 3.5) {
      const st = useGameStore.getState();
      if (st.shields < st.maxShields) {
        useGameStore.getState().setFlight({ shields: Math.min(st.maxShields, st.shields + 6 * dt) });
      }
    }

    if (actions.fire && this.fireCd <= 0 && this.laserHeat < 1) {
      this.fireCd = 0.16;
      const cooling = hasModule(save.loadout, "cooling_springs") ? 0.07 : 0.12;
      this.laserHeat = Math.min(1, this.laserHeat + cooling);
      const weapon = fittedWeapon(save.loadout);
      const shotPower = weapon === "multicannon" ? def.laser + 4 : weapon === "beam_laser" ? def.laser + 2 : def.laser;
      this.spawnShot(true, this.pos, _fwd, shotPower, false);
      sfxPlay.laser();
    }
    if (this.input.edges.bomb && this.bombCd <= 0) this.deployBomb();
    if (this.input.edges.chaff && this.chaffCd <= 0) this.deployChaff();
    if (actions.mine && this.miningCd <= 0) this.mineAsteroid();
    if (actions.scan && this.scanCd <= 0) this.scanBody();
    if (actions.salvage && this.salvageCd <= 0) this.salvageWreck();

    this.stepNpcs(dt);
    this.stepShots(dt);
    this.stepLoot(dt);
    this.station.rotation.y += dt * 0.18;
    this.planet.rotation.y += dt * 0.02;
    for (const a of this.asteroids) {
      a.mesh.rotation.x += a.spin.x * dt;
      a.mesh.rotation.y += a.spin.y * dt;
    }

    const distSt = this.pos.distanceTo(this.stationPos);
    const distPl = this.pos.distanceTo(this.planetPos);
    const distStar = this.pos.length();
    const dockingComputer = hasModule(save.loadout, "docking_computer");
    const canDock = distSt < (dockingComputer ? 86 : 56) && this.speed < (dockingComputer ? 52 : 38);
    const massLocked = distSt < 95 || distPl < this.planetR + 90 || distStar < 140;

    if (hasModule(save.loadout, "fuel_scoop") && distStar > 70 && distStar < 260 && save.fuel < def.tank) {
      const nextFuel = Math.min(def.tank, save.fuel + dt * 0.65);
      useGameStore.getState().patchSave({ fuel: nextFuel }, false);
      if (Math.floor(nextFuel * 10) !== Math.floor(save.fuel * 10)) {
        useGameStore.getState().setFlash("FUEL SCOOP ACTIVE");
      }
    }

    if (this.input.edges.dock && canDock) this.dock();
    if (this.input.edges.target) this.cycleTarget();

    const locked = useGameStore.getState().jumpLocked;
    if (this.input.edges.jump) {
      if (!locked) {
        useGameStore.getState().setFlash("NO DESTINATION  —  OPEN THE CHART (M)");
      } else if (massLocked) {
        useGameStore.getState().setFlash("MASS LOCKED");
        sfxPlay.warn();
      } else {
        this.charging = !this.charging;
        if (!this.charging) this.jumpCharge = 0;
        else useGameStore.getState().setFlash("HYPERDRIVE CHARGING");
      }
    }
    if (this.charging && locked && !massLocked) {
      this.jumpCharge += dt / 2.1;
      if (this.jumpCharge >= 1) this.completeJump(locked);
    } else if (!this.charging) {
      this.jumpCharge = 0;
    }

    this.hudAcc += dt;
    if (this.hudAcc > 0.09) {
      this.hudAcc = 0;
      this.publishHud(false, { canDock, massLocked, distSt });
    }
  }

  private collideWorld(dt: number) {
    void dt;
    const bump = (pushFrom: THREE.Vector3, minDist: number, dmg: number, reason: string) => {
      _tmp.copy(this.pos).sub(pushFrom);
      if (_tmp.lengthSq() < 0.0001) _tmp.set(0, 1, 0);
      _tmp.setLength(minDist);
      this.pos.copy(pushFrom).add(_tmp);
      this.speed *= 0.28;
      this.throttle *= 0.4;
      if (this.hitCd <= 0) this.damage(dmg, reason);
    };
    if (this.pos.length() < 48) bump(this.star.position, 52, 16, "STAR HEAT");
    if (this.pos.distanceTo(this.planetPos) < this.planetR + 6) {
      bump(this.planetPos, this.planetR + 14, 14, "PLANETARY IMPACT");
    }
    if (this.pos.distanceTo(this.stationPos) < 17) {
      bump(this.stationPos, 20, 6, "STATION COLLISION");
    }
    for (const a of this.asteroids) {
      if (this.pos.distanceTo(a.mesh.position) < 8) {
        bump(a.mesh.position, 12, 7, "ASTEROID STRIKE");
      }
    }
  }

  private spawnNpc(kind: Npc["kind"]) {
    const police = kind === "police";
    const mesh =
      kind === "police"
        ? makeCobra(POLICE_COLOR)
        : kind === "trader"
          ? makeHauler(0x8aa4d0)
          : kind === "thargoid"
            ? makeThargoid(0x8ee6a7)
            : makeSidewinder(PIRATE_COLOR);
    const ang = Math.random() * Math.PI * 2;
    const r = 260 + Math.random() * 340;
    const pos = new THREE.Vector3(
      this.pos.x + Math.cos(ang) * r,
      this.pos.y + (Math.random() - 0.5) * 80,
      this.pos.z + Math.sin(ang) * r,
    );
    mesh.position.copy(pos);
    this.spaceRoot.add(mesh);
    this.npcs.push({
      id: `${kind}-${Math.random().toString(36).slice(2, 7)}`,
      mesh,
      pos,
      yaw: Math.random() * Math.PI * 2,
      pitch: 0,
      speed: kind === "thargoid" ? 58 + Math.random() * 18 : 38 + Math.random() * 24,
      hull: kind === "thargoid" ? 95 : police ? 38 : kind === "trader" ? 30 : 22,
      fireCd: 1 + Math.random(),
      police,
      kind,
    });
  }

  private spawnShot(friendly: boolean, origin: THREE.Vector3, dir: THREE.Vector3, power: number, explosive: boolean) {
    const mesh = new THREE.Mesh(this.shotGeo, friendly ? this.shotMatP : this.shotMatE);
    const pos = origin.clone().addScaledVector(dir, 4);
    mesh.position.copy(pos);
    mesh.lookAt(pos.clone().add(dir));
    this.spaceRoot.add(mesh);
    this.shots.push({
      pos,
      vel: dir.clone().multiplyScalar(friendly ? 420 : 340),
      life: 1.15,
      friendly,
      power,
      explosive,
      mesh,
    });
  }

  private stepNpcs(dt: number) {
    for (const n of this.npcs) {
      _tmp.copy(this.pos).sub(n.pos);
      const dist = _tmp.length();
      const desiredYaw = Math.atan2(-_tmp.x, -_tmp.z);
      let dy = desiredYaw - n.yaw;
      while (dy > Math.PI) dy -= Math.PI * 2;
      while (dy < -Math.PI) dy += Math.PI * 2;
      n.yaw += Math.max(-1.6, Math.min(1.6, dy)) * dt * 1.4;
      const desiredPitch = Math.atan2(_tmp.y, Math.hypot(_tmp.x, _tmp.z));
      n.pitch += (desiredPitch - n.pitch) * dt * 1.2;
      const hold = n.kind === "trader" ? 150 : n.police ? 70 : n.kind === "thargoid" ? 105 : 90;
      if (dist < hold) n.speed = Math.max(18, n.speed - 20 * dt);
      else n.speed = Math.min(70, n.speed + 10 * dt);
      const fx = -Math.sin(n.yaw) * Math.cos(n.pitch);
      const fy = Math.sin(n.pitch);
      const fz = -Math.cos(n.yaw) * Math.cos(n.pitch);
      n.pos.x += fx * n.speed * dt;
      n.pos.y += fy * n.speed * dt;
      n.pos.z += fz * n.speed * dt;
      n.mesh.position.copy(n.pos);
      n.mesh.rotation.set(n.pitch, n.yaw, 0, "YXZ");
      n.fireCd -= dt;
      const stealth = moduleCount(useGameStore.getState().save.loadout, "stealth_mesh");
      const ecm = hasModule(useGameStore.getState().save.loadout, "ecm_suite");
      const eccm = hasModule(useGameStore.getState().save.loadout, "eccm_suite");
      const electronicDisruption = ecm && !eccm && Math.random() < 0.12;
      const hostile = n.kind === "pirate" || n.kind === "police" || n.kind === "thargoid";
      const facing = hostile && Math.abs(dy) < 0.22 && dist < 380 - stealth * 18 && dist > 28 && !electronicDisruption;
      if (facing && n.fireCd <= 0) {
        n.fireCd = n.kind === "thargoid" ? 0.3 : eccm ? 0.42 : 0.55;
        this.spawnShot(false, n.pos, _tmp.copy(this.pos).sub(n.pos).normalize(), n.kind === "thargoid" ? 13 : eccm ? 9 : 8, false);
      }
    }
  }

  private stepShots(dt: number) {
    const current = useGameStore.getState().save;
    const def = shipStats(current.shipId, current.cargoUpgrade, current.loadout);
    for (let i = this.shots.length - 1; i >= 0; i -= 1) {
      const s = this.shots[i]!;
      s.life -= dt;
      s.pos.addScaledVector(s.vel, dt);
      s.mesh.position.copy(s.pos);
      if (s.life <= 0) {
        this.spaceRoot.remove(s.mesh);
        this.shots.splice(i, 1);
        continue;
      }
      if (s.friendly) {
        for (let j = this.npcs.length - 1; j >= 0; j -= 1) {
          const n = this.npcs[j]!;
          if (s.pos.distanceTo(n.pos) < 5.5) {
            n.hull -= s.power;
            if (s.explosive) {
              for (const nearby of this.npcs) {
                if (nearby !== n && nearby.pos.distanceTo(s.pos) < 28) nearby.hull -= s.power * 0.5;
              }
              useGameStore.getState().setFlash("ORDNANCE DETONATION");
            }
            this.spaceRoot.remove(s.mesh);
            this.shots.splice(i, 1);
            sfxPlay.hit();
            if (n.hull <= 0) this.killNpc(j);
            else if (!n.police) useGameStore.getState().patchSave({ wanted: true });
            break;
          }
        }
      } else if (s.pos.distanceTo(this.pos) < 4.2 && this.chaffTimer <= 0) {
        this.spaceRoot.remove(s.mesh);
        this.shots.splice(i, 1);
        this.damage(9, "UNDER FIRE");
      }
    }
  }

  private killNpc(index: number) {
    const n = this.npcs[index]!;
    this.spaceRoot.remove(n.mesh);
    this.npcs.splice(index, 1);
    const can = makeCanister(CAN_COLOR);
    can.position.copy(n.pos);
    this.spaceRoot.add(can);
    const goods: CommodityId[] = ["food", "minerals", "gold", "computers", "luxuries", "alloys"];
    this.loot.push({
      mesh: can,
      pos: n.pos.clone(),
      life: 45,
      good: goods[Math.floor(Math.random() * goods.length)]!,
    });
    const st = useGameStore.getState();
    const reputation = { ...st.save.reputation };
    if (n.kind === "police") {
      reputation.federation = (reputation.federation ?? 0) - 6;
      reputation.empire = (reputation.empire ?? 0) - 3;
    } else if (n.kind === "pirate") {
      reputation.pirates = (reputation.pirates ?? 0) - 2;
      reputation[systemFaction(getSystem(st.save.systemId)!)] =
        (reputation[systemFaction(getSystem(st.save.systemId)!)] ?? 0) + 1;
    } else if (n.kind === "thargoid") {
      reputation.federation = (reputation.federation ?? 0) + 2;
      reputation.empire = (reputation.empire ?? 0) + 2;
    }
    st.patchSave({ kills: st.save.kills + 1, reputation });
    st.setFlash(n.police ? "POLICE CRAFT DESTROYED" : "TARGET DESTROYED");
    sfxPlay.kill();
  }

  private stepLoot(dt: number) {
    const save = useGameStore.getState().save;
    for (let i = this.loot.length - 1; i >= 0; i -= 1) {
      const l = this.loot[i]!;
      l.life -= dt;
      l.mesh.rotation.y += dt * 1.4;
      if (l.life <= 0) {
        this.spaceRoot.remove(l.mesh);
        this.loot.splice(i, 1);
        continue;
      }

      if (this.pos.distanceTo(l.pos) < 10) {
        useGameStore.getState().setFlash("WRECK IN RANGE  —  X TO SALVAGE");
      }
    }
  }

  private mineAsteroid() {
    const st = useGameStore.getState();
    const miningLaser =
      hasModule(st.save.loadout, "mining_laser") ||
      hasModule(st.save.loadout, "prospector_laser") ||
      hasModule(st.save.loadout, "excavator_laser");
    if (!miningLaser) {
      st.setFlash("MINING LASER REQUIRED  —  FIT ONE AT A STATION");
      sfxPlay.warn();
      this.miningCd = 1.2;
      return;
    }
      const forward = _fwd.set(
        -Math.sin(this.yaw) * Math.cos(this.pitch),
        Math.sin(this.pitch),
        -Math.cos(this.yaw) * Math.cos(this.pitch),
      );
      let best: AsteroidField | null = null;
      let bestDist = 170;
      for (const asteroid of this.asteroids) {
        _tmp.copy(asteroid.mesh.position).sub(this.pos);
        const dist = _tmp.length();
        if (dist >= bestDist || _tmp.normalize().dot(forward) < 0.72) continue;
        best = asteroid;
        bestDist = dist;
      }
      if (!best) {
        st.setFlash("NO ASTEROID IN BEAM");
        this.miningCd = 0.8;
        return;
      }
      const used = cargoUsed(st.save.cargo);
      const cap = cargoCapacity(st.save.shipId, st.save.cargoUpgrade, st.save.loadout);
      if (used >= cap) {
        st.setFlash("HOLD FULL");
        sfxPlay.warn();
        this.miningCd = 1;
        return;
      }
      const miningDrone = hasModule(st.save.loadout, "mining_drone");
      const processor = hasModule(st.save.loadout, "ore_processor") || hasModule(st.save.loadout, "refinery_unit");
      const yieldAmount = Math.min(cap - used, miningDrone ? (processor ? 3 : 2) : 1);
      const cargo = { ...st.save.cargo, [best.ore]: (st.save.cargo[best.ore] ?? 0) + yieldAmount };
      best.remaining -= 1;
      st.patchSave({ cargo });
      st.setFlash(`MINED ${yieldAmount}t ${best.ore.toUpperCase()}  —  ${best.remaining} FRAGMENTS LEFT`);
      sfxPlay.mining();
      this.miningCd = 0.75;
      if (best.remaining <= 0) {
        this.spaceRoot.remove(best.mesh);
        this.asteroids = this.asteroids.filter((candidate) => candidate !== best);
      }
  }

  private deployBomb() {
    const st = useGameStore.getState();
    const bomb = Object.values(st.save.loadout).find((id) => id === "plasma_bomb" || id === "fragmentation_bomb");
    if (!bomb) {
      st.setFlash("BOMB HARDPOINT REQUIRED  —  FIT PLASMA OR FRAGMENTATION BOMBS");
      sfxPlay.warn();
      this.bombCd = 1.2;
      return;
    }
    const power = bomb === "plasma_bomb" ? 34 : 22;
    this.spawnShot(true, this.pos, _fwd, power, true);
    this.shots[this.shots.length - 1]!.vel.multiplyScalar(0.32);
    this.shots[this.shots.length - 1]!.life = 2.2;
    this.bombCd = 2.5;
    st.setFlash(`${bomb === "plasma_bomb" ? "PLASMA" : "FRAG"} BOMB DEPLOYED`);
    sfxPlay.bomb();
  }

  private deployChaff() {
    const st = useGameStore.getState();
    if (!hasModule(st.save.loadout, "chaff_launcher")) {
      st.setFlash("CHAFF LAUNCHER REQUIRED");
      sfxPlay.warn();
      this.chaffCd = 1.2;
      return;
    }
    this.chaffTimer = 3.5;
    this.chaffCd = 8;
    st.setFlash("CHAFF CLOUD DEPLOYED");
    sfxPlay.chaff();
  }

  private scanBody() {
    const st = useGameStore.getState();
    const scanStrength = moduleScanStrength(st.save.loadout);
    if (scanStrength <= 0) {
      st.setFlash("DISCOVERY SCANNER REQUIRED  —  FIT ONE AT A STATION");
      sfxPlay.warn();
      this.scanCd = 1.2;
      return;
    }
    const bodies = [
      { id: "star", name: "PRIMARY STAR", pos: this.star.position },
      { id: "planet", name: "PLANETARY BODY", pos: this.planetPos },
    ];
    const body = bodies.sort((a, b) => this.pos.distanceTo(a.pos) - this.pos.distanceTo(b.pos))[0]!;
    const distance = this.pos.distanceTo(body.pos);
    if (distance > 900) {
      st.setFlash("SCAN RANGE EXCEEDED");
      this.scanCd = 1;
      return;
    }
    const key = `${st.save.systemId}:${body.id}`;
    if (st.save.exploredSystems[key]) {
      st.setFlash(`${body.name} ALREADY MAPPED`);
      this.scanCd = 1;
      return;
    }
    const exploredSystems = { ...st.save.exploredSystems, [key]: true };
    const value = Math.max(120, Math.round((420 - distance * 0.18) * scanStrength));
    st.patchSave({
      exploredSystems,
      explorationData: st.save.explorationData + value,
    });
    st.setFlash(`SCAN COMPLETE  —  ${body.name} DATA +${value} CR`);
    sfxPlay.scan();
    this.scanCd = 1.4;
  }

  private salvageWreck() {
    const st = useGameStore.getState();
    if (!Object.values(st.save.loadout).includes("salvage_beam")) {
      st.setFlash("SALVAGE BEAM REQUIRED  —  FIT ONE AT A STATION");
      sfxPlay.warn();
      this.salvageCd = 1.2;
      return;
    }
    const forward = _fwd.set(
      -Math.sin(this.yaw) * Math.cos(this.pitch),
      Math.sin(this.pitch),
      -Math.cos(this.yaw) * Math.cos(this.pitch),
    );
    let best: Loot | null = null;
    let bestDist = 70;
    for (const loot of this.loot) {
      _tmp.copy(loot.pos).sub(this.pos);
      const distance = _tmp.length();
      if (distance >= bestDist || _tmp.normalize().dot(forward) < 0.45) continue;
      best = loot;
      bestDist = distance;
    }
    if (!best) {
      st.setFlash("NO WRECK IN SALVAGE RANGE");
      this.salvageCd = 0.8;
      return;
    }
    const used = cargoUsed(st.save.cargo);
    const cap = cargoCapacity(st.save.shipId, st.save.cargoUpgrade, st.save.loadout);
    if (used >= cap) {
      st.setFlash("HOLD FULL");
      this.salvageCd = 1;
      return;
    }
    const cargo = { ...st.save.cargo, alloys: (st.save.cargo.alloys ?? 0) + 1 };
    st.patchSave({
      cargo,
      salvageRecovered: st.save.salvageRecovered + 1,
    });
    this.spaceRoot.remove(best.mesh);
    this.loot = this.loot.filter((loot) => loot !== best);
    st.setFlash(`SALVAGED 1t ALLOYS  —  ${best.good.toUpperCase()} WRECK`);
    sfxPlay.salvage();
    this.salvageCd = 0.9;
  }

  private damage(amount: number, reason: string) {
    const st = useGameStore.getState();
    this.hitCd = 0.6;
    this.shake = Math.min(1.4, this.shake + amount * 0.05);
    this.shieldRegen = 0;
    let shields = st.shields;
    let hull = st.hull;
    const absorbed = Math.min(shields, amount);
    shields -= absorbed;
    const armor = moduleCount(st.save.loadout, "armor_plating") * 0.1;
    hull -= (amount - absorbed) * Math.max(0.55, 1 - armor);
    st.setFlight({ shields, hull });
    st.patchSave({ shields, hull }, false);
    st.setAlert(reason);
    sfxPlay.hull();
    if (hull <= 0) {
      st.setMode("dead");
      st.setFlash("SHIP DESTROYED");
      this.mode = "dead";
      this.frozen = true;
    }
  }

  private dock() {
    const st = useGameStore.getState();
    const mission = st.save.activeMission;
    const destinationMission =
      mission && mission.destinationId === st.save.systemId ? mission : null;
    const missionCargo = destinationMission ? st.save.cargo[destinationMission.cargo] ?? 0 : 0;
    const missionRequirementMet =
      destinationMission !== null &&
      ((destinationMission.type === "courier" || destinationMission.type === "mining") &&
        missionCargo >= destinationMission.quantity ||
        destinationMission.type === "exploration" &&
          Boolean(st.save.exploredSystems[destinationMission.destinationId]) &&
          st.save.explorationData > destinationMission.progressAtAccept ||
        destinationMission.type === "salvage" &&
          st.save.salvageRecovered - destinationMission.progressAtAccept >= destinationMission.requirement);
    const completedMission = missionRequirementMet ? destinationMission : null;
    const missionComplete = completedMission !== null;
    const explorationPayout = st.save.explorationData;
    const missionFaction = systemFaction(getSystem(st.save.systemId)!);
    const reputation = { ...st.save.reputation };
    if (completedMission) {
      const delta = completedMission.type === "courier" ? 2 : completedMission.type === "exploration" ? 3 : 4;
      reputation[missionFaction] = (reputation[missionFaction] ?? 0) + delta;
      reputation["free-traders"] = (reputation["free-traders"] ?? 0) + 1;
    }
    const deliveredCargo = missionComplete && (completedMission.type === "courier" || completedMission.type === "mining")
      ? {
          ...st.save.cargo,
          [completedMission.cargo]: missionCargo - completedMission.quantity,
        }
      : st.save.cargo;
    if (destinationMission && !missionComplete && (destinationMission.type === "courier" || destinationMission.type === "mining")) {
      st.setFlash(`CARGO REQUIRED  —  ${destinationMission.quantity}t ${destinationMission.cargo.toUpperCase()}`);
    }
    st.patchSave({
      docked: true,
      hull: st.hull,
      shields: st.shields,
      wanted: st.save.wanted,
      cargo: deliveredCargo,
      credits: st.save.credits + explorationPayout + (missionComplete ? completedMission.reward : 0),
      ...(missionComplete
        ? {
            activeMission: null,
            completedMissions: st.save.completedMissions + 1,
            reputation,
          }
        : {}),
      ...(explorationPayout > 0
        ? {
            explorationData: 0,
          }
        : {}),
    });
    st.setMode("station");
    this.mode = "station";
    this.frozen = true;
    this.speed = 0;
    this.throttle = 0;
    st.setFlash(
      missionComplete
        ? `CONTRACT COMPLETE  +${(completedMission.reward + explorationPayout).toLocaleString()} CR`
        : explorationPayout > 0
          ? `EXPLORATION DATA SOLD  +${explorationPayout.toLocaleString()} CR`
          : destinationMission
            ? `CARGO REQUIRED  —  ${destinationMission.quantity}t ${destinationMission.cargo.toUpperCase()}`
            : "DOCKING GRANTED",
    );
    sfxPlay.dock();
    void logTrafficFn({
      data: { systemId: st.save.systemId, kind: "dock", detail: "A trader requested docking." },
    }).catch(() => undefined);
  }

  private completeJump(toId: string) {
    const st = useGameStore.getState();
    const from = st.save.systemId;
    const cost = jumpFuelCost(from, toId);
    if (st.save.fuel < cost) {
      st.setFlash("INSUFFICIENT FUEL");
      this.charging = false;
      this.jumpCharge = 0;
      sfxPlay.warn();
      return;
    }
    const dest = getSystem(toId);
    if (!dest) return;
    const def = shipStats(st.save.shipId, st.save.cargoUpgrade, st.save.loadout);
    if (systemDistance(getSystem(from)!, dest) > def.jump + 0.05) {
      st.setFlash("OUT OF RANGE");
      this.charging = false;
      return;
    }
    sfxPlay.jump();
    st.patchSave({
      systemId: toId,
      fuel: Math.max(0, Math.round((st.save.fuel - cost) * 10) / 10),
      docked: false,
      wanted: false,
    });
    st.setJump(null);
    st.setFlash(`ARRIVED  —  ${dest.name.toUpperCase()}`);
    this.charging = false;
    this.jumpCharge = 0;
    this.enterSystem(toId, "jump");
    void logTrafficFn({
      data: { systemId: toId, kind: "jump", detail: `A ship arrived in ${dest.name}.` },
    }).catch(() => undefined);
  }

  private cycleTarget() {
    const list = [
      { id: "station", dist: this.pos.distanceTo(this.stationPos) },
      ...this.npcs.map((n) => ({ id: n.id, dist: this.pos.distanceTo(n.pos) })),
    ].sort((a, b) => a.dist - b.dist);
    if (!list.length) return;
    const idx = list.findIndex((c) => c.id === this.targetId);
    this.targetId = list[(idx + 1) % list.length]!.id;
  }

  private publishHud(force: boolean, extra?: { canDock: boolean; massLocked: boolean; distSt: number }) {
    void force;
    _fwd.set(
      -Math.sin(this.yaw) * Math.cos(this.pitch),
      Math.sin(this.pitch),
      -Math.cos(this.yaw) * Math.cos(this.pitch),
    );
    _right.set(Math.cos(this.yaw), 0, -Math.sin(this.yaw));
    const contacts: Contact[] = [];
    const push = (id: string, kind: Contact["kind"], world: THREE.Vector3) => {
      _tmp.copy(world).sub(this.pos);
      const dist = _tmp.length();
      contacts.push({
        id,
        kind,
        dist,
        localX: _tmp.dot(_right),
        localZ: _tmp.x * _fwd.x + _tmp.y * _fwd.y + _tmp.z * _fwd.z,
      });
    };
    push("station", "station", this.stationPos);
    push("planet", "planet", this.planetPos);
    push("star", "star", _tmp.set(0, 0, 0));
    for (const n of this.npcs) push(n.id, n.police ? "police" : "pirate", n.pos);
    for (const l of this.loot) push("loot-" + l.good, "canister", l.pos);

    let targetName: string | null = null;
    let targetDist = 0;
    const tgt = contacts.find((c) => c.id === this.targetId) ?? contacts[0];
    if (tgt) {
      this.targetId = tgt.id;
      targetName =
        tgt.kind === "station"
          ? "Coriolis Station"
          : tgt.kind === "planet"
            ? "World"
            : tgt.kind === "star"
              ? "Primary star"
              : tgt.kind === "police"
                ? "System authority"
                : tgt.kind === "canister"
                  ? "Cargo canister"
                  : "Hostile craft";
      targetDist = tgt.dist;
    }

    const distSt = extra?.distSt ?? this.pos.distanceTo(this.stationPos);
    useGameStore.getState().setFlight({
      speed: this.speed,
      throttle: this.throttle,
      dampeners: this.inertiaDampeners,
      yaw: this.yaw,
      pitch: this.pitch,
      shields: useGameStore.getState().shields,
      hull: useGameStore.getState().hull,
      targetName,
      targetDist,
      contacts,
      canDock: extra?.canDock ?? (distSt < 56 && this.speed < 38),
      massLocked: extra?.massLocked ?? distSt < 95,
      laserHeat: this.laserHeat,
    });
    useGameStore.getState().setJumpCharge(this.jumpCharge);
    if (extra?.canDock) useGameStore.getState().setAlert("STATION IN RANGE  —  H TO DOCK");
    else if (this.charging) useGameStore.getState().setAlert("HYPERDRIVE CHARGING");
    else useGameStore.getState().setAlert("");
  }

  private draw(dt: number) {
    this.starfield.position.copy(this.camera.position);
    if (this.mode === "title") {
      const t = this.titleT;
      this.camera.position.set(Math.cos(t * 0.18) * 22, 7 + Math.sin(t * 0.12) * 3, Math.sin(t * 0.18) * 22);
      this.camera.lookAt(0, 0, 0);
      const cobra = this.titleRoot.userData.cobra as THREE.Group;
      const st = this.titleRoot.userData.station as THREE.Group;
      cobra.rotation.y += dt * 0.25;
      cobra.rotation.x = Math.sin(t * 0.4) * 0.08;
      st.rotation.y += dt * 0.1;
      return;
    }

    _fwd.set(
      -Math.sin(this.yaw) * Math.cos(this.pitch),
      Math.sin(this.pitch),
      -Math.cos(this.yaw) * Math.cos(this.pitch),
    );
    _up.set(0, 1, 0);
    const visualRoll = this.roll - (this.input.current.yaw || 0) * 0.35;
    this.player.position.copy(this.pos);
    this.player.rotation.set(this.pitch, this.yaw, visualRoll, "YXZ");

    if (this.mode === "station") {
      const t = performance.now() / 1000;
      this.camera.position.set(
        this.stationPos.x + Math.cos(t * 0.15) * 48,
        this.stationPos.y + 18,
        this.stationPos.z + Math.sin(t * 0.15) * 48,
      );
      this.camera.lookAt(this.stationPos);
      this.player.visible = false;
      return;
    }
    this.player.visible = this.mode !== "dead";

    _desired.copy(this.pos).addScaledVector(_fwd, -18);
    _desired.y += 5.2;
    if (this.shake > 0.01) {
      _desired.x += (Math.random() - 0.5) * this.shake * 1.4;
      _desired.y += (Math.random() - 0.5) * this.shake * 1.1;
    }
    this.camera.position.lerp(_desired, 1 - Math.exp(-4.2 * dt));
    _look.copy(this.pos).addScaledVector(_fwd, 14);
    this.camera.up.set(0, 1, 0);
    this.camera.lookAt(_look);
  }
}
