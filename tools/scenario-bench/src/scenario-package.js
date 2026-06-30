// scenario-package.js — Parse a scenario directory and the exported template.
//
// A scenario package is a directory:
//   scenarios/<name>/
//     algorithm-template.json   # exported from the platform (single JSON file)
//     scenario.yml              # load profile + base params
//     thresholds.yml            # pass/fail rules
//     videos.yml                # video source mode + sources
//
// The exported algorithm-template.json has several fields whose VALUES are JSON
// strings (not nested objects): algorithmProcessdata, atomicList, algorithmMetadata.
// They must be parsed a second time. The frame-rate baseline for throughput
// comparison lives in the orchestration graph at node AA_00001, in
// configObject.params: [{ key: 'fps', value: '3' }].

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parse as parseYaml } from 'yaml';

const FPS_ACTION_ID = 'AA_00001';

export class ScenarioPackage {
  /**
   * @param {string} dir absolute or relative path to the scenario directory.
   */
  constructor(dir) {
    this.dir = path.resolve(dir);
    this.template = null;       // parsed algorithm-template.json (object)
    this.scenario = null;       // parsed scenario.yml (object)
    this.thresholds = null;     // parsed thresholds.yml (object)
    this.videos = null;         // parsed videos.yml (object)
    this.targetFps = null;      // extracted orchestration fps baseline
  }

  /** Load and validate all four files. Returns this for chaining. */
  load() {
    this.template = this._readJson('algorithm-template.json');
    this.scenario = parseYaml(this._readText('scenario.yml'));
    this.thresholds = parseYaml(this._readText('thresholds.yml')) ?? {};
    this.videos = parseYaml(this._readText('videos.yml'));
    this._resolveVideoPaths();

    this._validate();
    this.targetFps = this._extractFps();
    return this;
  }

  // ── accessors ──────────────────────────────────────────────────────────

  /** algorithmId as a string (matches DTO convention). */
  get algorithmId() {
    return String(this.scenario.algorithmId ?? this.template.algorithmCode ?? '');
  }

  get scheduleId() {
    return this.scenario.scheduleId ?? '';
  }

  get loadProfile() {
    return this.scenario.loadProfile ?? [];
  }

  get sampleIntervalSec() {
    return this.scenario.sampleIntervalSec ?? 5;
  }

  get videoMode() {
    const m = this.videos?.mode;
    if (!['local', 'rtsp-fidelity', 'rtsp-deterministic'].includes(m)) {
      throw new Error(`videos.yml: unsupported mode "${m}"`);
    }
    return m;
  }

  get features() {
    return this.scenario.features ?? {};
  }

  /**
   * Local (file-based) video repeat count. 0 = loop forever (AlgChannelDemux
   * treats video_repeat_count_ <= 0 as unlimited), 1 = play once then stop.
   * Defaults to 0 for local mode so the task sustains load across holdSec.
   * Set explicitly in scenario.yml to override (e.g. 1 for single-pass tests).
   */
  get videoRepeatCount() {
    const v = this.scenario.videoRepeatCount;
    if (v == null) return this.videoMode === 'local' ? 0 : 1;
    return Number(v);
  }

  /**
   * taskConfig payload for /Task/ApplyParamsBatch.
   * Injects param.videoRepeatCount (key::CHANNEL_SOURCE_REPEAT in Keys.h) so the
   * demuxer loops local videos. For rtsp modes repeat count is irrelevant but
   * harmless. Callers may extend this with extra params before sending.
   */
  get taskConfig() {
    const base = this.template.taskConfig ?? { params: [], areas: [] };
    const params = Array.isArray(base.params) ? [...base.params] : [];
    const hasRepeat = params.some((p) => p?.key === 'param.videoRepeatCount');
    if (!hasRepeat) {
      params.push({ key: 'param.videoRepeatCount', value: String(this.videoRepeatCount) });
    }
    return { ...base, params, areas: base.areas ?? [] };
  }

  /**
   * Payload to send straight to /algorithm/layout/save.
   *
   * The backend DTO (AlgorithmDto_Layout.cc `MsgLayoutSaveRecv::from_json`) only reads a
   * FIXED set of fields and does so by reference (`j.at()` / `JSON_OPT`), so any extra
   * fields in a full platform export are simply ignored — that is NOT the failure mode.
   * The real trap is TYPE: every field in MsgLayoutSaveRecv is `std::string`, including
   * `algorithmCategory` and `algorithmUsage`. Platform exports serialize those two as JSON
   * NUMBERS (e.g. 2 / 1). nlohmann's `get_to<std::string>()` on a JSON number throws
   * `type_error`, which the router surfaces as code 24「参数异常」. This is why a freshly
   * exported 100KB+ template is rejected while a hand-trimmed one works.
   *
   * Fix: build a minimal payload with ONLY the DTO-known fields, and coerce the two
   * numeric ones to strings. We also normalize the id field (export uses `algorithmId`,
   * sometimes `id`/`algorithmCode`) and map `confVersionName` → `configVersionName`
   * (the DTO field name; the export's `confVersionName` is a different, ignored field).
   */
  get layoutSavePayload() {
    const t = this.template;
    const algorithmId = String(t.algorithmId ?? t.id ?? t.algorithmCode ?? '');
    if (!algorithmId) throw new Error('template: cannot derive algorithmId for layout save');
    const str = (v) => (v == null ? undefined : String(v));
    return {
      confVersionId: t.confVersionId,
      algorithmId,
      configVersionName: str(t.configVersionName ?? t.confVersionName),
      algorithmCategory: str(t.algorithmCategory),
      algorithmUsage: str(t.algorithmUsage),
      remark: str(t.remark),
      atomicList: str(t.atomicList),
      algorithmProcessdata: str(t.algorithmProcessdata),
      algorithmMetadata: str(t.algorithmMetadata),
      filePath: str(t.filePath),
    };
  }

  // ── internals ──────────────────────────────────────────────────────────

  /**
   * Resolve relative `file:` entries in videos.yml (local mode) against the
   * scenario directory, so the channel manager receives absolute paths.
   */
  _resolveVideoPaths() {
    if (this.videos?.mode !== 'local' || !Array.isArray(this.videos.local)) return;
    for (const src of this.videos.local) {
      if (src.file && !path.isAbsolute(src.file)) {
        src.file = path.resolve(this.dir, src.file);
      }
    }
  }
  _readText(name) {
    const p = path.join(this.dir, name);
    if (!fs.existsSync(p)) {
      throw new Error(`Missing scenario file: ${p}`);
    }
    return fs.readFileSync(p, 'utf8');
  }

  _readJson(name) {
    return JSON.parse(this._readText(name));
  }

  /**
   * The export JSON stores algorithmProcessdata as a JSON STRING (a list of
   * action nodes). Parse it once into an array of nodes.
   */
  _parseProcessData() {
    const raw = this.template.algorithmProcessdata;
    if (raw == null) return [];
    if (typeof raw === 'string') {
      try {
        const parsed = JSON.parse(raw);
        return Array.isArray(parsed) ? parsed : [];
      } catch {
        return [];
      }
    }
    return Array.isArray(raw) ? raw : [];
  }

  /**
   * Extract the orchestration frame-rate baseline.
   * Walks the AA_00001 (目标检测算法) node -> configObject.params -> { key: 'fps' }.
   * Falls back to null if not found (throughput ratio threshold is then skipped).
   */
  _extractFps() {
    const nodes = this._parseProcessData();
    const detNode = nodes.find((n) => n && n.actionId === FPS_ACTION_ID);
    if (!detNode) return null;
    let configObj = detNode.configObject;
    if (typeof configObj === 'string') {
      try { configObj = JSON.parse(configObj); } catch { return null; }
    }
    const params = configObj?.params;
    if (!Array.isArray(params)) return null;
    const fpsParam = params.find((p) => p?.key === 'fps');
    if (!fpsParam) return null;
    const val = Number(fpsParam.value);
    return Number.isFinite(val) && val > 0 ? val : null;
  }

  _validate() {
    if (!this.scenario?.name) throw new Error('scenario.yml: missing "name"');
    if (!this.scenario?.loadProfile?.length) {
      throw new Error('scenario.yml: loadProfile must have at least one step');
    }
    for (const [i, step] of this.scenario.loadProfile.entries()) {
      if (!Number.isInteger(step.channels) || step.channels < 1) {
        throw new Error(`scenario.yml: loadProfile[${i}].channels must be a positive integer`);
      }
      if (!Number.isFinite(step.holdSec) || step.holdSec <= 0) {
        throw new Error(`scenario.yml: loadProfile[${i}].holdSec must be > 0`);
      }
    }
    if (!this.videos?.mode) throw new Error('videos.yml: missing "mode"');
    this.videoMode;  // re-trigger mode validation
  }
}

// Re-export for unit testing of the fps extractor on arbitrary template objects.
export function extractTargetFps(template) {
  const pkg = Object.create(ScenarioPackage.prototype);
  pkg.template = template;
  return pkg._extractFps();
}
