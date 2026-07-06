import assert from 'node:assert/strict';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { ScenarioPackage } from '../src/scenario-package.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

test('loads a single-task scenario through the canonical workload schema', () => {
  const pkg = new ScenarioPackage(
    path.join(root, 'scenarios/no-helmet-99898-fps5-20260630'),
  ).load();

  assert.equal(pkg.scenario.name, 'no-helmet-99898-fps5-20260630');
  assert.equal(pkg.videoMode, 'local');
  assert.equal(pkg.videoRepeatCount, 0);
  assert.equal(pkg.tasks.length, 1);
  assert.equal(pkg.tasks[0].id, 'no-helmet');
  assert.equal(pkg.tasks[0].algorithmId, '99898');
  assert.deepEqual(pkg.bindings, [{ taskId: 'no-helmet', channels: 'all' }]);
  assert.equal(pkg.thresholds.pass.avgDiscardRate, 0.05);
  assert.match(pkg.videos.local[0].file, /LX0000000007\.mp4$/);

  const repeatParam = pkg.taskConfig.params.find((param) => param.key === 'param.videoRepeatCount');
  assert.equal(repeatParam?.value, '0');
});

test('loads a multi-task scenario without a package version flag', () => {
  const pkg = new ScenarioPackage(path.join(root, 'scenarios/multi-task-example')).load();

  assert.equal(pkg.scenario.version, undefined);
  assert.equal(pkg.tasks.length, 2);
  assert.deepEqual(
    pkg.tasks.map((task) => task.id),
    ['no-helmet', 'play-phone'],
  );
  assert.deepEqual(pkg.bindings, [
    { taskId: 'no-helmet', channels: 'all' },
    { taskId: 'play-phone', channels: 'all' },
  ]);
  assert.equal(pkg.thresholds.pass.avgDiscardRate, 0.05);
  assert.match(pkg.videos.local[0].file, /LX0000000007\.mp4$/);
});
