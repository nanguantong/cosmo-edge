// Bind scenario tasks and drive a slow-start load staircase.
//
// ApplyParamsBatch auto-enables tasks, so a large batch can create a short but
// severe decode/model/memory allocation spike. The runner therefore binds new
// channels in small batches and gives the CLI a chance to sample/fuse after each
// batch. Local file tasks are still bound only once; OFF->ON may not reopen the
// local demuxer.

export class TaskRunner {
  /**
   * @param {import('./cosmo-client.js').CosmoClient} client
   * @param {object} ctx
   * @param {string} ctx.algorithmId
   * @param {string|number} ctx.algorithmCode
   * @param {string} ctx.scheduleId
   * @param {object} ctx.taskConfig
   * @param {number} [ctx.rampBatchSize]
   * @param {number} [ctx.rampBatchDelaySec]
   * @param {import('./logger.js').Logger} [logger]
   */
  constructor(client, ctx, logger) {
    this.client = client;
    this.algorithmId = String(ctx.algorithmId);
    this.algorithmCode = String(ctx.algorithmCode);
    this.scheduleId = ctx.scheduleId;
    this.taskConfig = ctx.taskConfig ?? { params: [], areas: [] };
    this.rampBatchSize = Math.max(1, Number(ctx.rampBatchSize ?? 1));
    this.rampBatchDelaySec = Math.max(0, Number(ctx.rampBatchDelaySec ?? 15));
    this.log = logger;
    /** @type {Map<string, string>} channelId -> taskId */
    this.channelTaskMap = new Map();
    /** @type {string[]} videoChannelIds in bind order */
    this.allChannelIds = [];
  }

  taskIdFor(videoChannelId) {
    return `${videoChannelId}_${this.algorithmCode}`;
  }

  setChannels(videoChannelIds) {
    this.allChannelIds = [...videoChannelIds];
    this.channelTaskMap = new Map(videoChannelIds.map((id) => [id, this.taskIdFor(id)]));
  }

  async _bind(videoChannelIds) {
    if (!videoChannelIds.length) return [];
    this.log?.info(`Binding ${videoChannelIds.length} channel(s) via ApplyParamsBatch...`);
    const { failedList } = await this.client.taskApplyParamsBatch({
      algorithmId: this.algorithmId,
      scheduleId: this.scheduleId,
      taskConfig: this.taskConfig,
      targetChannelIds: videoChannelIds,
    });
    if (failedList?.length) {
      const failed = failedList.map((f) => f.id).join(', ');
      this.log?.warn(`ApplyParamsBatch partially failed on: ${failed}`);
    }
    return failedList ?? [];
  }

  async _switch(videoChannelIds, enable) {
    if (!videoChannelIds.length) return [];
    const tasks = videoChannelIds.map((id) => ({
      id: this.taskIdFor(id),
      channelId: id,
      algorithmId: this.algorithmId,
      enable,
    }));
    const { failedList } = await this.client.taskBatchSwitch(tasks);
    if (failedList?.length) {
      this.log?.warn(`BatchSwitchTask(enable=${enable}) failed on: ${failedList.map((f) => f.id).join(', ')}`);
    }
    return failedList ?? [];
  }

  /**
   * @param {Array<{channels:number, holdSec:number}>} loadProfile
   * @param {object} hooks
   * @param {(step:object, active:string[], added:string[]) => Promise<{stop:boolean, reason?:string}|void>} [hooks.onRampBatch]
   * @param {(step:object, active:string[]) => Promise<void>} [hooks.onStepStart]
   * @param {() => Promise<void>} [hooks.onSample]
   * @param {(step:object, active:string[]) => Promise<{stop:boolean, reason?:string}|void>} [hooks.onStepEnd]
   * @param {number} sampleIntervalSec
   * @returns {Promise<{bottleneckStep?:number, bottleneckReason?:string}>}
   */
  async runStaircase(loadProfile, hooks, sampleIntervalSec) {
    let active = [];
    let bottleneck = null;

    try {
      for (let i = 0; i < loadProfile.length; i++) {
        const step = { ...loadProfile[i], index: i };
        const target = this.allChannelIds.slice(0, step.channels);
        const toAdd = target.slice(active.length);

        this.log?.info(
          `[step ${i + 1}/${loadProfile.length}] ramp to ${step.channels} channels `
          + `(+${toAdd.length}), hold ${step.holdSec}s`,
        );

        for (let offset = 0; offset < toAdd.length; offset += this.rampBatchSize) {
          const batch = toAdd.slice(offset, offset + this.rampBatchSize);
          await this._bind(batch);
          active = this.allChannelIds.slice(0, active.length + batch.length);

          if (hooks?.onRampBatch) {
            const decision = await hooks.onRampBatch(step, active, batch);
            if (decision?.stop) {
              this.log?.warn(`[step ${i + 1}] ramp fuse tripped: ${decision.reason ?? 'threshold breached'}`);
              bottleneck = { bottleneckStep: i, bottleneckReason: decision.reason ?? 'ramp fuse tripped' };
              return bottleneck;
            }
          }

          const hasMoreBatches = offset + this.rampBatchSize < toAdd.length;
          if (hasMoreBatches && this.rampBatchDelaySec > 0) {
            await sleep(this.rampBatchDelaySec * 1000);
          }
        }

        active = target;
        if (hooks?.onStepStart) await hooks.onStepStart(step, active);

        const ticks = Math.max(1, Math.floor(step.holdSec / sampleIntervalSec));
        for (let t = 0; t < ticks; t++) {
          await sleep(sampleIntervalSec * 1000);
          try {
            if (hooks?.onSample) await hooks.onSample();
          } catch (err) {
            this.log?.warn(`sample tick failed: ${err.message}`);
          }
        }

        if (hooks?.onStepEnd) {
          const decision = await hooks.onStepEnd(step, active);
          if (decision?.stop) {
            this.log?.warn(`[step ${i + 1}] bottleneck detected - stopping staircase: ${decision.reason ?? 'threshold breached'}`);
            bottleneck = { bottleneckStep: i, bottleneckReason: decision.reason ?? 'threshold breached' };
            break;
          }
        }
      }

      return bottleneck ?? {};
    } finally {
      if (active.length) {
        this.log?.info(`Switching ${active.length} active task(s) OFF.`);
        try {
          await this._switch(active, 0);
        } catch (err) {
          this.log?.warn(`Best-effort task OFF failed: ${err.message}`);
        }
      }
    }
  }
}

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}
