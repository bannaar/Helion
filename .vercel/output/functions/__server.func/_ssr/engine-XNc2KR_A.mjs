import { c as jumpFuelCost, o as getSystem, u as systemDistance } from "./galaxy-CtkES_h3.mjs";
import { a as cargoCapacity, i as SHIPS, n as logTrafficFn, o as cargoUsed, r as useGameStore, s as sfxPlay } from "./routes-C8RGMmzr.mjs";
import { _ as Points, a as ConeGeometry, b as TorusGeometry, c as FogExp2, d as LineBasicMaterial, f as LineSegments, g as PerspectiveCamera, h as OctahedronGeometry, i as BufferGeometry, l as Group, m as MeshBasicMaterial, n as BoxGeometry, o as CylinderGeometry, p as Mesh, r as BufferAttribute, s as EdgesGeometry, t as WebGLRenderer, u as IcosahedronGeometry, v as PointsMaterial, x as Vector3, y as Scene } from "../_libs/three.mjs";
//#region node_modules/.nitro/vite/services/ssr/assets/engine-XNc2KR_A.js
var empty = () => ({
	yaw: 0,
	pitch: 0,
	roll: 0,
	thrust: 0,
	fire: false,
	dock: false,
	map: false,
	jump: false,
	target: false,
	pause: false
});
function radialDeadzone(x, y, dz = .18) {
	const m = Math.hypot(x, y);
	if (m < dz) return {
		x: 0,
		y: 0
	};
	const scale = (m - dz) / (1 - dz) / m;
	return {
		x: x * scale,
		y: y * scale
	};
}
var GAME_KEYS = /* @__PURE__ */ new Set([
	"KeyW",
	"KeyA",
	"KeyS",
	"KeyD",
	"KeyQ",
	"KeyE",
	"KeyR",
	"KeyF",
	"KeyH",
	"KeyJ",
	"KeyM",
	"Space",
	"Tab",
	"Escape",
	"ArrowUp",
	"ArrowDown",
	"ArrowLeft",
	"ArrowRight",
	"ShiftLeft",
	"ShiftRight"
]);
var Input = class {
	keys = /* @__PURE__ */ new Set();
	injected = null;
	touchYaw = 0;
	touchPitch = 0;
	touchThrust = 0;
	touchFire = false;
	touchDock = false;
	prev = empty();
	current = empty();
	edges = {
		dock: false,
		map: false,
		jump: false,
		target: false,
		pause: false,
		fire: false
	};
	unbind = [];
	enabled = true;
	attach(target = window) {
		const onDown = (e) => {
			if (!this.enabled) return;
			this.keys.add(e.code);
			if (GAME_KEYS.has(e.code)) e.preventDefault();
		};
		const onUp = (e) => this.keys.delete(e.code);
		const clear = () => this.keys.clear();
		window.addEventListener("keydown", onDown);
		window.addEventListener("keyup", onUp);
		window.addEventListener("blur", clear);
		document.addEventListener("visibilitychange", () => {
			if (document.hidden) clear();
		});
		this.unbind.push(() => {
			window.removeEventListener("keydown", onDown);
			window.removeEventListener("keyup", onUp);
			window.removeEventListener("blur", clear);
		});
	}
	detach() {
		for (const fn of this.unbind) fn();
		this.unbind = [];
		this.keys.clear();
	}
	setKeys(codes) {
		this.injected = codes;
	}
	held(code) {
		if (this.injected) return this.injected.includes(code);
		return this.keys.has(code);
	}
	poll() {
		const a = empty();
		if (this.held("KeyA") || this.held("ArrowLeft")) a.yaw += 1;
		if (this.held("KeyD") || this.held("ArrowRight")) a.yaw -= 1;
		if (this.held("ArrowUp") || this.held("KeyR")) a.pitch += 1;
		if (this.held("ArrowDown") || this.held("KeyF")) a.pitch -= 1;
		if (this.held("KeyQ")) a.roll += 1;
		if (this.held("KeyE")) a.roll -= 1;
		if (this.held("KeyW")) a.thrust += 1;
		if (this.held("KeyS")) a.thrust -= 1;
		a.fire = this.held("Space") || this.touchFire;
		a.dock = this.held("KeyH") || this.touchDock;
		a.map = this.held("KeyM");
		a.jump = this.held("KeyJ");
		a.target = this.held("Tab");
		a.pause = this.held("Escape");
		a.yaw += this.touchYaw;
		a.pitch += this.touchPitch;
		a.thrust += this.touchThrust;
		const pads = typeof navigator !== "undefined" ? navigator.getGamepads?.() : null;
		if (pads) for (const pad of pads) {
			if (!pad || pad.mapping !== "standard") continue;
			const stick = radialDeadzone(pad.axes[0] ?? 0, pad.axes[1] ?? 0);
			a.yaw += -stick.x;
			a.pitch += -stick.y;
			const rstick = radialDeadzone(pad.axes[2] ?? 0, pad.axes[3] ?? 0);
			a.roll += -rstick.x;
			const rt = pad.buttons[7]?.value ?? 0;
			const lt = pad.buttons[6]?.value ?? 0;
			a.thrust += rt - lt;
			if (pad.buttons[0]?.pressed) a.fire = true;
			if (pad.buttons[3]?.pressed) a.dock = true;
			if (pad.buttons[9]?.pressed) a.map = true;
			if (pad.buttons[2]?.pressed) a.jump = true;
			if (pad.buttons[1]?.pressed) a.pause = true;
			if (pad.buttons[4]?.pressed) a.target = true;
		}
		a.yaw = Math.max(-1, Math.min(1, a.yaw));
		a.pitch = Math.max(-1, Math.min(1, a.pitch));
		a.roll = Math.max(-1, Math.min(1, a.roll));
		a.thrust = Math.max(-1, Math.min(1, a.thrust));
		this.edges.dock = a.dock && !this.prev.dock;
		this.edges.map = a.map && !this.prev.map;
		this.edges.jump = a.jump && !this.prev.jump;
		this.edges.target = a.target && !this.prev.target;
		this.edges.pause = a.pause && !this.prev.pause;
		this.edges.fire = a.fire && !this.prev.fire;
		this.prev = { ...a };
		this.current = a;
		this.touchDock = false;
		return a;
	}
};
var lineMats = /* @__PURE__ */ new Map();
var fillMats = /* @__PURE__ */ new Map();
function lineMat(color, opacity = 1) {
	const key = color * 10 + opacity;
	let m = lineMats.get(key);
	if (!m) {
		m = new LineBasicMaterial({
			color,
			transparent: opacity < 1,
			opacity,
			depthWrite: opacity >= 1
		});
		lineMats.set(key, m);
	}
	return m;
}
function fillMat(color, opacity = .07) {
	const key = color * 10 + opacity;
	let m = fillMats.get(key);
	if (!m) {
		m = new MeshBasicMaterial({
			color,
			transparent: true,
			opacity,
			side: 2,
			depthWrite: false
		});
		fillMats.set(key, m);
	}
	return m;
}
function edged(geo, color, fill = .06) {
	const g = new Group();
	g.add(new Mesh(geo, fillMat(color, fill)));
	g.add(new LineSegments(new EdgesGeometry(geo, 12), lineMat(color)));
	return g;
}
function makeCobra(color) {
	const geo = new BufferGeometry();
	const v = new Float32Array([
		0,
		0,
		-3.8,
		2.8,
		0,
		1,
		.85,
		.72,
		.4,
		-.85,
		.72,
		.4,
		-2.8,
		0,
		1,
		1.5,
		0,
		2.6,
		-1.5,
		0,
		2.6,
		0,
		-.5,
		.6,
		.4,
		.15,
		2.2,
		-.4,
		.15,
		2.2
	]);
	const idx = [
		0,
		1,
		2,
		0,
		2,
		3,
		0,
		3,
		4,
		0,
		1,
		7,
		0,
		4,
		7,
		1,
		5,
		7,
		4,
		6,
		7,
		2,
		3,
		8,
		3,
		9,
		8,
		1,
		2,
		5,
		4,
		6,
		3,
		5,
		8,
		9,
		5,
		9,
		6,
		5,
		6,
		7
	];
	geo.setAttribute("position", new BufferAttribute(v, 3));
	geo.setIndex(idx);
	geo.computeVertexNormals();
	const g = edged(geo, color, .08);
	g.userData.radius = 3.2;
	return g;
}
function makeSidewinder(color) {
	const geo = new ConeGeometry(1.6, 3.6, 4);
	geo.rotateX(-Math.PI / 2);
	geo.translate(0, 0, -.2);
	const g = edged(geo, color, .08);
	const fin = new BoxGeometry(3.4, .08, 1.1);
	g.add(new Mesh(fin, fillMat(color, .08)));
	g.add(new LineSegments(new EdgesGeometry(fin), lineMat(color)));
	g.userData.radius = 2.4;
	return g;
}
function makeAsp(color) {
	const geo = new OctahedronGeometry(2.1, 0);
	geo.scale(1.15, .45, 2.1);
	const g = edged(geo, color, .07);
	g.userData.radius = 2.8;
	return g;
}
function makeStation(color) {
	const g = new Group();
	const body = new OctahedronGeometry(16, 0);
	g.add(new Mesh(body, fillMat(color, .05)));
	g.add(new LineSegments(new EdgesGeometry(body), lineMat(color)));
	const ring = new TorusGeometry(17.5, .28, 5, 20);
	ring.rotateX(Math.PI / 2);
	g.add(new Mesh(ring, fillMat(color, .1)));
	g.add(new LineSegments(new EdgesGeometry(ring, 1), lineMat(color)));
	const slot = new BoxGeometry(6.5, 2.2, 8);
	const slotMesh = new LineSegments(new EdgesGeometry(slot), lineMat(color, .85));
	slotMesh.position.z = 12;
	g.add(slotMesh);
	g.userData.radius = 18;
	return g;
}
function makePlanet(color, radius) {
	return edged(new IcosahedronGeometry(radius, 1), color, .045);
}
function makeStar(color) {
	const g = new Group();
	const core = new IcosahedronGeometry(28, 1);
	g.add(new Mesh(core, new MeshBasicMaterial({ color })));
	g.add(new LineSegments(new EdgesGeometry(core), lineMat(16777215, .35)));
	const halo = new IcosahedronGeometry(42, 1);
	g.add(new Mesh(halo, new MeshBasicMaterial({
		color,
		transparent: true,
		opacity: .12,
		depthWrite: false
	})));
	g.userData.radius = 28;
	return g;
}
function makeAsteroid(color, r) {
	const geo = new IcosahedronGeometry(r, 0);
	geo.scale(1, .7 + Math.random() * .5, .8 + Math.random() * .4);
	return edged(geo, color, .05);
}
function makeCanister(color) {
	const geo = new CylinderGeometry(.6, .6, 1.6, 6);
	geo.rotateZ(Math.PI / 2);
	return edged(geo, color, .14);
}
function makeStarfield(n = 700) {
	const pos = new Float32Array(n * 3);
	for (let i = 0; i < n; i += 1) {
		const r = 420 + Math.random() * 1800;
		const theta = Math.random() * Math.PI * 2;
		const phi = Math.acos(2 * Math.random() - 1);
		pos[i * 3] = r * Math.sin(phi) * Math.cos(theta);
		pos[i * 3 + 1] = r * Math.sin(phi) * Math.sin(theta);
		pos[i * 3 + 2] = r * Math.cos(phi);
	}
	const geo = new BufferGeometry();
	geo.setAttribute("position", new BufferAttribute(pos, 3));
	const mat = new PointsMaterial({
		color: 13624554,
		size: 1.4,
		sizeAttenuation: false,
		depthWrite: false
	});
	return new Points(geo, mat);
}
function disposeSharedMaterials() {
	for (const m of lineMats.values()) m.dispose();
	for (const m of fillMats.values()) m.dispose();
	lineMats.clear();
	fillMats.clear();
}
var FIXED = 1 / 60;
var PLAYER_COLOR = 9426120;
var PIRATE_COLOR = 13662826;
var POLICE_COLOR = 9086160;
var STATION_COLOR = 12964052;
var AST_COLOR = 9077880;
var CAN_COLOR = 13943968;
var _fwd = new Vector3();
var _up = new Vector3();
var _desired = new Vector3();
var _look = new Vector3();
var _right = new Vector3();
var _tmp = new Vector3();
function shipMesh(id, color) {
	if (id === "sidewinder") return makeSidewinder(color);
	if (id === "asp") return makeAsp(color);
	return makeCobra(color);
}
var HelionEngine = class {
	canvas;
	renderer;
	scene = new Scene();
	camera = new PerspectiveCamera(62, 1, .4, 6e3);
	input = new Input();
	running = false;
	acc = 0;
	last = 0;
	hudAcc = 0;
	mode = "title";
	frozen = true;
	yaw = 0;
	pitch = 0;
	roll = 0;
	speed = 0;
	throttle = 0;
	pos = new Vector3();
	player = new Group();
	starfield;
	titleRoot = new Group();
	spaceRoot = new Group();
	station = new Group();
	planet = new Group();
	star = new Group();
	stationPos = new Vector3(640, 24, 280);
	planetPos = new Vector3(980, -50, 420);
	planetR = 150;
	npcs = [];
	shots = [];
	loot = [];
	asteroids = [];
	shotGeo = new BoxGeometry(.18, .18, 1.8);
	shotMatP = new MeshBasicMaterial({ color: PLAYER_COLOR });
	shotMatE = new MeshBasicMaterial({ color: PIRATE_COLOR });
	fireCd = 0;
	laserHeat = 0;
	hitCd = 0;
	shake = 0;
	titleT = 0;
	jumpCharge = 0;
	charging = false;
	targetId = null;
	injectSteer = null;
	ro = null;
	shieldRegen = 0;
	constructor(canvas) {
		this.canvas = canvas;
		this.renderer = new WebGLRenderer({
			canvas,
			antialias: false,
			powerPreference: "low-power",
			alpha: false
		});
		this.renderer.setClearColor(461068, 1);
		this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 1.25));
		this.scene.fog = new FogExp2(461068, 55e-5);
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
	wireProbe() {
		if (!(typeof window !== "undefined" && /(?:\?|&)qa=1\b/.test(window.location.search)) && typeof window !== "undefined") {}
		window.__controlsTest = {
			getYaw: () => this.yaw,
			getSpeed: () => this.speed,
			setKeys: (codes) => {
				this.input.setKeys(codes);
				if (codes.length === 0) this.injectSteer = null;
			},
			setSteer: (v) => {
				this.injectSteer = v;
			}
		};
	}
	buildTitle() {
		const cobra = makeCobra(PLAYER_COLOR);
		cobra.position.set(0, 0, 0);
		const st = makeStation(STATION_COLOR);
		st.position.set(28, -6, -36);
		st.scale.setScalar(.45);
		const pl = makePlanet(6982298, 22);
		pl.position.set(-40, -8, -70);
		this.titleRoot.add(cobra, st, pl);
		this.titleRoot.userData.cobra = cobra;
		this.titleRoot.userData.station = st;
	}
	buildSpaceScaffold() {
		this.station = makeStation(STATION_COLOR);
		this.station.position.copy(this.stationPos);
		this.planet = makePlanet(6982298, this.planetR);
		this.planet.position.copy(this.planetPos);
		this.star = makeStar(15918788);
		this.player = new Group();
		this.spaceRoot.add(this.star, this.planet, this.station, this.player);
	}
	rebuildPlayerMesh() {
		while (this.player.children.length) {
			const ch = this.player.children[0];
			this.player.remove(ch);
		}
		const id = useGameStore.getState().save.shipId;
		this.player.add(shipMesh(id, PLAYER_COLOR));
	}
	clearNpcs() {
		for (const n of this.npcs) this.spaceRoot.remove(n.mesh);
		this.npcs = [];
		for (const s of this.shots) this.spaceRoot.remove(s.mesh);
		this.shots = [];
		for (const l of this.loot) this.spaceRoot.remove(l.mesh);
		this.loot = [];
		for (const a of this.asteroids) this.spaceRoot.remove(a.mesh);
		this.asteroids = [];
	}
	enterSystem(systemId, kind) {
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
			this.pos.copy(this.stationPos).add(_tmp.set(62, 10, 28));
			this.yaw = Math.atan2(-(this.stationPos.x - this.pos.x), -(this.stationPos.z - this.pos.z));
			this.pitch = 0;
			this.roll = 0;
			this.speed = 8;
			this.throttle = .18;
		} else if (kind === "jump") {
			this.pos.set(1180, 50, -240);
			this.yaw = Math.atan2(-(this.stationPos.x - this.pos.x), -(this.stationPos.z - this.pos.z));
			this.pitch = -.08;
			this.speed = 22;
			this.throttle = .3;
		} else {
			this.pos.copy(this.stationPos).add(_tmp.set(78, 16, 42));
			this.yaw = Math.atan2(-(this.stationPos.x - this.pos.x), -(this.stationPos.z - this.pos.z));
			this.pitch = -.05;
			this.speed = 0;
			this.throttle = 0;
		}
		const threat = sys.pirateThreat + (useGameStore.getState().save.wanted ? 2 : 0);
		const count = Math.min(5, threat + (sys.government === "anarchy" ? 1 : 0));
		for (let i = 0; i < count; i += 1) this.spawnNpc(false);
		if (useGameStore.getState().save.wanted && sys.government !== "anarchy") this.spawnNpc(true);
		for (let i = 0; i < 6; i += 1) {
			const a = makeAsteroid(AST_COLOR, 4 + Math.random() * 7);
			a.position.set((Math.random() - .5) * 900, (Math.random() - .5) * 220, (Math.random() - .5) * 900);
			this.spaceRoot.add(a);
			this.asteroids.push({
				mesh: a,
				spin: new Vector3(Math.random() - .5, Math.random() - .5, Math.random() - .5).multiplyScalar(.4)
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
		logTrafficFn({ data: {
			systemId: id,
			kind: "undock",
			detail: "A trader undocked."
		} }).catch(() => void 0);
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
	setPaused(v) {
		this.frozen = v || this.mode !== "space";
	}
	setMode(mode) {
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
	lockJump(id) {
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
	resize() {
		const parent = this.canvas.parentElement ?? this.canvas;
		const w = Math.max(1, parent.clientWidth);
		const h = Math.max(1, parent.clientHeight);
		this.camera.aspect = w / h;
		this.camera.updateProjectionMatrix();
		this.renderer.setSize(w, h, false);
	}
	frame(t) {
		const raw = Math.min(.1, (t - this.last) / 1e3 || 0);
		this.last = t;
		this.acc += raw;
		while (this.acc >= FIXED) {
			this.step(FIXED);
			this.acc -= FIXED;
		}
		this.draw(raw);
		this.renderer.render(this.scene, this.camera);
	}
	step(dt) {
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
		if (this.input.edges.pause && this.mode === "space") useGameStore.getState().setPaused(!useGameStore.getState().paused);
		if (useGameStore.getState().paused || this.mode !== "space") {
			if (this.mode === "station" || this.mode === "dead") this.station.rotation.y += dt * .12;
			return;
		}
		const save = useGameStore.getState().save;
		const def = SHIPS[save.shipId];
		let steer = actions.yaw;
		if (this.injectSteer !== null) steer = this.injectSteer;
		this.yaw += steer * def.turnRate * dt;
		this.pitch += actions.pitch * 1.15 * dt;
		this.pitch = Math.max(-1.15, Math.min(1.15, this.pitch));
		this.roll += actions.roll * 1.8 * dt;
		this.roll *= Math.max(0, 1 - 1.6 * dt);
		this.roll = Math.max(-.9, Math.min(.9, this.roll));
		this.throttle = Math.max(0, Math.min(1, this.throttle + actions.thrust * .7 * dt));
		const target = this.throttle * def.maxSpeed;
		this.speed += (target - this.speed) * Math.min(1, 2.1 * dt);
		_fwd.set(-Math.sin(this.yaw) * Math.cos(this.pitch), Math.sin(this.pitch), -Math.cos(this.yaw) * Math.cos(this.pitch));
		this.pos.addScaledVector(_fwd, this.speed * dt);
		this.collideWorld(dt);
		this.fireCd = Math.max(0, this.fireCd - dt);
		this.laserHeat = Math.max(0, this.laserHeat - dt * .35);
		this.hitCd = Math.max(0, this.hitCd - dt);
		this.shake *= Math.max(0, 1 - 6 * dt);
		this.shieldRegen += dt;
		if (this.hitCd <= 0 && this.shieldRegen > 3.5) {
			const st = useGameStore.getState();
			if (st.shields < st.maxShields) useGameStore.getState().setFlight({ shields: Math.min(st.maxShields, st.shields + 6 * dt) });
		}
		if (actions.fire && this.fireCd <= 0 && this.laserHeat < 1) {
			this.fireCd = .16;
			this.laserHeat = Math.min(1, this.laserHeat + .12);
			this.spawnShot(true, this.pos, _fwd, def.laser);
			sfxPlay.laser();
		}
		this.stepNpcs(dt);
		this.stepShots(dt);
		this.stepLoot(dt);
		this.station.rotation.y += dt * .18;
		this.planet.rotation.y += dt * .02;
		for (const a of this.asteroids) {
			a.mesh.rotation.x += a.spin.x * dt;
			a.mesh.rotation.y += a.spin.y * dt;
		}
		const distSt = this.pos.distanceTo(this.stationPos);
		const distPl = this.pos.distanceTo(this.planetPos);
		const distStar = this.pos.length();
		const canDock = distSt < 56 && this.speed < 38;
		const massLocked = distSt < 95 || distPl < this.planetR + 90 || distStar < 140;
		if (this.input.edges.dock && canDock) this.dock();
		if (this.input.edges.target) this.cycleTarget();
		const locked = useGameStore.getState().jumpLocked;
		if (this.input.edges.jump) {
			if (!locked) useGameStore.getState().setFlash("NO DESTINATION  —  OPEN THE CHART (M)");
			else if (massLocked) {
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
		} else if (!this.charging) this.jumpCharge = 0;
		this.hudAcc += dt;
		if (this.hudAcc > .09) {
			this.hudAcc = 0;
			this.publishHud(false, {
				canDock,
				massLocked,
				distSt
			});
		}
	}
	collideWorld(dt) {
		if (this.pos.length() < 48) {
			this.pos.setLength(48);
			this.damage(18, "STAR HEAT");
		}
		if (this.pos.distanceTo(this.planetPos) < this.planetR + 4) {
			_tmp.copy(this.pos).sub(this.planetPos).setLength(this.planetR + 6);
			this.pos.copy(this.planetPos).add(_tmp);
			this.speed *= .4;
			this.damage(10, "PLANETARY IMPACT");
		}
		if (this.pos.distanceTo(this.stationPos) < 17) {
			_tmp.copy(this.pos).sub(this.stationPos).setLength(18);
			this.pos.copy(this.stationPos).add(_tmp);
			this.speed *= .5;
			this.damage(6, "STATION COLLISION");
		}
		for (const a of this.asteroids) if (this.pos.distanceTo(a.mesh.position) < 8) {
			this.damage(8, "ASTEROID STRIKE");
			this.speed *= .7;
		}
	}
	spawnNpc(police) {
		const mesh = police ? makeCobra(POLICE_COLOR) : makeSidewinder(PIRATE_COLOR);
		const ang = Math.random() * Math.PI * 2;
		const r = 260 + Math.random() * 340;
		const pos = new Vector3(this.pos.x + Math.cos(ang) * r, this.pos.y + (Math.random() - .5) * 80, this.pos.z + Math.sin(ang) * r);
		mesh.position.copy(pos);
		this.spaceRoot.add(mesh);
		this.npcs.push({
			id: `${police ? "pol" : "pir"}-${Math.random().toString(36).slice(2, 7)}`,
			mesh,
			pos,
			yaw: Math.random() * Math.PI * 2,
			pitch: 0,
			speed: 38 + Math.random() * 24,
			hull: police ? 38 : 22,
			fireCd: 1 + Math.random(),
			police
		});
	}
	spawnShot(friendly, origin, dir, _power) {
		const mesh = new Mesh(this.shotGeo, friendly ? this.shotMatP : this.shotMatE);
		const pos = origin.clone().addScaledVector(dir, 4);
		mesh.position.copy(pos);
		mesh.lookAt(pos.clone().add(dir));
		this.spaceRoot.add(mesh);
		this.shots.push({
			pos,
			vel: dir.clone().multiplyScalar(friendly ? 420 : 340),
			life: 1.15,
			friendly,
			mesh
		});
	}
	stepNpcs(dt) {
		for (const n of this.npcs) {
			_tmp.copy(this.pos).sub(n.pos);
			const dist = _tmp.length();
			let dy = Math.atan2(-_tmp.x, -_tmp.z) - n.yaw;
			while (dy > Math.PI) dy -= Math.PI * 2;
			while (dy < -Math.PI) dy += Math.PI * 2;
			n.yaw += Math.max(-1.6, Math.min(1.6, dy)) * dt * 1.4;
			const desiredPitch = Math.atan2(_tmp.y, Math.hypot(_tmp.x, _tmp.z));
			n.pitch += (desiredPitch - n.pitch) * dt * 1.2;
			if (dist < (n.police ? 70 : 90)) n.speed = Math.max(18, n.speed - 20 * dt);
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
			if (Math.abs(dy) < .22 && dist < 380 && dist > 28 && n.fireCd <= 0) {
				n.fireCd = .55;
				this.spawnShot(false, n.pos, _tmp.copy(this.pos).sub(n.pos).normalize(), 8);
			}
		}
	}
	stepShots(dt) {
		const def = SHIPS[useGameStore.getState().save.shipId];
		for (let i = this.shots.length - 1; i >= 0; i -= 1) {
			const s = this.shots[i];
			s.life -= dt;
			s.pos.addScaledVector(s.vel, dt);
			s.mesh.position.copy(s.pos);
			if (s.life <= 0) {
				this.spaceRoot.remove(s.mesh);
				this.shots.splice(i, 1);
				continue;
			}
			if (s.friendly) for (let j = this.npcs.length - 1; j >= 0; j -= 1) {
				const n = this.npcs[j];
				if (s.pos.distanceTo(n.pos) < 5.5) {
					n.hull -= def.laser;
					this.spaceRoot.remove(s.mesh);
					this.shots.splice(i, 1);
					sfxPlay.hit();
					if (n.hull <= 0) this.killNpc(j);
					else if (!n.police) useGameStore.getState().patchSave({ wanted: true });
					break;
				}
			}
			else if (s.pos.distanceTo(this.pos) < 4.2) {
				this.spaceRoot.remove(s.mesh);
				this.shots.splice(i, 1);
				this.damage(9, "UNDER FIRE");
			}
		}
	}
	killNpc(index) {
		const n = this.npcs[index];
		this.spaceRoot.remove(n.mesh);
		this.npcs.splice(index, 1);
		const can = makeCanister(CAN_COLOR);
		can.position.copy(n.pos);
		this.spaceRoot.add(can);
		const goods = [
			"food",
			"minerals",
			"gold",
			"computers",
			"luxuries",
			"alloys"
		];
		this.loot.push({
			mesh: can,
			pos: n.pos.clone(),
			life: 45,
			good: goods[Math.floor(Math.random() * goods.length)]
		});
		const st = useGameStore.getState();
		st.patchSave({ kills: st.save.kills + 1 });
		st.setFlash(n.police ? "POLICE CRAFT DESTROYED" : "TARGET DESTROYED");
		sfxPlay.kill();
	}
	stepLoot(dt) {
		const save = useGameStore.getState().save;
		for (let i = this.loot.length - 1; i >= 0; i -= 1) {
			const l = this.loot[i];
			l.life -= dt;
			l.mesh.rotation.y += dt * 1.4;
			if (l.life <= 0) {
				this.spaceRoot.remove(l.mesh);
				this.loot.splice(i, 1);
				continue;
			}
			if (this.pos.distanceTo(l.pos) < 10) {
				if (cargoUsed(save.cargo) < cargoCapacity(save.shipId, save.cargoUpgrade)) {
					const cargo = {
						...save.cargo,
						[l.good]: (save.cargo[l.good] ?? 0) + 1
					};
					useGameStore.getState().patchSave({ cargo });
					useGameStore.getState().setFlash(`SCOOPED 1t ${l.good.toUpperCase()}`);
					sfxPlay.scoop();
				} else useGameStore.getState().setFlash("HOLD FULL");
				this.spaceRoot.remove(l.mesh);
				this.loot.splice(i, 1);
			}
		}
	}
	damage(amount, reason) {
		const st = useGameStore.getState();
		this.hitCd = .6;
		this.shake = Math.min(1.4, this.shake + amount * .05);
		this.shieldRegen = 0;
		let shields = st.shields;
		let hull = st.hull;
		const absorbed = Math.min(shields, amount);
		shields -= absorbed;
		hull -= amount - absorbed;
		st.setFlight({
			shields,
			hull
		});
		st.patchSave({
			shields,
			hull
		}, false);
		st.setAlert(reason);
		sfxPlay.hull();
		if (hull <= 0) {
			st.setMode("dead");
			st.setFlash("SHIP DESTROYED");
			this.mode = "dead";
			this.frozen = true;
		}
	}
	dock() {
		const st = useGameStore.getState();
		st.patchSave({
			docked: true,
			hull: st.hull,
			shields: st.shields,
			wanted: st.save.wanted
		});
		st.setMode("station");
		this.mode = "station";
		this.frozen = true;
		this.speed = 0;
		this.throttle = 0;
		st.setFlash("DOCKING GRANTED");
		sfxPlay.dock();
		logTrafficFn({ data: {
			systemId: st.save.systemId,
			kind: "dock",
			detail: "A trader requested docking."
		} }).catch(() => void 0);
	}
	completeJump(toId) {
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
		const def = SHIPS[st.save.shipId];
		if (systemDistance(getSystem(from), dest) > def.jump + .05) {
			st.setFlash("OUT OF RANGE");
			this.charging = false;
			return;
		}
		sfxPlay.jump();
		st.patchSave({
			systemId: toId,
			fuel: Math.max(0, Math.round((st.save.fuel - cost) * 10) / 10),
			docked: false,
			wanted: false
		});
		st.setJump(null);
		st.setFlash(`ARRIVED  —  ${dest.name.toUpperCase()}`);
		this.charging = false;
		this.jumpCharge = 0;
		this.enterSystem(toId, "jump");
		logTrafficFn({ data: {
			systemId: toId,
			kind: "jump",
			detail: `A ship arrived in ${dest.name}.`
		} }).catch(() => void 0);
	}
	cycleTarget() {
		const list = [{
			id: "station",
			dist: this.pos.distanceTo(this.stationPos)
		}, ...this.npcs.map((n) => ({
			id: n.id,
			dist: this.pos.distanceTo(n.pos)
		}))].sort((a, b) => a.dist - b.dist);
		if (!list.length) return;
		const idx = list.findIndex((c) => c.id === this.targetId);
		this.targetId = list[(idx + 1) % list.length].id;
	}
	publishHud(force, extra) {
		_fwd.set(-Math.sin(this.yaw) * Math.cos(this.pitch), Math.sin(this.pitch), -Math.cos(this.yaw) * Math.cos(this.pitch));
		_right.set(Math.cos(this.yaw), 0, -Math.sin(this.yaw));
		const contacts = [];
		const push = (id, kind, world) => {
			_tmp.copy(world).sub(this.pos);
			const dist = _tmp.length();
			contacts.push({
				id,
				kind,
				dist,
				localX: _tmp.dot(_right),
				localZ: _tmp.x * _fwd.x + _tmp.y * _fwd.y + _tmp.z * _fwd.z
			});
		};
		push("station", "station", this.stationPos);
		push("planet", "planet", this.planetPos);
		push("star", "star", _tmp.set(0, 0, 0));
		for (const n of this.npcs) push(n.id, n.police ? "police" : "pirate", n.pos);
		for (const l of this.loot) push("loot-" + l.good, "canister", l.pos);
		let targetName = null;
		let targetDist = 0;
		const tgt = contacts.find((c) => c.id === this.targetId) ?? contacts[0];
		if (tgt) {
			this.targetId = tgt.id;
			targetName = tgt.kind === "station" ? "Coriolis Station" : tgt.kind === "planet" ? "World" : tgt.kind === "star" ? "Primary star" : tgt.kind === "police" ? "System authority" : tgt.kind === "canister" ? "Cargo canister" : "Hostile craft";
			targetDist = tgt.dist;
		}
		const distSt = extra?.distSt ?? this.pos.distanceTo(this.stationPos);
		useGameStore.getState().setFlight({
			speed: this.speed,
			throttle: this.throttle,
			yaw: this.yaw,
			pitch: this.pitch,
			shields: useGameStore.getState().shields,
			hull: useGameStore.getState().hull,
			targetName,
			targetDist,
			contacts,
			canDock: extra?.canDock ?? (distSt < 56 && this.speed < 38),
			massLocked: extra?.massLocked ?? distSt < 95,
			laserHeat: this.laserHeat
		});
		useGameStore.getState().setJumpCharge(this.jumpCharge);
		if (extra?.canDock) useGameStore.getState().setAlert("STATION IN RANGE  —  H TO DOCK");
		else if (this.charging) useGameStore.getState().setAlert("HYPERDRIVE CHARGING");
		else useGameStore.getState().setAlert("");
	}
	draw(dt) {
		this.starfield.position.copy(this.camera.position);
		if (this.mode === "title") {
			const t = this.titleT;
			this.camera.position.set(Math.cos(t * .18) * 22, 7 + Math.sin(t * .12) * 3, Math.sin(t * .18) * 22);
			this.camera.lookAt(0, 0, 0);
			const cobra = this.titleRoot.userData.cobra;
			const st = this.titleRoot.userData.station;
			cobra.rotation.y += dt * .25;
			cobra.rotation.x = Math.sin(t * .4) * .08;
			st.rotation.y += dt * .1;
			return;
		}
		_fwd.set(-Math.sin(this.yaw) * Math.cos(this.pitch), Math.sin(this.pitch), -Math.cos(this.yaw) * Math.cos(this.pitch));
		_up.set(0, 1, 0);
		const visualRoll = this.roll - (this.input.current.yaw || 0) * .35;
		this.player.position.copy(this.pos);
		this.player.rotation.set(this.pitch, this.yaw, visualRoll, "YXZ");
		if (this.mode === "station") {
			const t = performance.now() / 1e3;
			this.camera.position.set(this.stationPos.x + Math.cos(t * .15) * 48, this.stationPos.y + 18, this.stationPos.z + Math.sin(t * .15) * 48);
			this.camera.lookAt(this.stationPos);
			this.player.visible = false;
			return;
		}
		this.player.visible = this.mode !== "dead";
		_desired.copy(this.pos).addScaledVector(_fwd, -18);
		_desired.y += 5.2;
		if (this.shake > .01) {
			_desired.x += (Math.random() - .5) * this.shake * 1.4;
			_desired.y += (Math.random() - .5) * this.shake * 1.1;
		}
		this.camera.position.lerp(_desired, 1 - Math.exp(-4.2 * dt));
		_look.copy(this.pos).addScaledVector(_fwd, 14);
		this.camera.up.set(0, 1, 0);
		this.camera.lookAt(_look);
	}
};
//#endregion
export { HelionEngine };
