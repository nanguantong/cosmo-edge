// Emit metrics.json, summary.json, and a self-contained HTML report.

import fs from 'node:fs';
import path from 'node:path';

export class ReportWriter {
  constructor(outputDir) {
    this.outputDir = path.resolve(outputDir);
  }

  async write(runResult) {
    fs.mkdirSync(this.outputDir, { recursive: true });

    const jsonPath = path.join(this.outputDir, 'metrics.json');
    fs.writeFileSync(jsonPath, JSON.stringify(runResult, null, 2), 'utf8');

    const stepSummaries = this._summarizeSteps(runResult);
    const summary = this._buildSummary(runResult, stepSummaries);
    const summaryPath = path.join(this.outputDir, 'summary.json');
    fs.writeFileSync(summaryPath, JSON.stringify(summary, null, 2), 'utf8');

    const html = this._renderHtml(runResult, stepSummaries, summary);
    const htmlPath = path.join(this.outputDir, 'report.html');
    fs.writeFileSync(htmlPath, html, 'utf8');

    return { jsonPath, summaryPath, htmlPath };
  }

  async writePartial(runResult) {
    fs.mkdirSync(this.outputDir, { recursive: true });
    const jsonPath = path.join(this.outputDir, 'metrics.partial.json');
    fs.writeFileSync(jsonPath, JSON.stringify(runResult, null, 2), 'utf8');
    return { jsonPath };
  }

  _summarizeSteps(r) {
    return (r.steps ?? []).map((st) =>
      this._summarizeStep(st, r.samples ?? [], r.thresholds ?? {}, r.videoMode ?? 'local'),
    );
  }

  _summarizeStep(step, samples, thresholds, videoMode) {
    const allTicks = samples.filter((s) => s.stepIndex === step.index);
    const ticks = allTicks.slice(Math.floor(allTicks.length / 2));
    if (!allTicks.length) {
      return {
        step,
        channels: step.channels,
        holdSec: step.holdSec,
        targetFps: null,
        minFpsAcross: null,
        maxDiscard: null,
        avgDiscard: null,
        detectorLatencyMs: null,
        criticalPathLatencyMs: null,
        channelStats: [],
        taskStats: [],
        perThreshold: [],
        pass: null,
        reasons: ['未执行，瓶颈提前停止'],
        skipped: true,
      };
    }

    const byBinding = new Map();
    for (const t of ticks) {
      for (const ch of t.channels ?? []) {
        if (ch.missing) continue;
        const taskKey = ch.taskKey ?? 'default';
        const key = `${taskKey}::${ch.channelId}`;
        if (!byBinding.has(key)) {
          byBinding.set(key, {
            taskKey,
            taskDisplayName: ch.taskDisplayName ?? taskKey,
            taskType: ch.taskType ?? 'cv',
            algorithmId: ch.algorithmId ?? null,
            channelId: ch.channelId,
            fps: [],
            pipelineMinFps: [],
            discardRate: [],
            detectorLat: [],
            criticalLat: [],
          });
        }
        const s = byBinding.get(key);
        if (typeof ch.measuredFps === 'number') s.fps.push(ch.measuredFps);
        if (typeof ch.pipelineMinFps === 'number') s.pipelineMinFps.push(ch.pipelineMinFps);
        if (typeof ch.discardRate === 'number') s.discardRate.push(ch.discardRate);

        const detectorMs = detectorLatencyMs(ch.nodeDurationInfos ?? []);
        const criticalMs = criticalPathLatencyMs(ch.nodeDurationInfos ?? []);
        if (detectorMs != null) s.detectorLat.push(detectorMs);
        if (criticalMs != null) s.criticalLat.push(criticalMs);
      }
    }

    const channelStats = [...byBinding.values()].map((s) => ({
      taskKey: s.taskKey,
      taskDisplayName: s.taskDisplayName,
      taskType: s.taskType,
      algorithmId: s.algorithmId,
      channelId: s.channelId,
      avgDetectorFps: s.fps.length ? round(mean(s.fps), 2) : null,
      minDetectorFps: s.fps.length ? round(Math.min(...s.fps), 2) : null,
      minPipelineFps: s.pipelineMinFps.length ? round(Math.min(...s.pipelineMinFps), 2) : null,
      avgDiscardRate: s.discardRate.length ? round(mean(s.discardRate), 4) : null,
      avgDetectorLatencyMs: s.detectorLat.length ? round(mean(s.detectorLat), 1) : null,
      avgCriticalPathLatencyMs: s.criticalLat.length ? round(mean(s.criticalLat), 1) : null,
    }));

    const allDetectorFps = channelStats.flatMap((c) => [c.avgDetectorFps, c.minDetectorFps]).filter((v) => v != null);
    const allDiscard = channelStats.map((c) => c.avgDiscardRate).filter((v) => v != null);
    const allDetectorLat = channelStats.map((c) => c.avgDetectorLatencyMs).filter((v) => v != null);
    const allCriticalLat = channelStats.map((c) => c.avgCriticalPathLatencyMs).filter((v) => v != null);

    const targetFpsValues = [...new Set(channelStats.map((c) => c.taskKey)
      .flatMap((taskKey) => ticks.flatMap((t) => (t.channels ?? [])
        .filter((ch) => (ch.taskKey ?? 'default') === taskKey)
        .map((ch) => ch.targetFps)
        .filter((v) => v != null))))];
    const targetFps = targetFpsValues.length <= 1 ? (targetFpsValues[0] ?? null) : targetFpsValues.join(' / ');
    const minFpsAcross = allDetectorFps.length ? Math.min(...allDetectorFps) : null;
    const maxDiscard = allDiscard.length ? Math.max(...allDiscard) : null;
    const avgDiscard = allDiscard.length ? round(mean(allDiscard), 4) : null;
    const maxDetectorLat = allDetectorLat.length ? Math.max(...allDetectorLat) : null;
    const maxCriticalLat = allCriticalLat.length ? Math.max(...allCriticalLat) : null;

    const pktDiscard = ticks
      .map((t) => t.hardware?.packetDiscardUtilization?.usedPercent ?? null)
      .filter((v) => v != null);
    const maxPktDiscard = pktDiscard.length ? Math.max(...pktDiscard) / 100 : null;

    const steady = ticks.slice(Math.floor(ticks.length / 2));
    const peak = (sel) => {
      const vals = steady.map(sel).filter((v) => typeof v === 'number');
      return vals.length ? Math.max(...vals) : null;
    };
    const maxNpu = peak((t) => t.hardware?.npuUtilization?.usedPercent);
    const maxCpu = peak((t) => t.hardware?.cpuUtilization?.usedPercent);
    const maxMem = peak((t) => t.hardware?.generalMemoryUtilization?.usedPercent);
    const taskStats = summarizeTasks(channelStats);

    const perThreshold = [];
    const pass = thresholds.pass ?? {};
    const overall = { pass: true, reasons: [] };
    const check = (name, actual, op, limit) => {
      if (actual == null || limit == null) {
        perThreshold.push({ name, threshold: limit, actual: null, result: 'N/A' });
        return;
      }
      const ok = op === '>=' ? actual >= limit : actual <= limit;
      perThreshold.push({ name, threshold: limit, actual, result: ok ? 'PASS' : 'FAIL' });
      if (!ok) {
        overall.pass = false;
        overall.reasons.push(formatThresholdFailure(name, actual, limit));
      }
    };

    check('criticalPathLatencyMs', maxCriticalLat, '<=', pass.maxCriticalPathLatencyMs ?? pass.maxAvgNodeLatencyMs);
    check('detectorLatencyMs', maxDetectorLat, '<=', pass.maxDetectorLatencyMs);
    check('avgDiscardRate', avgDiscard, '<=', pass.avgDiscardRate ?? pass.maxDiscardRate);
    if (videoMode !== 'local') {
      check('maxPacketDiscardRate', maxPktDiscard, '<=', pass.maxPacketDiscardRate);
    }

    return {
      step,
      channels: step.channels,
      holdSec: step.holdSec,
      targetFps,
      minFpsAcross,
      maxDiscard,
      avgDiscard,
      detectorLatencyMs: maxDetectorLat,
      criticalPathLatencyMs: maxCriticalLat,
      maxNpu,
      maxCpu,
      maxMem,
      channelStats,
      taskStats,
      perThreshold,
      pass: overall.pass,
      reasons: overall.reasons,
    };
  }

  _buildSummary(r, stepSummaries) {
    const ran = stepSummaries.filter((s) => !s.skipped);
    const firstFailed = ran.find((s) => s.pass === false) ?? null;
    const bottleneck = r.bottleneck ?? (firstFailed
      ? {
          stepIndex: firstFailed.step.index,
          stepNumber: firstFailed.step.index + 1,
          channels: firstFailed.channels,
          reason: firstFailed.reasons.join('; '),
        }
      : null);
    const hasBottleneck = Boolean(bottleneck);
    const verifiedPassed = ran.filter((s) =>
      s.pass && (!hasBottleneck || s.step.index < bottleneck.stepIndex),
    );
    const maxVerifiedPassedChannels = verifiedPassed.length
      ? Math.max(...verifiedPassed.map((s) => s.channels))
      : null;
    const continuousProfile = r.profileMode === 'capacity' || isContinuousChannelProfile(stepSummaries);
    const maxStableChannels = continuousProfile ? maxVerifiedPassedChannels : null;
    const allRanStepsPass = ran.length > 0 && ran.every((s) => s.pass);
    const capacityMeasured = r.status !== 'aborted'
      && continuousProfile
      && maxVerifiedPassedChannels != null
      && (hasBottleneck || allRanStepsPass);
    const overallPass = r.status !== 'aborted' && allRanStepsPass && !hasBottleneck;

    let conclusion;
    if (r.status === 'aborted') {
      conclusion = `压测中断：运行到 ${r.error?.atChannels ?? '?'} 路时停止，原因：${r.error?.message ?? '未知错误'}`;
    } else if (bottleneck) {
      if (continuousProfile) {
        const failChannels = firstFailed?.channels ?? bottleneck.channels;
        conclusion = `容量上限：${maxStableChannels ?? 0} 路；${failChannels} 路开始不满足通过条件，原因：${(firstFailed?.reasons?.join('; ') || bottleneck.reason)}`;
      } else {
        const upper = firstFailed?.channels ?? bottleneck.channels;
        conclusion = `已验证通过阶梯：${maxVerifiedPassedChannels ?? 0} 路；连续最大稳定路数未精确测定，已知 >= ${maxVerifiedPassedChannels ?? 0} 路且 < ${upper} 路；第 ${bottleneck.stepNumber} 阶段 ${bottleneck.channels} 路触发失败/停止，原因：${bottleneck.reason}`;
      }
    } else if (overallPass) {
      conclusion = continuousProfile
        ? `全部测试路数通过；容量上限至少为 ${maxStableChannels ?? '-'} 路`
        : `全部配置阶梯通过；已验证通过阶梯：${maxVerifiedPassedChannels ?? '-'} 路；连续最大稳定路数未精确测定`;
    } else {
      conclusion = '没有足够的有效采样点形成结论';
    }

    return {
      scenarioName: r.scenarioName,
      algorithmId: r.algorithmId,
      algorithmName: r.algorithmName,
      tasks: r.tasks ?? [],
      targetFps: r.targetFps,
      videoMode: r.videoMode,
      profileMode: r.profileMode ?? 'configured',
      status: r.status,
      overallPass,
      allRanStepsPass,
      hasBottleneck,
      capacityMeasured,
      conclusion,
      maxStableChannels,
      maxStableChannelsExact: continuousProfile,
      maxVerifiedPassedChannels,
      firstFailedStep: firstFailed ? {
        stepIndex: firstFailed.step.index,
        stepNumber: firstFailed.step.index + 1,
        channels: firstFailed.channels,
        reasons: firstFailed.reasons,
      } : null,
      capacityBound: !continuousProfile && firstFailed ? {
        lowerInclusive: maxVerifiedPassedChannels,
        upperExclusive: firstFailed.channels,
      } : null,
      bottleneck,
      baselineFps: r.baselineFps,
      device: r.device,
      startedAt: r.startedAt,
      endedAt: r.endedAt,
      sampleCount: (r.samples ?? []).length,
    };
  }

  _renderHtml(r, stepSummaries, summary) {
    const esc = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
    const pass = r.thresholds?.pass ?? {};
    const sampleInterval = Number(r.samples?.[1]?.ts && r.samples?.[0]?.ts ? ((r.samples[1].ts - r.samples[0].ts) / 1000).toFixed(1) : NaN);
    const samplingText = Number.isFinite(sampleInterval)
      ? `约每 ${Math.round(sampleInterval)}s 采样一次；阶梯汇总使用该阶梯后半段采样点作为稳定窗口。`
      : '阶梯汇总使用该阶梯后半段采样点作为稳定窗口。';
    const profileText = summary.maxStableChannelsExact
      ? '当前按连续路数扫描，可直接给出容量上限。容量上限是最后一个完整执行且通过报告阈值的路数。'
      : `当前阶梯不是连续通道数，只能给出已验证通过阶梯；连续最大稳定路数需在相邻区间内补测。${summary.capacityBound ? `本次已知 >= ${summary.capacityBound.lowerInclusive ?? 0} 路且 < ${summary.capacityBound.upperExclusive} 路。` : ''}`;
    const interpretationRows = [
      ['容量结论', profileText],
      ['路数 PASS/FAIL', `每个路数按 thresholds.yml 的报告阈值判定：关键链路 <= ${pass.maxCriticalPathLatencyMs ?? pass.maxAvgNodeLatencyMs ?? '-'}ms，检测节点 <= ${pass.maxDetectorLatencyMs ?? '-'}ms，平均丢弃率 <= ${pass.avgDiscardRate ?? pass.maxDiscardRate ?? '-'}。`],
      ['瓶颈停止', '运行期保护熔断，用于避免设备继续加压。典型触发条件包括稳定窗口 FPS 相对基线折半、丢弃率连续采样超过 5%、CPU/NPU/内存接近饱和等。它可能发生在下一阶梯 ramp 过程中。'],
      ['采样窗口', samplingText],
      ['失败原因', '表格中的失败原因来自未通过的阈值项；若同时存在瓶颈停止，顶部横幅展示触发提前停止的运行期原因。'],
    ];
    const baseRows = [
      ['场景', r.scenarioName],
      ['任务', formatTaskList(r.tasks, r.algorithmId, r.algorithmName)],
      ['编排设定 FPS（参考）', formatTargetFps(r.tasks, r.targetFps)],
      ['视频模式', r.videoMode],
      ['设备', `${r.device?.model ?? ''} / ${r.device?.sn ?? ''}`],
      ['软件版本', r.device?.softwareVersion ?? ''],
      ['开始时间', r.startedAt],
      ['结束时间', r.endedAt],
      ['总采样点', (r.samples ?? []).length],
      ['基线 FPS（参考）', r.baselineFps != null ? `${r.baselineFps} (step 1, 1ch)` : '-'],
    ];

    const bottleneckBanner = summary.bottleneck
      ? `<div class="banner warn"><strong>检测到瓶颈</strong><br>第 ${summary.bottleneck.stepNumber} 阶段（${summary.bottleneck.channels} 路）触发停止。原因：${esc(summary.bottleneck.reason)}</div>`
      : '';

    const abortedBanner = r.status === 'aborted'
      ? `<div class="banner error"><strong>压测中断（部分报告）</strong><br>运行到 ${r.error?.atChannels ?? '?'} 路 / 第 ${r.error?.atStepIndex != null ? r.error.atStepIndex + 1 : '?'} 阶段时中断。原因：${esc(r.error?.message ?? '未知错误')}</div>`
      : '';

    const runBadge = summary.hasBottleneck
      ? { className: 'warn', label: 'STOPPED' }
      : (summary.overallPass ? { className: 'pass', label: 'PASS' } : { className: 'fail', label: 'FAIL' });
    const stepStatus = (s) => {
      if (s.skipped) return { className: 'na', label: 'SKIP' };
      if (summary.bottleneck?.stepIndex === s.step.index) return { className: 'warn', label: 'STOPPED' };
      return s.pass ? { className: 'pass', label: 'PASS' } : { className: 'fail', label: 'FAIL' };
    };

    const stepRows = stepSummaries.map((s) => {
      const status = stepStatus(s);
      return `
      <tr>
        <td>${s.step.index + 1}</td>
        <td>${s.channels}</td>
        <td>${s.holdSec}s</td>
        <td>${s.targetFps ?? '-'}</td>
        <td>${s.minFpsAcross ?? '-'}</td>
        <td>${s.criticalPathLatencyMs ?? '-'}</td>
        <td>${s.detectorLatencyMs ?? '-'}</td>
        <td>${s.avgDiscard != null ? s.avgDiscard : '-'}</td>
        <td>${s.maxDiscard != null ? s.maxDiscard : '-'}</td>
        <td class="${s.maxNpu >= 90 ? 'fail' : ''}">${s.maxNpu != null ? s.maxNpu + '%' : '-'}</td>
        <td class="${s.maxCpu >= 90 ? 'fail' : ''}">${s.maxCpu != null ? s.maxCpu + '%' : '-'}</td>
        <td class="${s.maxMem >= 90 ? 'fail' : ''}">${s.maxMem != null ? s.maxMem + '%' : '-'}</td>
        <td class="${status.className}">${status.label}</td>
        <td>${esc((s.reasons ?? []).join('; '))}</td>
      </tr>`;
    }).join('');

    const verdictRows = stepSummaries.flatMap((s) =>
      s.perThreshold.map((t) => `
        <tr>
          <td>${s.channels}ch</td>
          <td>${esc(t.name)}</td>
          <td>${t.threshold ?? '-'}</td>
          <td>${t.actual ?? '-'}</td>
          <td class="${t.result === 'PASS' ? 'pass' : (t.result === 'FAIL' ? 'fail' : 'na')}">${t.result}</td>
        </tr>`),
    ).join('');

    const taskRows = stepSummaries.flatMap((s) =>
      (s.taskStats ?? []).map((t) => `
        <tr>
          <td>${s.channels}ch</td>
          <td>${esc(t.taskDisplayName ?? t.taskKey)}</td>
          <td>${t.algorithmId ?? '-'}</td>
          <td>${t.bindingCount}</td>
          <td>${t.minDetectorFps ?? '-'}</td>
          <td>${t.avgDiscardRate ?? '-'}</td>
          <td>${t.maxDetectorLatencyMs ?? '-'}</td>
          <td>${t.maxCriticalPathLatencyMs ?? '-'}</td>
        </tr>`),
    ).join('');

    return `<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>压测报告 - ${esc(r.scenarioName)}</title>
<style>
  body{font:14px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;margin:24px;color:#1a1a1a}
  h1{font-size:20px} h2{font-size:16px;margin-top:28px;border-bottom:1px solid #ddd;padding-bottom:4px}
  table{border-collapse:collapse;width:100%;margin:8px 0}
  th,td{border:1px solid #ddd;padding:6px 10px;text-align:left;vertical-align:top}
  th{background:#f5f5f5}.pass{color:#16a34a;font-weight:600}
  .fail{color:#dc2626;font-weight:600}.na{color:#888}
  .badge{display:inline-block;padding:4px 12px;border-radius:4px;color:#fff;font-weight:600}
  .badge.pass{background:#16a34a}.badge.fail{background:#dc2626}.badge.warn{background:#f59e0b}
  .summary{background:#f8fafc;border:1px solid #cbd5e1;padding:12px 14px;border-radius:4px;margin:12px 0}
  .banner{padding:10px 14px;border-radius:4px;margin:12px 0}.warn{background:#fef3c7;border:1px solid #f59e0b}.error{background:#fee2e2;border:1px solid #dc2626}
  .note-table th{width:140px}
</style></head><body>
<h1>压测报告</h1>
<p>总体结果: <span class="badge ${runBadge.className}">${runBadge.label}</span>${r.status === 'aborted' ? ' <span class="badge fail">ABORTED</span>' : ''}</p>
<div class="summary"><strong>结论</strong><br>${esc(summary.conclusion)}</div>
${abortedBanner}
${bottleneckBanner}
<h2>判定口径</h2>
<table class="note-table">${interpretationRows.map(([k, v]) => `<tr><th>${esc(k)}</th><td>${esc(v)}</td></tr>`).join('')}</table>
<h2>基础信息</h2>
<table>${baseRows.map(([k, v]) => `<tr><th>${esc(k)}</th><td>${esc(v)}</td></tr>`).join('')}</table>
<h2>路数结果</h2>
<table>
  <tr><th>序号</th><th>路数</th><th>保持</th><th>目标FPS(参考)</th><th>检测FPS(参考)</th><th>关键链路延时ms</th><th>检测节点延时ms</th><th>平均丢弃率</th><th>最差通道丢弃率</th><th>NPU峰值</th><th>CPU峰值</th><th>内存峰值</th><th>结果</th><th>失败原因</th></tr>
  ${stepRows}
</table>
<h2>分任务汇总</h2>
<table>
  <tr><th>路数</th><th>任务</th><th>算法ID</th><th>绑定数</th><th>最低检测FPS</th><th>平均丢弃率</th><th>最大检测延时ms</th><th>最大关键链路ms</th></tr>
  ${taskRows}
</table>
<h2>阈值判定明细</h2>
<table>
  <tr><th>路数</th><th>指标</th><th>阈值</th><th>实测</th><th>结果</th></tr>
  ${verdictRows}
</table>
</body></html>`;
  }
}

function mean(arr) { return arr.reduce((a, b) => a + b, 0) / arr.length; }
function round(v, d) { const f = 10 ** d; return Math.round(v * f) / f; }

function summarizeTasks(channelStats) {
  const byTask = new Map();
  for (const stat of channelStats) {
    const key = stat.taskKey ?? 'default';
    if (!byTask.has(key)) {
      byTask.set(key, {
        taskKey: key,
        taskDisplayName: stat.taskDisplayName ?? key,
        taskType: stat.taskType ?? 'cv',
        algorithmId: stat.algorithmId ?? null,
        fps: [],
        discard: [],
        detectorLat: [],
        criticalLat: [],
        bindingCount: 0,
      });
    }
    const task = byTask.get(key);
    task.bindingCount++;
    if (stat.minDetectorFps != null) task.fps.push(stat.minDetectorFps);
    if (stat.avgDiscardRate != null) task.discard.push(stat.avgDiscardRate);
    if (stat.avgDetectorLatencyMs != null) task.detectorLat.push(stat.avgDetectorLatencyMs);
    if (stat.avgCriticalPathLatencyMs != null) task.criticalLat.push(stat.avgCriticalPathLatencyMs);
  }

  return [...byTask.values()].map((task) => ({
    taskKey: task.taskKey,
    taskDisplayName: task.taskDisplayName,
    taskType: task.taskType,
    algorithmId: task.algorithmId,
    bindingCount: task.bindingCount,
    minDetectorFps: task.fps.length ? round(Math.min(...task.fps), 2) : null,
    avgDiscardRate: task.discard.length ? round(mean(task.discard), 4) : null,
    maxDetectorLatencyMs: task.detectorLat.length ? round(Math.max(...task.detectorLat), 1) : null,
    maxCriticalPathLatencyMs: task.criticalLat.length ? round(Math.max(...task.criticalLat), 1) : null,
  }));
}

function formatTaskList(tasks, legacyAlgorithmId, legacyAlgorithmName) {
  if (!Array.isArray(tasks) || !tasks.length) {
    return `${legacyAlgorithmId ?? '-'} (${legacyAlgorithmName ?? '-'})`;
  }
  return tasks
    .map((task) => `${task.id}: ${task.algorithmId}${task.displayName ? ` (${task.displayName})` : ''}`)
    .join('; ');
}

function formatTargetFps(tasks, legacyTargetFps) {
  if (!Array.isArray(tasks) || !tasks.length) {
    return legacyTargetFps ?? '未提取到';
  }
  return tasks.map((task) => `${task.id}=${task.targetFps ?? 'N/A'}`).join('; ');
}

function isContinuousChannelProfile(stepSummaries) {
  const channels = stepSummaries
    .map((s) => s.channels)
    .filter((v) => Number.isInteger(v))
    .sort((a, b) => a - b);
  if (!channels.length || channels[0] !== 1) return false;
  for (let i = 1; i < channels.length; i++) {
    if (channels[i] !== channels[i - 1] + 1) return false;
  }
  return true;
}

function formatThresholdFailure(name, actual, limit) {
  const label = {
    criticalPathLatencyMs: '关键链路延时',
    detectorLatencyMs: '检测节点延时',
    avgDiscardRate: '平均丢弃率',
    maxDiscardRate: '丢弃率',
    maxPacketDiscardRate: '网络丢包率',
  }[name] ?? name;

  if (name === 'avgDiscardRate' || name === 'maxDiscardRate' || name === 'maxPacketDiscardRate') {
    return `${label} ${formatPercent(actual)}，超过阈值 ${formatPercent(limit)}`;
  }
  if (name.endsWith('LatencyMs')) {
    return `${label} ${actual}ms，超过阈值 ${limit}ms`;
  }
  return `${label} ${actual}，未满足阈值 ${limit}`;
}

function formatPercent(v) {
  return `${round(Number(v) * 100, 2)}%`;
}

function nodeAvgUs(n) {
  if (n?.durationAvgUs != null) return Number(n.durationAvgUs);
  if (n?.durationMs != null) return Number(n.durationMs) * 1000;
  return Number(n?.duration ?? 0);
}

function nodeName(n) {
  return String(n?.name ?? '').toLowerCase();
}

function nodeLatencyMs(n) {
  const us = nodeAvgUs(n);
  return Number.isFinite(us) && us > 0 ? us / 1000 : null;
}

function detectorLatencyMs(nodes) {
  const matched = nodes
    .filter((n) => {
      const name = nodeName(n);
      return name.includes('aidetector') || name.includes('detector') || name.includes('检测');
    })
    .map(nodeLatencyMs)
    .filter((v) => v != null);
  return matched.length ? Math.max(...matched) : null;
}

function criticalPathLatencyMs(nodes) {
  const matched = nodes
    .filter((n) => {
      const name = nodeName(n);
      return name.includes('decode')
        || name.includes('aidetector')
        || name.includes('detector')
        || name.includes('检测')
        || name.includes('追踪')
        || name.includes('track')
        || name.includes('分类')
        || name.includes('classif')
        || name.includes('目标判断')
        || name.includes('judge');
    })
    .map(nodeLatencyMs)
    .filter((v) => v != null);
  return matched.length ? matched.reduce((a, b) => a + b, 0) : null;
}
