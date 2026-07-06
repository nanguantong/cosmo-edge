import assert from 'node:assert/strict';
import test from 'node:test';

import { runtimeStepDecision } from '../src/step-evaluator.js';

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
