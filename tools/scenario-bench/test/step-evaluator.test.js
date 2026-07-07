import assert from 'node:assert/strict';
import test from 'node:test';

import { summarizeStep, runtimeStepDecision } from '../src/step-evaluator.js';

test('runtime decision stops CV tasks when steady throughput falls below half baseline', () => {
  const decision = runtimeStepDecision({
    avgDiscard: 0,
    taskStats: [{
      taskKey: 'cv',
      taskDisplayName: 'cv',
      taskType: 'cv',
      minThroughputFps: 4.9,
      avgDiscardRate: 0,
    }],
  }, {
    baselineByTask: { cv: 10 },
    fpsHalveRatio: 0.5,
  });

  assert.equal(decision.stop, true);
  assert.match(decision.reason, /cv fps 4\.9 < baseline 10\.0\*0\.5/);
});

test('runtime decision stops VLM tasks by configured FPS ratio instead of CV baseline fuse', () => {
  const decision = runtimeStepDecision({
    avgDiscard: 0,
    taskStats: [{
      taskKey: 'vlm',
      taskDisplayName: 'vlm',
      taskType: 'vlm',
      minThroughputFps: 0.6,
      minFpsRatio: 0.7,
      avgDiscardRate: 0,
    }],
  }, {
    baselineByTask: { vlm: 2 },
    thresholds: {
      taskTypes: {
        vlm: { minFpsRatio: 0.8 },
      },
    },
  });

  assert.equal(decision.stop, true);
  assert.match(decision.reason, /vlm fpsRatio 0\.700 < 0\.8/);
  assert.doesNotMatch(decision.reason, /baseline/);
});

test('VLM step summary uses window counter throughput instead of minimum instant delta', () => {
  const samples = [0, 1, 2, 3, 4, 5].map((index) => ({
    stepIndex: 0,
    ts: index * 10_000,
    channels: [{
      taskKey: 'vlm',
      taskDisplayName: 'vlm',
      taskType: 'vlm',
      algorithmId: '55009',
      channelId: 'ch1',
      targetFps: 0.1,
      measuredFps: 0,
      fpsRatio: 0,
      primaryProcessTotal: 100 + Math.max(0, index - 2),
      discardRate: 0,
      telemetryMissing: false,
      nodeDurationInfos: [
        { name: 'Qwen3VLWorker', durationAvgUs: 1_200_000 },
      ],
    }],
    hardware: {},
  }));

  const summary = summarizeStep(
    { index: 0, channels: 1, holdSec: 60 },
    samples,
    { taskTypes: { vlm: { minFpsRatio: 0.8, maxMissingRate: 0 } } },
    'local',
  );

  assert.equal(summary.pass, true);
  assert.equal(summary.channelStats[0].windowThroughputFps, 0.1);
  assert.equal(summary.taskStats[0].minFpsRatio, 1);
});
