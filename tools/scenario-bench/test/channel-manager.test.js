import assert from 'node:assert/strict';
import test from 'node:test';

import { ChannelManager } from '../src/channel-manager.js';

test('reuses local AddVideo channels by bench-prefixed channelName', async () => {
  let addCalls = 0;
  const client = {
    async cameraPage() {
      return {
        rows: [
          { channelName: 'bench-case-01-clip', videoChannelId: 'existing-1' },
          { channelCode: 'bench-case-02', videoChannelId: 'existing-2' },
        ],
      };
    },
    async cameraAddVideo() {
      addCalls++;
      return { resData: { id: `created-${addCalls}` } };
    },
  };
  const manager = new ChannelManager(client, {
    channelPrefix: 'bench-case',
    reuse: true,
  });

  const ids = await manager.ensureChannels({
    mode: 'local',
    local: [{ name: 'clip', filePath: '/device/clip.mp4', contentLength: 100 }],
  }, 2);

  assert.deepEqual(ids, ['existing-1', 'existing-2']);
  assert.equal(addCalls, 0);
});

test('created local AddVideo channel names keep a stable bench prefix', async () => {
  const payloads = [];
  const client = {
    async cameraPage() {
      return { rows: [] };
    },
    async cameraAddVideo(payload) {
      payloads.push(payload);
      return { resData: { id: `created-${payloads.length}` } };
    },
  };
  const manager = new ChannelManager(client, {
    channelPrefix: 'bench-case',
    reuse: true,
  });

  const ids = await manager.ensureChannels({
    mode: 'local',
    local: [{ name: 'clip', filePath: '/device/clip.mp4', contentLength: 100 }],
  }, 1);

  assert.deepEqual(ids, ['created-1']);
  assert.equal(payloads[0].channelName, 'bench-case-01-clip');
  assert.equal(payloads[0].filePath, '/device/clip.mp4');
  assert.equal(payloads[0].contentLength, '100');
});
