import assert from 'node:assert/strict';
import test from 'node:test';

import { MetricsSampler } from '../src/metrics-sampler.js';
import { ReportWriter } from '../src/report-writer.js';
import { detectVlmMode, extractTargetFps } from '../src/scenario-package.js';

function processData(nodes) {
  return { algorithmProcessdata: JSON.stringify(nodes) };
}

test('detectVlmMode recognizes alarm review and direct VLM actions', () => {
  const review = detectVlmMode(processData([{
    actionId: 'BA_00004',
    configObject: { params: [{ key: 'enableLlmReview', value: '1' }] },
  }]));
  assert.deepEqual(review, { direct: false, review: true });

  const directTemplate = processData([{
    actionId: 'DA_00003',
    configObject: { params: [{ key: 'fps', value: '0.3' }] },
  }]);
  assert.deepEqual(detectVlmMode(directTemplate), { direct: true, review: false });
  assert.equal(extractTargetFps(directTemplate), 0.3);

  const disabled = detectVlmMode(processData([{
    actionId: 'BA_00004',
    configObject: { params: [{ key: 'enableLlmReview', value: '0' }] },
  }]));
  assert.deepEqual(disabled, { direct: false, review: false });
});

test('direct VLM throughput uses completed counter deltas instead of detector FPS', async () => {
  let completed = 10;
  let includeVlm = true;
  const client = {
    async taskRunningDetail() {
      return {
        status: [{
          taskId: 'ch1_alg',
          channelId: 'ch1',
          actionStatus: [
            {
              actionId: 'AA_00001',
              name: 'detector',
              processCount: 100,
              processCountPeriod: 50,
              periodMs: 2000,
            },
            ...(includeVlm ? [{
              actionId: 'DA_00003',
              name: 'Qwen3VLWorker',
              processCount: completed,
              processCountPeriod: 2,
              periodMs: 2000,
            }] : []),
          ],
          nodeDurationInfos: [],
        }],
      };
    },
    async queryHardwareResource() {
      return { itemList: [] };
    },
  };
  const sampler = new MetricsSampler(client);
  const binding = [{
    taskKey: 'vlm',
    taskType: 'vlm',
    vlmMode: true,
    vlmObservable: true,
    channelId: 'ch1',
    taskId: 'ch1_alg',
    targetFps: null,
  }];

  const originalNow = Date.now;
  let now = 1_000;
  Date.now = () => now;
  try {
    const first = await sampler.sample(binding);
    assert.equal(first.channels[0].measuredFps, 1);

    completed += 3;
    now += 2_000;
    const second = await sampler.sample(binding);
    assert.equal(second.channels[0].measuredFps, 1.5);
    assert.equal(second.channels[0].primaryProcessTotal, 13);
    assert.equal(second.channels[0].telemetryMissing, false);

    includeVlm = false;
    now += 2_000;
    const missing = await sampler.sample(binding);
    assert.equal(missing.channels[0].measuredFps, null);
    assert.equal(missing.channels[0].telemetryMissing, true);
  } finally {
    Date.now = originalNow;
  }
});

test('VLM report includes Qwen inference in the critical path and rejects missing telemetry', () => {
  const writer = new ReportWriter('.');
  const channel = {
    taskKey: 'vlm',
    taskDisplayName: 'vlm',
    taskType: 'vlm',
    algorithmId: '1',
    channelId: 'ch1',
    targetFps: 1,
    measuredFps: 1,
    throughputFps: 1,
    fpsRatio: 1,
    pipelineMinFps: 1,
    discardRate: 0,
    telemetryMissing: false,
    nodeDurationInfos: [
      { name: 'detector', durationAvgUs: 4_000, durationMaxUs: 5_000 },
      { name: '123 Qwen3VLWorker', durationAvgUs: 800_000, durationMaxUs: 1_000_000 },
    ],
  };
  const samples = [0, 1, 2, 3].map((index) => ({
    stepIndex: 0,
    ts: index * 2_000,
    channels: [{ ...channel }],
    hardware: {},
  }));
  const summary = writer._summarizeStep(
    { index: 0, channels: 1, holdSec: 8 },
    samples,
    {
      taskTypes: {
        vlm: {
          minFpsRatio: 0.8,
          maxMissingRate: 0,
          maxEndToEndLatencyMs: 900,
        },
      },
    },
    'local',
  );

  assert.equal(summary.pass, true);
  assert.equal(summary.primaryLatencyMs, 800);
  assert.equal(summary.criticalPathLatencyMs, 800);

  const missing = writer._summarizeStep(
    { index: 0, channels: 1, holdSec: 8 },
    samples.map((sample) => ({
      ...sample,
      channels: [{
        ...channel,
        telemetryMissing: true,
      }],
    })),
    { taskTypes: { vlm: { minFpsRatio: 0.8, maxMissingRate: 0 } } },
    'local',
  );
  assert.equal(missing.pass, false);
  assert.match(missing.reasons.join(' '), /采样缺失率/);
});
