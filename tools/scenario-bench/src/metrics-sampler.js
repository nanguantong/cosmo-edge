// metrics-sampler.js — Collect RunningDetail + HardwareResource each tick.
//
// Response shapes (verified against source DTOs):
//   RunningDetail: resData.status[] where each item has
//     { taskId, channelId, actionStatus[], nodeDurationInfos[] }
//     actionStatus[]: { actionId, name, statusCode, holdCount, alarmCount,
//       insertCount, processCount, discardCount, periodMs,
//       insertCountPeriod, processCountPeriod, discardCountPeriod }
//   HardwareResource: resData.itemList[] where each item is
//     { key, name, usedPercent, usedSize, unusedSize, available }
//     key in { cpuUtilization, generalMemoryUtilization, npuUtilization,
//       modelMemoryUtilization, pictureMemoryUtilization, TPPMemoryUtilization,
//       eMMCUtilization, packetDiscardUtilization }
//
// NOTE: RunningDetail silently drops tasks whose action count <= 2. We detect
// such "sampling-missing" channels by diffing expected taskIds vs returned ones.

const HW_KEYS = [
  'cpuUtilization',
  'generalMemoryUtilization',
  'npuUtilization',
  'modelMemoryUtilization',
  'pictureMemoryUtilization',
  'TPPMemoryUtilization',
  'eMMCUtilization',
  'packetDiscardUtilization',
];

export class MetricsSampler {
  /**
   * @param {import('./cosmo-client.js').CosmoClient} client
   * @param {import('./logger.js').Logger} [logger]
   */
  constructor(client, logger) {
    this.client = client;
    this.log = logger;
  }

  /**
   * Take one sample for the currently active channels.
   * @param {Map<string,string>} channelTaskMap  channelId -> taskId (active set)
   * @param {number} targetFps  orchestration fps baseline (for computed FPS ratio)
   * @returns {Promise<object>} a sample record
   */
  async sample(channelTaskMap, targetFps) {
    const ts = Date.now();
    const activeChannelIds = [...channelTaskMap.keys()];
    const activeTaskIds = [...channelTaskMap.values()];

    // Sample both endpoints in parallel; either may fail independently.
    const [taskDetail, hwRes] = await Promise.allSettled([
      activeTaskIds.length ? this.client.taskRunningDetail(activeTaskIds) : Promise.resolve({ status: [] }),
      this.client.queryHardwareResource(),
    ]);

    const perChannel = this._parseRunningDetail(
      taskDetail, activeChannelIds, channelTaskMap, targetFps,
    );
    const hw = this._parseHardware(hwRes);

    return {
      ts,
      iso: new Date(ts).toISOString(),
      activeChannels: activeChannelIds.length,
      channels: perChannel,
      hardware: hw,
    };
  }

  // ── RunningDetail parsing ──────────────────────────────────────────────

  _parseRunningDetail(detailResult, activeChannelIds, channelTaskMap, targetFps) {
    const out = [];
    if (detailResult.status !== 'fulfilled') {
      // Whole RunningDetail call failed — mark every channel as errored.
      for (const chId of activeChannelIds) {
        out.push({ channelId: chId, taskId: channelTaskMap.get(chId), error: String(detailResult.reason?.message ?? detailResult.reason), missing: true });
      }
      return out;
    }

    const statusList = detailResult.value?.status ?? [];
    const returnedByTaskId = new Map(statusList.map((s) => [s.taskId, s]));

    for (const chId of activeChannelIds) {
      const taskId = channelTaskMap.get(chId);
      const st = returnedByTaskId.get(taskId);
      if (!st) {
        // Not returned -> either action count <= 2 (silent filter) or task not started yet.
        out.push({ channelId: chId, taskId, missing: true });
        continue;
      }
      out.push(this._summarizeTask(st, targetFps));
    }
    return out;
  }

  /**
   * Aggregate a task's actions into per-channel throughput.
   * FPS uses the slowest effective action rate instead of summing pipeline nodes,
   * otherwise multi-node graphs overcount the channel throughput.
   */
  _summarizeTask(st, targetFps) {
    const actions = Array.isArray(st.actionStatus) ? st.actionStatus : [];
    let insertPeriod = 0, processPeriod = 0, discardPeriod = 0;
    let periodMs = 0, holdCount = 0, alarmCount = 0;
    let insertTotal = 0, processTotal = 0, discardTotal = 0;
    let pipelineMinFps = Infinity;
    let primaryFps = null;
    let maxDiscardRate = 0;
    const actionSummaries = [];

    for (const a of actions) {
      const actionInsertPeriod = num(a.insertCountPeriod);
      const actionProcessPeriod = num(a.processCountPeriod);
      const actionDiscardPeriod = num(a.discardCountPeriod);
      const actionPeriodMs = num(a.periodMs);

      insertPeriod += actionInsertPeriod;
      processPeriod += actionProcessPeriod;
      discardPeriod += actionDiscardPeriod;
      periodMs = Math.max(periodMs, actionPeriodMs);  // pick the longest window
      holdCount += num(a.holdCount);
      alarmCount += num(a.alarmCount);
      insertTotal += num(a.insertCount);
      processTotal += num(a.processCount);
      discardTotal += num(a.discardCount);

      const actionFps = actionPeriodMs > 0 ? (actionProcessPeriod * 1000) / actionPeriodMs : null;
      const actionName = String(a.name ?? '');
      const actionId = String(a.actionId ?? '');
      actionSummaries.push({
        actionId,
        name: actionName,
        fps: actionFps != null ? round(actionFps, 2) : null,
        insertPeriod: actionInsertPeriod,
        processPeriod: actionProcessPeriod,
        discardPeriod: actionDiscardPeriod,
        periodMs: actionPeriodMs,
      });

      if (actionFps != null && actionProcessPeriod > 0) {
        pipelineMinFps = Math.min(pipelineMinFps, actionFps);
        if (primaryFps == null && isPrimaryThroughputAction(actionName, actionId)) {
          primaryFps = actionFps;
        }
      }
      const actionDiscardDen = actionInsertPeriod + actionDiscardPeriod;
      if (actionDiscardDen > 0) {
        maxDiscardRate = Math.max(maxDiscardRate, actionDiscardPeriod / actionDiscardDen);
      }
    }

    if (primaryFps == null) {
      const firstEffective = actionSummaries.find((a) => a.fps != null && a.processPeriod > 0);
      primaryFps = firstEffective?.fps ?? null;
    }
    const measuredFps = primaryFps ?? 0;
    const minPipelineFps = pipelineMinFps !== Infinity ? pipelineMinFps : 0;
    const discardRate = maxDiscardRate;
    const fpsRatio = targetFps && targetFps > 0 ? measuredFps / targetFps : null;

    return {
      channelId: st.channelId,
      taskId: st.taskId,
      algorithmName: st.algorithmName,
      algorithmVersion: st.algorithmVersion,
      actionCount: actions.length,
      measuredFps: round(measuredFps, 2),
      pipelineMinFps: round(minPipelineFps, 2),
      targetFps,
      fpsRatio: fpsRatio != null ? round(fpsRatio, 3) : null,
      discardRate: round(discardRate, 4),
      insertPeriod, processPeriod, discardPeriod, periodMs,
      insertTotal, processTotal, discardTotal,
      holdCount, alarmCount,
      actionSummaries,
      nodeDurationInfos: Array.isArray(st.nodeDurationInfos) ? st.nodeDurationInfos : [],
    };
  }

  // ── HardwareResource parsing ───────────────────────────────────────────

  _parseHardware(hwResult) {
    const hw = {};
    if (hwResult.status !== 'fulfilled') {
      hw._error = String(hwResult.reason?.message ?? hwResult.reason);
      return hw;
    }
    const itemList = hwResult.value?.itemList ?? [];
    const byKey = new Map(itemList.map((it) => [it.key, it]));
    for (const key of HW_KEYS) {
      const it = byKey.get(key);
      if (it) {
        hw[key] = { usedPercent: num(it.usedPercent), usedSize: it.usedSize, unusedSize: it.unusedSize };
      }
    }
    hw.customScore = hwResult.value?.customScore ?? null;
    return hw;
  }
}

function num(v) {
  const n = Number(v);
  return Number.isFinite(n) ? n : 0;
}

function round(v, digits) {
  const f = 10 ** digits;
  return Math.round(v * f) / f;
}

function isPrimaryThroughputAction(name, actionId) {
  const n = String(name).toLowerCase();
  const id = String(actionId);
  return n.includes('aidetector')
    || n.includes('detector')
    || n.includes('检测')
    || id.startsWith('1001');
}
