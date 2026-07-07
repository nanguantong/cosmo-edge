import assert from 'node:assert/strict';
import test from 'node:test';

import { ReportWriter } from '../src/report-writer.js';

test('capacity conclusion uses bottleneck step when an earlier step has a report failure', () => {
  const writer = new ReportWriter('.');
  const steps = [
    { step: { index: 0 }, channels: 1, pass: false, reasons: ['warmup window below target'] },
    { step: { index: 1 }, channels: 2, pass: true, reasons: [] },
    { step: { index: 2 }, channels: 3, pass: true, reasons: [] },
    { step: { index: 3 }, channels: 4, pass: true, reasons: [] },
    { step: { index: 4 }, channels: 5, pass: true, reasons: [] },
    { step: { index: 5 }, channels: 6, pass: true, reasons: [] },
    { step: { index: 6 }, channels: 7, pass: false, reasons: ['fpsRatio 0.661 < 0.8'] },
  ];

  const summary = writer._buildSummary({
    status: 'completed',
    profileMode: 'capacity',
    bottleneck: {
      stepIndex: 6,
      stepNumber: 7,
      channels: 7,
      reason: 'fpsRatio 0.661 < 0.8',
    },
  }, steps);

  assert.equal(summary.maxStableChannels, 6);
  assert.match(summary.conclusion, /容量上限：6 路/);
  assert.match(summary.conclusion, /第 7 阶段/);
  assert.doesNotMatch(summary.conclusion, /1 路开始/);
});

test('HTML rendering formats task percentage fields', () => {
  const writer = new ReportWriter('.');
  const html = writer._renderHtml({
    scenarioName: 'vlm',
    tasks: [{ id: 'vlm', type: 'vlm', algorithmId: '77175', targetFps: 0.1 }],
    videoMode: 'local',
    status: 'completed',
    thresholds: { pass: {} },
    samples: [{ ts: 0 }, { ts: 3000 }],
  }, [{
    step: { index: 0 },
    channels: 1,
    holdSec: 60,
    targetFps: 0.1,
    minFpsAcross: 0.1,
    criticalPathLatencyMs: 1200,
    detectorLatencyMs: 1200,
    avgDiscard: 0,
    maxDiscard: 0,
    maxNpu: 90,
    maxCpu: 3,
    maxMem: 55,
    pass: true,
    reasons: [],
    perThreshold: [],
    taskStats: [{
      taskKey: 'vlm',
      taskDisplayName: 'vlm',
      strategy: 'vlm',
      algorithmId: '77175',
      bindingCount: 1,
      minThroughputFps: 0.1,
      minFpsRatio: 1,
      maxMissingRate: 0,
      avgDiscardRate: 0,
      maxPrimaryLatencyMs: 1200,
      maxCriticalPathLatencyMs: 1200,
    }],
  }], {
    overallPass: true,
    hasBottleneck: false,
    maxStableChannelsExact: true,
    conclusion: 'ok',
  });

  assert.match(html, /100%/);
  assert.match(html, /0%/);
});
