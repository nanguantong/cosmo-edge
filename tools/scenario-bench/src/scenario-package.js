// scenario-package.js - Parse a scenario directory into a normalized workload.
//
// v1 packages remain supported:
//   algorithm-template.json + scenario.yml + thresholds.yml + videos.yml
//
// v2 packages model the generic case directly:
//   scenario.yml:
//     version: 2
//     channels: { mode, repeatCount, sources: [...] }
//     tasks: [{ id, type, algorithmId, scheduleId, template }]
//     bindings: [{ task, channels }]
//
// Internally both forms become:
//   tasks[] + bindings[] + videos + thresholds + loadProfile.

import fs from 'node:fs';
import path from 'node:path';
import { parse as parseYaml } from 'yaml';
import { normalizeTaskType } from './task-strategies.js';

const FPS_ACTION_ID = 'AA_00001';
const SUPPORTED_VIDEO_MODES = new Set(['local', 'rtsp-fidelity', 'rtsp-deterministic']);

export class ScenarioPackage {
  /**
   * @param {string} dir absolute or relative path to the scenario directory.
   */
  constructor(dir) {
    this.dir = path.resolve(dir);
    this.scenario = null;
    this.thresholds = null;
    this.videos = null;
    this.channelConfig = null;
    this.tasks = [];
    this.bindings = [];
    this.version = 1;

    // Legacy accessors keep old callers and reports working.
    this.template = null;
    this.targetFps = null;
  }

  /** Load and validate the package. Returns this for chaining. */
  load() {
    this.scenario = parseYaml(this._readText('scenario.yml')) ?? {};
    this.version = Number(this.scenario.version ?? (Array.isArray(this.scenario.tasks) ? 2 : 1));
    this.thresholds = this._readOptionalYaml('thresholds.yml') ?? this.scenario.thresholds ?? {};

    if (this.version >= 2 || Array.isArray(this.scenario.tasks)) {
      this._loadWorkloadV2();
    } else {
      this._loadLegacyV1();
    }

    this._resolveVideoPaths();
    this._validate();

    this.template = this.primaryTask?.template ?? null;
    this.targetFps = this.primaryTask?.targetFps ?? null;
    return this;
  }

  // -- normalized accessors -------------------------------------------------

  get primaryTask() {
    return this.tasks[0] ?? null;
  }

  /** algorithmId as a string (legacy single-task accessor). */
  get algorithmId() {
    return this.primaryTask?.algorithmId ?? '';
  }

  get scheduleId() {
    return this.primaryTask?.scheduleId ?? '';
  }

  get loadProfile() {
    return this.scenario.loadProfile ?? [];
  }

  get sampleIntervalSec() {
    return Number(this.scenario.sampleIntervalSec ?? 5);
  }

  get videoMode() {
    const m = this.videos?.mode;
    if (!SUPPORTED_VIDEO_MODES.has(m)) {
      throw new Error(`videos/channels: unsupported mode "${m}"`);
    }
    return m;
  }

  get features() {
    return this.scenario.features ?? {};
  }

  /**
   * Local video repeat count. 0 = loop forever (AlgChannelDemux treats
   * video_repeat_count_ <= 0 as unlimited), 1 = play once then stop.
   */
  get videoRepeatCount() {
    const v = this.channelConfig?.repeatCount ?? this.scenario.videoRepeatCount;
    if (v == null) return this.videoMode === 'local' ? 0 : 1;
    return Number(v);
  }

  /** Legacy single-task taskConfig accessor. */
  get taskConfig() {
    return this.primaryTask?.taskConfig ?? { params: [], areas: [] };
  }

  /** Legacy single-task layout save payload accessor. */
  get layoutSavePayload() {
    if (!this.primaryTask) throw new Error('workload: no tasks defined');
    return this.primaryTask.layoutSavePayload;
  }

  /** Payloads for all task templates that need layout/save. */
  get layoutSavePayloads() {
    return this.tasks.map((task) => ({
      taskId: task.id,
      displayName: task.displayName,
      algorithmId: task.algorithmId,
      payload: task.layoutSavePayload,
    }));
  }

  // -- loaders --------------------------------------------------------------

  _loadLegacyV1() {
    const templateFile = 'algorithm-template.json';
    const template = this._readJson(templateFile);
    this.videos = parseYaml(this._readText('videos.yml')) ?? {};
    this.channelConfig = { mode: this.videos.mode, repeatCount: this.scenario.videoRepeatCount };

    const task = this._normalizeTask({
      id: 'default',
      displayName: this.scenario.displayName ?? this.scenario.name,
      type: this.scenario.taskType ?? 'cv',
      algorithmId: this.scenario.algorithmId,
      algorithmCode: this.scenario.algorithmCode,
      scheduleId: this.scenario.scheduleId,
      template: templateFile,
    }, template, 0);

    this.tasks = [task];
    this.bindings = [{ taskId: task.id, channels: 'all' }];
  }

  _loadWorkloadV2() {
    if (this.scenario.channels) {
      this.channelConfig = this.scenario.channels;
      this.videos = this._videosFromChannels(this.scenario.channels);
    } else {
      this.videos = parseYaml(this._readText('videos.yml')) ?? {};
      this.channelConfig = { mode: this.videos.mode, repeatCount: this.scenario.videoRepeatCount };
    }

    if (!Array.isArray(this.scenario.tasks) || !this.scenario.tasks.length) {
      throw new Error('scenario.yml: version 2 workload must define tasks[]');
    }

    this.tasks = this.scenario.tasks.map((taskSpec, index) => {
      const templateFile = taskSpec.template ?? (index === 0 ? 'algorithm-template.json' : null);
      if (!templateFile) {
        throw new Error(`scenario.yml: tasks[${index}].template is required`);
      }
      const template = this._readJson(templateFile);
      return this._normalizeTask(taskSpec, template, index);
    });

    this.bindings = this._normalizeBindings(this.scenario.bindings);
  }

  _videosFromChannels(channels) {
    const mode = channels.mode;
    const sources = channels.sources ?? channels.local ?? channels.rtsp ?? [];
    const videos = { mode };
    if (mode === 'local') {
      videos.local = sources;
    } else {
      videos.rtsp = sources;
    }
    return videos;
  }

  _normalizeTask(spec, template, index) {
    const algorithmId = String(spec.algorithmId ?? template.algorithmId ?? template.id ?? template.algorithmCode ?? '');
    if (!algorithmId) {
      throw new Error(`scenario.yml: tasks[${index}].algorithmId cannot be derived`);
    }

    const id = String(spec.id ?? spec.name ?? `task-${index + 1}`);
    const algorithmCode = String(spec.algorithmCode ?? template.algorithmCode ?? template.algorithmId ?? algorithmId);
    const scheduleId = spec.scheduleId ?? this.scenario.scheduleId ?? '';
    const targetFps = spec.targetFps != null ? Number(spec.targetFps) : extractTargetFpsFromTemplate(template);
    const taskConfig = buildTaskConfig(template, this.videoRepeatCount);

    return {
      id,
      displayName: spec.displayName ?? template.algorithmName ?? id,
      type: spec.type ?? 'cv',
      algorithmId,
      algorithmCode,
      scheduleId,
      templateFile: spec.template ?? (index === 0 ? 'algorithm-template.json' : null),
      template,
      targetFps: Number.isFinite(targetFps) && targetFps > 0 ? targetFps : null,
      taskConfig,
      layoutSavePayload: buildLayoutSavePayload(template),
    };
  }

  _normalizeBindings(bindings) {
    if (!bindings?.length) {
      return this.tasks.map((task) => ({ taskId: task.id, channels: 'all' }));
    }

    return bindings.map((binding, index) => {
      const taskId = String(binding.task ?? binding.taskId ?? '');
      if (!taskId) throw new Error(`scenario.yml: bindings[${index}].task is required`);
      return {
        taskId,
        channels: binding.channels ?? 'all',
      };
    });
  }

  // -- file helpers ---------------------------------------------------------

  _readText(name) {
    const p = path.isAbsolute(name) ? name : path.join(this.dir, name);
    if (!fs.existsSync(p)) {
      throw new Error(`Missing scenario file: ${p}`);
    }
    return fs.readFileSync(p, 'utf8');
  }

  _readJson(name) {
    return JSON.parse(this._readText(name));
  }

  _readOptionalYaml(name) {
    const p = path.join(this.dir, name);
    if (!fs.existsSync(p)) return null;
    return parseYaml(fs.readFileSync(p, 'utf8')) ?? {};
  }

  /**
   * Resolve relative file entries against the scenario directory, so the
   * channel manager receives absolute paths.
   */
  _resolveVideoPaths() {
    if (this.videos?.mode !== 'local' || !Array.isArray(this.videos.local)) return;
    for (const src of this.videos.local) {
      if (src.file && !path.isAbsolute(src.file)) {
        src.file = path.resolve(this.dir, src.file);
      }
    }
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
    if (!this.videos?.mode) throw new Error('videos/channels: missing "mode"');
    this.videoMode; // re-trigger mode validation
    if (this.videoMode === 'local' && !this.videos.local?.length) {
      throw new Error('videos/channels: local mode requires at least one source');
    }
    if (this.videoMode !== 'local' && !this.videos.rtsp?.length) {
      throw new Error('videos/channels: rtsp mode requires at least one source');
    }

    if (!this.tasks.length) throw new Error('scenario.yml: at least one task is required');
    const seen = new Set();
    for (const task of this.tasks) {
      if (seen.has(task.id)) throw new Error(`scenario.yml: duplicate task id "${task.id}"`);
      seen.add(task.id);
      if (!task.scheduleId) throw new Error(`scenario.yml: task "${task.id}" missing scheduleId`);
      if (normalizeTaskType(task.type) === 'vlm' && task.targetFps == null) {
        throw new Error(`scenario.yml: VLM task "${task.id}" must define targetFps, for example targetFps: 0.2`);
      }
    }

    const taskIds = new Set(this.tasks.map((t) => t.id));
    for (const binding of this.bindings) {
      if (!taskIds.has(binding.taskId)) {
        throw new Error(`scenario.yml: binding references unknown task "${binding.taskId}"`);
      }
    }
  }
}

function buildTaskConfig(template, videoRepeatCount) {
  const base = template.taskConfig ?? { params: [], areas: [] };
  const params = Array.isArray(base.params) ? [...base.params] : [];
  const hasRepeat = params.some((p) => p?.key === 'param.videoRepeatCount');
  if (!hasRepeat) {
    params.push({ key: 'param.videoRepeatCount', value: String(videoRepeatCount) });
  }
  return { ...base, params, areas: base.areas ?? [] };
}

function buildLayoutSavePayload(template) {
  const algorithmId = String(template.algorithmId ?? template.id ?? template.algorithmCode ?? '');
  if (!algorithmId) throw new Error('template: cannot derive algorithmId for layout save');
  const str = (v) => (v == null ? undefined : String(v));
  return {
    confVersionId: template.confVersionId,
    algorithmId,
    configVersionName: str(template.configVersionName ?? template.confVersionName),
    algorithmCategory: str(template.algorithmCategory),
    algorithmUsage: str(template.algorithmUsage),
    remark: str(template.remark),
    atomicList: str(template.atomicList),
    algorithmProcessdata: str(template.algorithmProcessdata),
    algorithmMetadata: str(template.algorithmMetadata),
    filePath: str(template.filePath),
  };
}

function parseProcessData(template) {
  const raw = template.algorithmProcessdata;
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

function extractTargetFpsFromTemplate(template) {
  const nodes = parseProcessData(template);
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

// Re-export for unit testing of the fps extractor on arbitrary template objects.
export function extractTargetFps(template) {
  return extractTargetFpsFromTemplate(template);
}
