#!/usr/bin/env node
// cli.js — scenario-bench entry point.
//
// Usage:
//   node src/cli.js run \
//     --device http://192.168.1.10:8080 \
//     --user admin --password admin \
//     --scenario scenarios/play-phone \
//     --output reports/play-phone-20260630
//
// Flags:
//   --device      Device base URL (required)
//   --user        Login account (required)
//   --password    Plain-text password (required; MD5-hashed internally)
//   --scenario    Path to scenario directory (required)
//   --output      Report output directory (required)
//   --no-reuse    Do not reuse existing bench channels; always create new
//   --cleanup     Delete created channels after the run
//   --skip-import Skip the layout save step (template already imported)
//   --lang        Accept-Language header (default zh-CN)
//   --verbose     Debug logging

import { CosmoClient } from './cosmo-client.js';
import { ScenarioPackage } from './scenario-package.js';
import { ChannelManager } from './channel-manager.js';
import { TaskRunner } from './task-runner.js';
import { MetricsSampler } from './metrics-sampler.js';
import { ReportWriter } from './report-writer.js';
import { Logger } from './logger.js';

function parseArgs(argv) {
  const args = {};
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a.startsWith('--')) {
      const key = a.slice(2);
      if (key === 'verbose' || key === 'no-reuse' || key === 'cleanup' || key === 'skip-import') {
        args[key] = true;
      } else {
        args[key] = argv[++i];
      }
    } else if (a === 'run') {
      args.command = 'run';
    } else if (a === 'help' || a === '--help' || a === '-h') {
      args.command = 'help';
    }
  }
  return args;
}

function printHelp() {
  console.log(`scenario-bench — CosmoEdge 场景任务可复现压测工具

用法:
  scenario-bench run --device <url> --user <u> --password <p> \\
                     --scenario <dir> --output <dir> [options]

必填:
  --device <url>       设备地址, 例如 http://192.168.1.10:8080
  --user <account>     登录账号
  --password <plain>   登录密码 (内部 MD5 大写后传输)
  --scenario <dir>     场景包目录 (含 algorithm-template.json 等 4 个文件)
  --output <dir>       报告输出目录

可选:
  --no-reuse           不复用已存在的 bench 通道, 总是新建
  --cleanup            结束后删除本次创建的通道
  --skip-import        跳过 algorithm/layout/save (模板已导入时使用)
  --ramp-batch-size <n> 每批新增通道数, 默认 1
  --ramp-batch-delay-sec <n> 批次间隔秒数, 默认 15
  --lang <code>        Accept-Language, 默认 zh-CN
  --verbose            打印调试日志
  -h, --help           显示本帮助`);
}

async function main() {
  const args = parseArgs(process.argv.slice(2));

  if (!args.command || args.command === 'help') {
    printHelp();
    return process.exit(args.command === 'help' ? 0 : 1);
  }

  const required = ['device', 'user', 'password', 'scenario', 'output'];
  const missing = required.filter((k) => !args[k]);
  if (missing.length) {
    console.error(`缺少必填参数: ${missing.map((m) => '--' + m).join(', ')}`);
    printHelp();
    return process.exit(2);
  }

  const log = new Logger({ verbose: args.verbose });
  const startedAt = new Date().toISOString();

  // Mutable run state, hoisted so a partial report is still written if the run
  // aborts mid-way (e.g. device hangs under heavy load → request times out and
  // throws). Everything collected up to the failure point is preserved.
  let pkg = null;
  let deviceInfo = {};
  let channelMgr = null;
  const samples = [];
  let baselineFps = null;
  let bottleneckStep = null;
  let bottleneckReason = null;
  let runError = null;
  let currentChannels = 0;
  let currentStepIndex = -1;
  const writer = new ReportWriter(args.output);

  const buildResult = (status = runError ? 'aborted' : 'completed') => ({
    scenarioName: pkg?.scenario?.displayName ?? pkg?.scenario?.name,
    algorithmId: pkg?.algorithmId,
    algorithmName: pkg?.template?.algorithmName,
    targetFps: pkg?.targetFps,
    videoMode: pkg?.videoMode,
    status,
    error: runError ? { message: runError.message, atChannels: currentChannels || null, atStepIndex: currentStepIndex } : null,
    device: {
      model: deviceInfo.deviceType ?? deviceInfo.deviceModel,
      sn: deviceInfo.deviceSn ?? deviceInfo.sn,
      softwareVersion: deviceInfo.softwareVersion,
      hardwareVersion: deviceInfo.hardwareVersion,
    },
    thresholds: pkg?.thresholds,
    loadProfile: pkg?.loadProfile?.map((s, i) => ({ index: i, ...s })) ?? [],
    bottleneck: bottleneckStep != null
      ? { stepIndex: bottleneckStep, stepNumber: bottleneckStep + 1, channels: pkg?.loadProfile?.[bottleneckStep]?.channels, reason: bottleneckReason }
      : null,
    baselineFps,
    startedAt,
    endedAt: new Date().toISOString(),
    samples,
    steps: pkg?.loadProfile?.map((s, i) => ({ index: i, ...s })) ?? [],
  });

  const writePartial = async () => {
    if (!pkg) return;
    try {
      await writer.writePartial(buildResult('running'));
    } catch (err) {
      log.warn(`write partial report failed: ${err.message}`);
    }
  };

  try {
    // 1. Parse scenario package.
    log.info(`Loading scenario package: ${args.scenario}`);
    pkg = new ScenarioPackage(args.scenario).load();
    log.info(`Scenario "${pkg.scenario.name}" | algorithmId=${pkg.algorithmId} | targetFps=${pkg.targetFps ?? 'N/A'} | mode=${pkg.videoMode}`);
    const maxChannels = Math.max(...pkg.loadProfile.map((s) => s.channels));

    // 2. Connect + login.
    log.info(`Connecting to device ${args.device}...`);
    const client = new CosmoClient({
      base: args.device,
      user: args.user,
      password: args.password,
      lang: args.lang ?? 'zh-CN',
    });
    await client.login();
    log.info('Login OK.');

    // 3. Record device info (devInfoList is a [{key,name,value}] list — flatten it).
    const deviceInfoRaw = await client.queryDeviceInfo().catch((e) => { log.warn(`QueryDeviceInfo failed: ${e.message}`); return {}; });
    for (const it of deviceInfoRaw?.devInfoList ?? []) {
      if (it?.key) deviceInfo[it.key] = it.value;
    }

    // 4. Import orchestration template (unless skipped).
    if (args['skip-import']) {
      log.info('Skipping layout save (--skip-import).');
    } else {
      log.info('Saving orchestration template via /algorithm/layout/save...');
      await client.layoutSave(pkg.layoutSavePayload);
      log.info('Layout saved.');
    }

    // 5. Ensure channels.
    channelMgr = new ChannelManager(client, {
      channelPrefix: `bench-${pkg.scenario.name}`,
      reuse: !args['no-reuse'],
      cleanup: args.cleanup,
      logger: log,
    });
    log.info(`Ensuring ${maxChannels} channels (mode=${pkg.videoMode})...`);
    const videoChannelIds = await channelMgr.ensureChannels(pkg.videos, maxChannels);
    log.info(`Channels ready: ${videoChannelIds.join(', ')}`);

    // 6. Build task runner: bind all, switch all off, then ramp.
    //    taskConfig carries param.videoRepeatCount so local videos loop (0=infinite,
    //    see AlgChannelDemux / Keys.h CHANNEL_SOURCE_REPEAT).
    const runner = new TaskRunner(client, {
      algorithmId: pkg.algorithmId,
      algorithmCode: pkg.template.algorithmCode,
      scheduleId: pkg.scheduleId,
      taskConfig: pkg.taskConfig,
      rampBatchSize: Number(args['ramp-batch-size'] ?? 1),
      rampBatchDelaySec: Number(args['ramp-batch-delay-sec'] ?? 15),
    }, log);
    await runner.setChannels(videoChannelIds);

    // 7. Sample during the staircase, with bottleneck early-stop.
    //    Bottleneck judgement uses STEADY-STATE samples only. RunningDetail's fps
    //    (processCountPeriod/periodMs) ramps up over ~10 ticks after a channel is
    //    bound, because periodMs is a cumulative window from task start. Likewise
    //    packet discard spikes transiently while freshly-added channels stabilize.
    //    Judging on all ticks would false-trigger on these ramp artifacts, so we
    //    only look at the last half of each step's hold window (steady state).
    //    Criteria: steady minFps < baseline×0.5 after a step hold. Resource and
    //    discard fuses are evaluated per sample with consecutive/window rules.
    const sampler = new MetricsSampler(client, log);
    const activeMap = () => {
      const m = new Map();
      for (const chId of runner.allChannelIds.slice(0, currentChannels)) {
        m.set(chId, runner.taskIdFor(chId));
      }
      return m;
    };

    // Bottleneck thresholds (absolute, independent of pass/fail thresholds).
    const FPS_HALVE_RATIO = 0.5;     // steady minFps < baselineFps * 0.5 → bottleneck
    const DISCARD_BOTTLENECK = 0.05; // steady meanDiscard > 5% → bottleneck

    /**
     * Aggregate a step's STEADY-STATE samples (last half of the hold window, to
     * exclude the post-ramp fps/discard transient) into per-channel min fps,
     * MEAN discard (not max — max is too sensitive to single-tick spikes and
     * false-triggers on transient jitter), and peak NPU/CPU utilization.
     *
     * Discard uses the mean across all steady ticks × all channels because a
     * single outlier sample (e.g. a GC pause or a freshly-stabilizing channel)
     * should not flip a whole step to bottleneck. The mean smooths that out while
     * still catching a sustained, real overload where discard stays high.
     */
    const summarizeSamples = (stepIdx) => {
      const all = samples.filter((s) => s.stepIndex === stepIdx && !s.channels.every((c) => c.missing));
      if (!all.length) return { minFps: null, meanDiscard: null, maxNpu: null, maxCpu: null };
      // steady state = last half of the step's ticks (>=1)
      const steady = all.slice(Math.floor(all.length / 2));
      let minFps = Infinity, maxNpu = -Infinity, maxCpu = -Infinity;
      const discardValues = [];
      for (const t of steady) {
        const npu = t.hardware?.npuUtilization?.usedPercent;
        const cpu = t.hardware?.cpuUtilization?.usedPercent;
        if (typeof npu === 'number' && npu > maxNpu) maxNpu = npu;
        if (typeof cpu === 'number' && cpu > maxCpu) maxCpu = cpu;
        for (const ch of t.channels) {
          if (ch.missing) continue;
          if (typeof ch.measuredFps === 'number' && ch.measuredFps < minFps) minFps = ch.measuredFps;
          if (typeof ch.discardRate === 'number') discardValues.push(ch.discardRate);
        }
      }
      const meanDiscard = discardValues.length
        ? discardValues.reduce((a, b) => a + b, 0) / discardValues.length
        : null;
      return {
        minFps: minFps === Infinity ? null : minFps,
        meanDiscard,
        maxNpu: maxNpu === -Infinity ? null : maxNpu,
        maxCpu: maxCpu === -Infinity ? null : maxCpu,
      };
    };

    const captureSample = async () => {
      const m = activeMap();
      const sample = await sampler.sample(m, pkg.targetFps);
      sample.stepIndex = currentStepIndex;
      samples.push(sample);
      await writePartial();
      const ch0 = sample.channels[0];
      log.debug(`sample step=${currentStepIndex} ch=${sample.activeChannels} fps=${ch0?.measuredFps ?? '-'} discard=${ch0?.discardRate ?? '-'} cpu=${sample.hardware?.cpuUtilization?.usedPercent ?? '-'}%`);
      return sample;
    };

    const lastConsecutive = (selector, predicate) => {
      let count = 0;
      for (let i = samples.length - 1; i >= 0; i--) {
        const value = selector(samples[i]);
        if (typeof value !== 'number' || !predicate(value)) break;
        count++;
      }
      return count;
    };

    const recentAverage = (selector, windowMs, minSamples = 3) => {
      const now = samples[samples.length - 1]?.ts ?? Date.now();
      const values = samples
        .filter((s) => now - s.ts <= windowMs)
        .map(selector)
        .filter((v) => typeof v === 'number' && Number.isFinite(v));
      return values.length >= minSamples ? values.reduce((a, b) => a + b, 0) / values.length : null;
    };

    // Mean discard across channels for one sample (not max — see summarizeSamples
    // rationale: mean resists single-channel transient spikes).
    const meanChannelDiscard = (s) => {
      const values = (s.channels ?? [])
        .filter((ch) => !ch.missing && typeof ch.discardRate === 'number')
        .map((ch) => ch.discardRate);
      return values.length ? values.reduce((a, b) => a + b, 0) / values.length : null;
    };

    const quickFuse = (sample) => {
      const reasons = [];
      if (sample.hardware?._error && sample.channels?.length && sample.channels.every((c) => c.missing)) {
        reasons.push('RunningDetail and HardwareResource unavailable');
      }

      const memAvg60s = recentAverage((s) => s.hardware?.generalMemoryUtilization?.usedPercent, 60_000);
      const mem98Count = lastConsecutive(
        (s) => s.hardware?.generalMemoryUtilization?.usedPercent,
        (v) => v >= 98,
      );
      const cpu98Count = lastConsecutive(
        (s) => s.hardware?.cpuUtilization?.usedPercent,
        (v) => v >= 98,
      );
      const npu98Count = lastConsecutive(
        (s) => s.hardware?.npuUtilization?.usedPercent,
        (v) => v >= 98,
      );
      const discardCount = lastConsecutive(meanChannelDiscard, (v) => v > DISCARD_BOTTLENECK);

      if (mem98Count >= 3) {
        reasons.push(`memory >= 98% for ${mem98Count} consecutive samples`);
      }
      if (memAvg60s != null && memAvg60s >= 95) {
        reasons.push(`memory 60s average ${memAvg60s.toFixed(1)}% >= 95%`);
      }
      if (cpu98Count >= 3) {
        reasons.push(`CPU >= 98% for ${cpu98Count} consecutive samples`);
      }
      if (npu98Count >= 3) {
        reasons.push(`NPU >= 98% for ${npu98Count} consecutive samples`);
      }
      if (discardCount >= 2) {
        reasons.push(`discardRate > ${DISCARD_BOTTLENECK} for ${discardCount} consecutive samples`);
      }
      return reasons.length ? { stop: true, reason: reasons.join('; ') } : { stop: false };
    };

    const staircaseResult = await runner.runStaircase(pkg.loadProfile, {
      onRampBatch: async (step, active) => {
        currentChannels = active.length;
        currentStepIndex = step.index;
        const sample = await captureSample();
        return quickFuse(sample);
      },
      onStepStart: async (step, active) => {
        currentChannels = active.length;
        currentStepIndex = step.index;
      },
      onSample: async () => {
        await captureSample();
      },
      onStepEnd: async (step) => {
        const { minFps, meanDiscard, maxNpu, maxCpu } = summarizeSamples(step.index);
        const sat = `npu=${maxNpu ?? '-'}% cpu=${maxCpu ?? '-'}%`;
        // Lock in the baseline from the first step.
        if (step.index === 0) {
          baselineFps = minFps ?? null;
          log.info(`[baseline] step 1 steady minFps=${baselineFps} fps (${sat}) → gate at fps<${(baselineFps * FPS_HALVE_RATIO).toFixed(1)} or meanDiscard>${DISCARD_BOTTLENECK}`);
          return;
        }
        const reasons = [];
        if (baselineFps != null && minFps != null && minFps < baselineFps * FPS_HALVE_RATIO) {
          reasons.push(`fps ${minFps.toFixed(1)} < baseline ${baselineFps.toFixed(1)}×${FPS_HALVE_RATIO} (${(baselineFps * FPS_HALVE_RATIO).toFixed(1)})`);
        }
        if (meanDiscard != null && meanDiscard > DISCARD_BOTTLENECK) {
          reasons.push(`meanDiscard ${meanDiscard.toFixed(3)} > ${DISCARD_BOTTLENECK}`);
        }
        // Discard fuse is also handled by quickFuse as "two consecutive samples".
        const satNote = (maxNpu >= 90 || maxCpu >= 90) ? ' [资源接近饱和]' : '';
        log.info(`[step ${step.index + 1}] steady minFps=${minFps?.toFixed(1) ?? '-'} meanDiscard=${meanDiscard?.toFixed(3) ?? '-'} ${sat}${satNote} ${reasons.length ? '→ BOTTLENECK (' + reasons.join('; ') + ')' : '→ ok, continuing'}`);
        return reasons.length ? { stop: true, reason: reasons.join('; ') } : { stop: false };
      },
    }, pkg.sampleIntervalSec);
    bottleneckStep = staircaseResult?.bottleneckStep ?? null;
    bottleneckReason = staircaseResult?.bottleneckReason ?? null;
  } catch (err) {
    // The run aborted (device hang/timeout, network error, API failure, etc.).
    // Keep going: we still write a partial report from whatever was collected.
    runError = err;
    log.error(`压测中断（将输出部分报告）: ${err.message}`);
  } finally {
    // Best-effort teardown so we don't leave tasks running / channels orphaned.
    if (channelMgr) {
      try {
        await channelMgr.finish();
      } catch (e) {
        log.warn(`清理通道失败: ${e.message}`);
      }
    }
  }

  // 8. Write report — ALWAYS, as long as we parsed the scenario. A partial run
  //    (aborted) still produces a report with status=aborted + whatever samples
  //    were captured, so a device hang under load is never silently lost.
  if (!pkg) {
    // Failure happened before the scenario package even loaded — nothing to report.
    console.error(`\n✘ 压测失败: ${runError?.message ?? 'unknown error'}`);
    if (runError?.stack && process.env.BENCH_DEBUG) console.error(runError.stack);
    return process.exit(1);
  }

  const reachedChannels = currentChannels || null;
  const result = buildResult();
  const { jsonPath, htmlPath } = await writer.write(result);
  if (runError) {
    log.warn(`部分报告已输出（status=aborted, 中断于 ${reachedChannels ?? '?'} 路）:\n  ${jsonPath}\n  ${htmlPath}`);
    console.error(`\n✘ 压测中断: ${runError.message}`);
    if (runError.stack && process.env.BENCH_DEBUG) console.error(runError.stack);
    return process.exit(1);
  }
  log.info(`Report written:\n  ${jsonPath}\n  ${htmlPath}`);
}

main().catch((err) => {
  console.error(`\n✘ 压测失败: ${err.message}`);
  if (err.stack && process.env.BENCH_DEBUG) console.error(err.stack);
  process.exit(1);
});
