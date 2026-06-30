// channel-manager.js — Create / reuse video channels on the device.
//
// Two creation paths:
//   - local:           Camera/AddVideo  (filePath + fileName + contentLength)
//   - rtsp-*:          Camera/Add       (channelCode + channelName + url)
//
// IMPORTANT identifier convention (verified in source):
//   - `channelCode` is the caller-supplied code.
//   - `videoChannelId` is the system-generated internal id, returned as resData.id
//     from Add/AddVideo, and present in Camera/Page items.
//   - Task binding (ApplyParamsBatch.targetChannelIds) and RunningDetail use
//     `videoChannelId`, NOT channelCode.
// So we must capture and keep videoChannelId for every channel we create.

import path from 'node:path';
import fs from 'node:fs';

export class ChannelManager {
  /**
   * @param {import('./cosmo-client.js').CosmoClient} client
   * @param {object} [opts]
   * @param {string} [opts.channelPrefix] prefix for bench channels, default 'bench'
   * @param {boolean} [opts.reuse] reuse existing bench channels by name, default true
   * @param {boolean} [opts.cleanup] delete created channels on finish, default false
   * @param {import('./logger.js').Logger} [opts.logger]
   */
  constructor(client, opts = {}) {
    this.client = client;
    this.prefix = opts.channelPrefix ?? 'bench';
    this.reuse = opts.reuse ?? true;
    this.cleanup = opts.cleanup ?? false;
    this.log = opts.logger;
    /** @type {Map<string, {videoChannelId:string, channelCode:string, channelName:string, mode:string}>} */
    this.created = new Map();
  }

  /**
   * Ensure `count` channels exist for the scenario's video mode.
   * Reuses existing bench channels when reuse=true, otherwise always creates new.
   * @param {object} videos parsed videos.yml object
   * @param {number} count max channels needed (largest loadProfile step)
   * @returns {Promise<string[]>} array of videoChannelIds (length === count)
   */
  async ensureChannels(videos, count) {
    const mode = videos.mode;
    let videoChannelIds = [];

    if (this.reuse) {
      videoChannelIds = await this._findExistingBenchChannels(count);
    }

    // Create the shortfall.
    let index = videoChannelIds.length;
    while (videoChannelIds.length < count) {
      const code = `${this.prefix}-${String(index + 1).padStart(2, '0')}`;
      const name = `压测通道${String(index + 1).padStart(2, '0')}`;
      const id = await this._createOne(mode, code, name, videos, index);
      videoChannelIds.push(id);
      this.created.set(id, { videoChannelId: id, channelCode: code, channelName: name, mode });
      index++;
    }
    return videoChannelIds;
  }

  /** Tear down channels created by this manager (only if cleanup=true). */
  async finish() {
    if (!this.cleanup) return;
    const ids = [...this.created.keys()];
    if (!ids.length) return;
    try {
      await this.client.cameraBatchDelete(ids);
    } catch { /* best-effort; ignore */ }
  }

  // ── internals ──────────────────────────────────────────────────────────

  async _createOne(mode, code, name, videos, index) {
    if (mode === 'local') {
      const src = videos.local?.[index] ?? videos.local?.[0];
      if (!src) throw new Error(`videos.yml: not enough local sources for channel ${index + 1}`);
      // Two ways to source a local video:
      //   - file:     a path on THIS machine → uploaded to the device temp store first.
      //   - filePath: a path already staged on the device → used directly (skip upload).
      let filePath;
      let contentLength;
      if (src.file) {
        // NOTE: AddVideo CONSUMES (moves) the temp file, so the same uploaded
        // filePath cannot be reused across channels — each channel needs its own
        // upload. See MessageCameraHandler / CameraDeviceCrud (code 17 文件不存在).
        filePath = await this._uploadLocalVideo(src);
        contentLength = this._localSize(src);
      } else if (src.filePath) {
        filePath = src.filePath;
        contentLength = Number(src.contentLength ?? 0);
      } else {
        throw new Error(`videos.yml: local source must define 'file' or 'filePath'`);
      }
      // Backend (MessageCameraHandler.cc:44) reads only {filePath, channelName, contentLength}
      // from AddVideo; channelCode is NOT consumed there, so we omit it.
      const res = await this.client.cameraAddVideo({
        channelName: src.name ?? name,
        filePath,
        contentLength: String(contentLength),
      });
      const id = res?.resData?.id;
      if (!id) throw new Error(`AddVideo for ${code} returned no id`);
      return id;
    }
    // rtsp-fidelity / rtsp-deterministic
    const src = videos.rtsp?.[index] ?? videos.rtsp?.[0];
    if (!src) throw new Error(`videos.yml: not enough rtsp sources for channel ${index + 1}`);
    const payload = {
      channelCode: code,
      channelName: src.name ?? name,
      channelType: 0,  // RTSP camera
      url: src.url,
    };
    const res = await this.client.cameraAdd(payload);
    const id = res?.resData?.id;
    if (!id) throw new Error(`Camera/Add for ${code} returned no id`);
    return id;
  }

  /**
   * Upload a local video file to the device temp store in chunks, returning the
   * device-side filePath to pass to AddVideo. Mirrors the frontend
   * uploadVideoByChunk (CHUNK_SIZE = 32 MiB). The returned filePath comes from the
   * LAST chunk's response (when all chunks are merged).
   */
  async _uploadLocalVideo(src) {
    const localPath = src.file;
    if (!localPath) throw new Error(`videos.yml: local source missing 'file' path`);
    const stat = fs.statSync(localPath);
    const totalSize = stat.size;
    const CHUNK_SIZE = 32 * 1024 * 1024;
    const totalChunks = Math.max(1, Math.ceil(totalSize / CHUNK_SIZE));
    const uploadId = `bench_${Date.now()}_${Math.random().toString(16).slice(2)}`;
    const fileName = path.basename(localPath);
    const fd = fs.openSync(localPath, 'r');
    let filePath = null;
    try {
      for (let chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        const start = chunkIndex * CHUNK_SIZE;
        const chunkSize = Math.min(CHUNK_SIZE, totalSize - start);
        const chunkBuf = Buffer.alloc(chunkSize);
        fs.readSync(fd, chunkBuf, 0, chunkSize, start);
        const res = await this.client.uploadTempChunk(chunkBuf, fileName, {
          uploadId,
          chunkIndex,
          totalChunks,
          totalSize,
          chunkSize,
        });
        if (chunkIndex === totalChunks - 1) {
          filePath = res?.resData?.filePath ?? null;
        }
      }
    } finally {
      fs.closeSync(fd);
    }
    if (!filePath) throw new Error(`uploadTemp returned no filePath for ${fileName}`);
    this.log?.debug?.(`uploaded ${fileName} → ${filePath}`);
    return filePath;
  }

  /** Cached local-file size, used for AddVideo contentLength. */
  _localSize(src) {
    if (src._size != null) return src._size;
    const s = fs.statSync(src.file);
    return (src._size = s.size);
  }

  /**
   * Find existing channels whose channelName starts with our prefix,
   * up to `count`. Used for reuse across reruns.
   */
  async _findExistingBenchChannels(count) {
    const found = [];
    let pageNum = 1;
    const pageSize = 200;
    try {
      while (found.length < count) {
        const res = await this.client.cameraPage({ pageNum, pageSize });
        const list = res?.rows ?? res?.list ?? res?.data ?? [];
        if (!list.length) break;
        for (const ch of list) {
          // Match by channelName containing our prefix marker (bench channel names are localized,
          // so match on channelCode prefix instead when present).
          const code = ch.channelCode ?? '';
          if (code.startsWith(this.prefix) && ch.videoChannelId) {
            found.push(ch.videoChannelId);
            if (found.length >= count) break;
          }
        }
        if (list.length < pageSize) break;
        pageNum++;
      }
    } catch {
      // Reuse is best-effort; fall through to create new.
    }
    return found;
  }
}
