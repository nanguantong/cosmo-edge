// Emit metrics.json and a self-contained HTML report.
//
// Scenario-task pass/fail is latency-oriented. FPS is kept as a reference only
// because business nodes such as alarm throttling and event push are not
// per-frame stages and must not decide throughput pass/fail.

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

    const html = this._renderHtml(runResult);
    const htmlPath = path.join(this.outputDir, 'report.html');
    fs.writeFileSync(htmlPath, html, 'utf8');

    return { jsonPath, htmlPath };
  }

  async writePartial(runResult) {
    fs.mkdirSync(this.outputDir, { recursive: true });
    const jsonPath = path.join(this.outputDir, 'metrics.partial.json');
    fs.writeFileSync(jsonPath, JSON.stringify(runResult, null, 2), 'utf8');
    return { jsonPath };
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
        detectorLatencyMs: null,
        criticalPathLatencyMs: null,
        channelStats: [],
        perThreshold: [],
        pass: null,
        reasons: ['未执行（瓶颈提前停止）'],
        skipped: true,
      };
    }

    const byChannel = new Map();
    for (const t of ticks) {
      for (const ch of t.channels ?? []) {
        if (ch.missing) continue;
        if (!byChannel.has(ch.channelId)) {
          byChannel.set(ch.channelId, {
            fps: [],
            pipelineMinFps: [],
            discardRate: [],
            detectorLat: [],
            criticalLat: [],
          });
        }
        const s = byChannel.get(ch.channelId);
        if (typeof ch.measuredFps === 'number') s.fps.push(ch.measuredFps);
        if (typeof ch.pipelineMinFps === 'number') s.pipelineMinFps.push(ch.pipelineMinFps);
        if (typeof ch.discardRate === 'number') s.discardRate.push(ch.discardRate);

        const detectorMs = detectorLatencyMs(ch.nodeDurationInfos ?? []);
        const criticalMs = criticalPathLatencyMs(ch.nodeDurationInfos ?? []);
        if (detectorMs != null) s.detectorLat.push(detectorMs);
        if (criticalMs != null) s.criticalLat.push(criticalMs);
      }
    }

    const channelStats = [...byChannel.entries()].map(([channelId, s]) => ({
      channelId,
      avgDetectorFps: s.fps.length ? round(mean(s.fps), 2) : null,
      minDetectorFps: s.fps.length ? round(Math.min(...s.fps), 2) : null,
      minPipelineFps: s.pipelineMinFps.length ? round(Math.min(...s.pipelineMinFps), 2) : null,
      avgDiscardRate: s.discardRate.length ? round(mean(s.discardRate), 4) : null,
      // Latency is aggregated MEAN across the steady ticks for one channel (not max):
      // a single-tick GC/scheduling spike should not pin the whole step's latency. The
      // slowest channel is still surfaced by the per-step max-across-channels below, so
      // a genuinely overloaded channel is never hidden — only transient jitter is.
      avgDetectorLatencyMs: s.detectorLat.length ? round(mean(s.detectorLat), 1) : null,
      avgCriticalPathLatencyMs: s.criticalLat.length ? round(mean(s.criticalLat), 1) : null,
    }));

    const allDetectorFps = channelStats.flatMap((c) => [c.avgDetectorFps, c.minDetectorFps]).filter((f) => f != null);
    const allDiscard = channelStats.map((c) => c.avgDiscardRate).filter((f) => f != null);
    // Per-step latency = max of per-channel MEAN latencies. This keeps the "slowest
    // channel" bottleneck signal (the channel that's really struggling) while each
    // channel's value is already de-spiked by the mean-across-ticks above. Mirrors the
    // discard-rate philosophy: smooth tick-level noise, keep channel-level signal.
    const allDetectorLat = channelStats.map((c) => c.avgDetectorLatencyMs).filter((f) => f != null);
    const allCriticalLat = channelStats.map((c) => c.avgCriticalPathLatencyMs).filter((f) => f != null);

    const targetFps = samples[0]?.channels?.[0]?.targetFps ?? samples[0]?.targetFps ?? null;
    const minFpsAcross = allDetectorFps.length ? Math.min(...allDetectorFps) : null;
    const maxDiscard = allDiscard.length ? Math.max(...allDiscard) : null;
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
        overall.reasons.push(`${name}: ${actual} ${op} ${limit} failed`);
      }
    };

    check('criticalPathLatencyMs', maxCriticalLat, '<=', pass.maxCriticalPathLatencyMs ?? pass.maxAvgNodeLatencyMs);
    check('detectorLatencyMs', maxDetectorLat, '<=', pass.maxDetectorLatencyMs);
    check('maxDiscardRate', maxDiscard, '<=', pass.maxDiscardRate);
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
      detectorLatencyMs: maxDetectorLat,
      criticalPathLatencyMs: maxCriticalLat,
      maxNpu,
      maxCpu,
      maxMem,
      channelStats,
      perThreshold,
      pass: overall.pass,
      reasons: overall.reasons,
    };
  }

  _renderHtml(r) {
    const stepSummaries = r.steps.map((st) =>
      this._summarizeStep(st, r.samples ?? [], r.thresholds ?? {}, r.videoMode ?? 'local'),
    );
    const ranSummaries = stepSummaries.filter((s) => !s.skipped);
    const overallPass = r.status !== 'aborted'
      && ranSummaries.length > 0
      && ranSummaries.every((s) => s.pass);
    const esc = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

    const baseRows = [
      ['场景', r.scenarioName],
      ['算法', `${r.algorithmId} (${esc(r.algorithmName)})`],
      ['编排设定 FPS（参考）', r.targetFps ?? '未提取到'],
      ['视频模式', r.videoMode],
      ['设备', esc(`${r.device?.model ?? ''} / ${r.device?.sn ?? ''}`)],
      ['软件版本', esc(r.device?.softwareVersion ?? '')],
      ['开始时间', r.startedAt],
      ['结束时间', r.endedAt],
      ['总采样点', (r.samples ?? []).length],
      ['基线 FPS（参考）', r.baselineFps != null ? `${r.baselineFps} (step 1, 1ch)` : '-'],
    ];

    const bottleneckBanner = r.bottleneck
      ? `<div style="background:#fef3c7;border:1px solid #f59e0b;padding:10px 14px;border-radius:4px;margin:12px 0">
           <strong>检测到瓶颈</strong>：第 ${r.bottleneck.stepNumber} 阶段（${r.bottleneck.channels} 路）触发提前停止。<br>
           原因：${esc(r.bottleneck.reason)}
         </div>`
      : '';

    const abortedBanner = r.status === 'aborted'
      ? `<div style="background:#fee2e2;border:1px solid #dc2626;padding:10px 14px;border-radius:4px;margin:12px 0">
           <strong>压测中断（部分报告）</strong>：运行在 ${r.error?.atChannels ?? '?'} 路 / 第 ${r.error?.atStepIndex != null ? r.error.atStepIndex + 1 : '?'} 阶段时中断，下方数据仅为中断前已采集部分。<br>
           原因：${esc(r.error?.message ?? '未知错误')}
         </div>`
      : '';

    const stepRows = stepSummaries.map((s) => `
      <tr>
        <td>${s.step.index + 1}</td>
        <td>${s.channels}</td>
        <td>${s.holdSec}s</td>
        <td>${s.targetFps ?? '-'}</td>
        <td>${s.minFpsAcross ?? '-'}</td>
        <td>${s.criticalPathLatencyMs ?? '-'}</td>
        <td>${s.detectorLatencyMs ?? '-'}</td>
        <td>${s.maxDiscard != null ? s.maxDiscard : '-'}</td>
        <td class="${s.maxNpu >= 90 ? 'fail' : ''}">${s.maxNpu != null ? s.maxNpu + '%' : '-'}</td>
        <td class="${s.maxCpu >= 90 ? 'fail' : ''}">${s.maxCpu != null ? s.maxCpu + '%' : '-'}</td>
        <td class="${s.maxMem >= 90 ? 'fail' : ''}">${s.maxMem != null ? s.maxMem + '%' : '-'}</td>
        <td class="${s.skipped ? 'na' : (s.pass ? 'pass' : 'fail')}">${s.skipped ? 'SKIP' : (s.pass ? 'PASS' : 'FAIL')}</td>
        <td>${esc((s.reasons ?? []).join('; '))}</td>
      </tr>`).join('');

    const verdictRows = stepSummaries.flatMap((s) =>
      s.perThreshold.map((t) => `
        <tr>
          <td>step ${s.step.index + 1} (${s.channels}ch)</td>
          <td>${esc(t.name)}</td>
          <td>${t.threshold ?? '-'}</td>
          <td>${t.actual ?? '-'}</td>
          <td class="${t.result === 'PASS' ? 'pass' : (t.result === 'FAIL' ? 'fail' : 'na')}">${t.result}</td>
        </tr>`),
    ).join('');

    return `<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>压测报告 - ${esc(r.scenarioName)}</title>
<style>
  body{font:14px/1.5 -apple-system,Segoe UI,Roboto,sans-serif;margin:24px;color:#1a1a1a}
  h1{font-size:20px} h2{font-size:16px;margin-top:28px;border-bottom:1px solid #ddd;padding-bottom:4px}
  table{border-collapse:collapse;width:100%;margin:8px 0}
  th,td{border:1px solid #ddd;padding:6px 10px;text-align:left}
  th{background:#f5f5f5}.pass{color:#16a34a;font-weight:600}
  .fail{color:#dc2626;font-weight:600}.na{color:#888}
  .badge{display:inline-block;padding:4px 12px;border-radius:4px;color:#fff;font-weight:600}
  .badge.pass{background:#16a34a}.badge.fail{background:#dc2626}
</style></head><body>
<h1>压测报告</h1>
<p>总体结果: <span class="badge ${overallPass ? 'pass' : 'fail'}">${overallPass ? 'PASS' : 'FAIL'}</span>${r.status === 'aborted' ? ' <span class="badge fail">ABORTED</span>' : ''}</p>
${abortedBanner}
${bottleneckBanner}
<h2>基础信息</h2>
<table>${baseRows.map(([k, v]) => `<tr><th>${esc(k)}</th><td>${v}</td></tr>`).join('')}</table>
<h2>阶梯结果</h2>
<table>
  <tr><th>阶梯</th><th>通道数</th><th>保持</th><th>目标FPS(参考)</th><th>检测FPS(参考)</th><th>关键链路延时ms</th><th>检测节点延时ms</th><th>最大丢弃率</th><th>NPU峰值</th><th>CPU峰值</th><th>内存峰值</th><th>结果</th><th>失败原因</th></tr>
  ${stepRows}
</table>
<h2>阈值判定明细</h2>
<table>
  <tr><th>阶梯</th><th>指标</th><th>阈值</th><th>实测</th><th>结果</th></tr>
  ${verdictRows}
</table>
</body></html>`;
  }
}

function mean(arr) { return arr.reduce((a, b) => a + b, 0) / arr.length; }
function round(v, d) { const f = 10 ** d; return Math.round(v * f) / f; }

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
