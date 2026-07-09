---
title: "Deploy Ultralytics YOLO with CosmoEdge"
description: Turn an Ultralytics YOLO model export into a visual edge video analytics pipeline with model management, OSD, events, and benchmark references.
prev:
  text: "Volume 5: Model Porting"
  link: /en/tutorials/05-model-porting/model-porting
next: false
---

# Deploy Ultralytics YOLO with CosmoEdge

> **Status**: Draft for review. Fill the TODO fields before sharing the final community link.
> **Audience**: Ultralytics YOLO users who already have a trained model and want to turn it into an edge video analytics application.
> **Goal**: Export a YOLO model, import it into CosmoEdge, compose a visual pipeline, bind it to a video source, and verify live OSD plus event output.

This guide focuses on the deployment side of the workflow:

```plain
Ultralytics YOLO model
  -> ONNX export
  -> CosmoEdge Model Repository
  -> Visual pipeline orchestration
  -> Video channel binding
  -> Live OSD and event records
  -> MQTT / HTTP integration
```

CosmoEdge does not replace Ultralytics training or export. It provides the runtime and application layer after a model is ready: model lifecycle management, visual pipeline composition, video source binding, OSD rendering, alarm/event records, and integration with downstream systems.

## Demo

<!-- TODO: Replace this placeholder with a stable public video URL or repo-hosted video asset. -->
<!-- Suggested source material: yolov8sdet&clspipeline.mp4 -->

The demo should show the end-to-end flow in this order:

1. Live OSD result, so readers first see what the deployment produces.
2. Ultralytics YOLO model download or selection.
3. ONNX export.
4. ONNX import into the CosmoEdge Model Repository.
5. Visual pipeline composition.
6. Video channel binding and rule configuration.
7. Final live OSD and event records.

## Workflow Overview

![From Ultralytics YOLO to Edge Video Analytics](../../../assets/community/ultralytics-yolo-edge-flow.png)

At a high level, the workflow has two parts:

- **Model preparation**: export a trained Ultralytics model to ONNX, then prepare labels and model metadata.
- **Application deployment**: import the model, compose the pipeline, bind video sources, configure regions/rules, and verify OSD/events.

For a full model conversion walkthrough, see [Volume 5: Model Porting](../05-model-porting/model-porting.md). For pipeline internals, see [Volume 4: Pipeline Orchestration](../04-pipeline-orchestration/pipeline-orchestration.md).

## Reproducible Reference Configuration

Use this section as the public reproduction record for the guide. It should be complete enough for another developer to compare their own Ultralytics export, ONNX runtime path, hardware backend, and validation output against this example.

### Model Preparation Environment

| Field | Value |
| --- | --- |
| Python version | `TODO` |
| Ultralytics package | `TODO: ultralytics==x.y.z` |
| ONNX package | `TODO: onnx==x.y.z` |
| ONNX Runtime package | `TODO: onnxruntime==x.y.z` |
| Export host OS | `TODO` |
| Export command log | `TODO: link to command output or paste exact command block` |

### Models

| Role | Model | Source | Export input size | Export status |
| --- | --- | --- | --- | --- |
| Detection | `YOLOv8n.pt` | Ultralytics documentation | `640 x 640` | Used by ScenarioBench v1.0 |
| Classification | `YOLOv8n-cls.pt` | Ultralytics documentation | `224 x 224` | Used by ScenarioBench v1.0 no-helmet pipeline |
| Demo detection model | `TODO` | `TODO` | `TODO` | Fill from the final demo material |

### Runtime And Hardware

| Target | Hardware profile | CosmoEdge software version | Runtime backend | Purpose |
| --- | --- | --- | --- | --- |
| NPU benchmark | `npu-yy-16t01-preview` (`YY-16T01-Preview`) | `V1.0.0.0` | `TODO: NPU runtime/toolchain version` | High-concurrency edge CV validation |
| x86 baseline | `x86-cpu-baseline` (`X86-TRIAL`) | `V0.1.0.0` | `TODO: ONNX Runtime CPU version` | CPU-only comparison and ONNX-path validation |
| Demo device | `TODO` | `TODO` | `TODO` | Device used in the public demo video |

The benchmark hardware identifiers are anonymized. Keep that policy for public docs, but preserve enough version information for reproduction.

### Validation Inputs And Outputs

| Validation item | Current value | Final guide requirement |
| --- | --- | --- |
| Test video | `data/test-video/Safety Helmet.mp4` | Confirm resolution, duration, source FPS, and whether it can be shared publicly. |
| Pipeline template | `Pedestrian Detection 45626`, `No Safety Helmet 7463` | Attach or link the exported pipeline layout if it is safe to publish. |
| Model import output | `TODO` | Screenshot of the imported model and metadata. |
| Live OSD output | `TODO` | Screenshot or short video showing boxes, labels, regions, and event state. |
| Event output | `TODO` | Screenshot or sample payload from Event Center, MQTT, or HTTP webhook. |
| Benchmark output | ScenarioBench v1.0 reports | Link the exact report used for each result row. |

<!-- TODO: Replace TODO fields above before posting the final follow-up in the Ultralytics discussion. -->
<!-- TODO: Confirm whether exact Ultralytics package and ONNX Runtime versions come from the demo environment, benchmark environment, or a freshly reproduced export environment. -->

## Prerequisites

Prepare the following before starting:

| Item | Notes |
| --- | --- |
| CosmoEdge runtime | Use a released CosmoEdge package or a validated development build. |
| Target hardware | x86 mode can validate the UI and ONNX path. NPU hardware is required for production-level edge throughput. |
| Ultralytics model | A `.pt` detection model such as `yolov8n.pt`, or a custom trained model. |
| Test video | Use a video that contains objects from your model label set. |
| Label file | A class-name mapping that matches the exported model output. |
| Optional classifier | Required only for multi-stage pipelines such as detection plus safety-helmet classification. |

<!-- TODO: Confirm the exact CosmoEdge release version and hardware profile used for the public demo. -->
<!-- TODO: Confirm whether the canonical demo uses YOLOv8n only, YOLOv8s detection, or detection plus classification. -->

## Step 1: Export YOLO to ONNX

Install the exact packages used for the reproduced export. Pinning versions keeps export behavior, ONNX graph structure, and runtime compatibility easier to compare:

```bash
python -m pip install "ultralytics==TODO" "onnx==TODO" "onnxruntime==TODO"
```

<!-- TODO: Replace package placeholders with the final reproduced environment versions. -->

Export a YOLO detection model to ONNX:

```bash
yolo export \
  model=yolov8n.pt \
  format=onnx \
  imgsz=640 \
  opset=TODO \
  simplify=TODO \
  dynamic=TODO \
  half=TODO \
  nms=TODO
```

For a custom trained model:

```bash
yolo export \
  model=path/to/best.pt \
  format=onnx \
  imgsz=640 \
  opset=TODO \
  simplify=TODO \
  dynamic=TODO \
  half=TODO \
  nms=TODO
```

If your pipeline also uses a classifier, export that model separately:

```bash
yolo export \
  model=yolov8n-cls.pt \
  format=onnx \
  imgsz=224 \
  opset=TODO \
  simplify=TODO \
  dynamic=TODO \
  half=TODO
```

<!-- TODO: Confirm the exact export command used in the demo, including imgsz, opset, simplify, dynamic, half, and NMS settings. -->
<!-- TODO: If classification is part of the canonical pipeline, replace the classifier command with the exact model name and input size. -->

Expected output:

```plain
yolov8n.onnx
```

Before importing the model, keep the model metadata together:

```plain
model/
  yolov8n.onnx
  labels.txt
  README.md
```

Example `labels.txt`:

```plain
person
bicycle
car
...
```

<!-- TODO: Replace the label example with the exact labels used by the demo model. -->

Step 1 should leave enough evidence for troubleshooting:

- The exact `yolo export` command.
- The generated `.onnx` file name.
- ONNX input shape and output tensor names.
- `ultralytics`, `onnx`, and `onnxruntime` package versions.
- A quick ONNX Runtime smoke test result if available.

## Step 2: Import the Model into CosmoEdge

Open the CosmoEdge web console and go to **Model Repository**.

For an x86 validation path, import the ONNX model directly if the model type is supported by the current runtime. For an NPU deployment path, convert the ONNX model to the target runtime format first, then import the converted model package.

Recommended metadata:

| Field | Example | Notes |
| --- | --- | --- |
| Main type | Detection | Use Classification for classifier models. |
| Subtype | YOLO detection | Select the template that matches the model family. |
| Input size | `640 x 640` | Must match the export shape. |
| Normalization | `0-1` | Confirm against the model preprocessing path. |
| Color channel | `RGB` | Confirm whether the exported model expects RGB or BGR. |
| Labels | `labels.txt` | Must match model output order. |
| Runtime backend | `ONNX Runtime CPU` or `NPU runtime package` | Record the exact runtime and conversion version. |

<!-- TODO: Add clean screenshots: Model Repository list, import dialog, and model configuration page. -->
<!-- TODO: Confirm exact UI field names in the English UI. -->

Post-import checklist:

- The model appears in the Model Repository.
- The model type and input size are correct.
- Labels render as names, not only numeric class IDs.
- A small image test returns expected boxes before the model is used in a video pipeline.

## Step 3: Build the Visual Pipeline

Create or open a scenario task, then enter the visual pipeline editor.

A minimal detection pipeline usually looks like this:

```plain
Video Decode
  -> Object Detection
  -> Object Tracking
  -> Region / Rule Logic
  -> Event Reporting
  -> OSD Rendering
```

A two-stage detection plus classification pipeline usually looks like this:

```plain
Video Decode
  -> Object Detection
  -> Object Tracking
  -> Crop / Target Mapping
  -> Attribute Classification
  -> Region / Rule Logic
  -> Event Reporting
  -> OSD Rendering
```

<!-- TODO: Attach the exported pipeline layout file used by the demo, if available. -->
<!-- TODO: Add a clean full-pipeline screenshot from the English UI. -->

Key configuration points:

| Node | What to verify |
| --- | --- |
| Detection | Model selection, confidence threshold, NMS / IOU threshold, class labels. |
| Tracking | Track ID stability, target handoff to downstream nodes. |
| Classification | Crop source, classifier model, class mapping, threshold. |
| Region / Rule Logic | Region name, direction, min size, alarm sensitivity, dwell time if applicable. |
| Event Reporting | Event type, snapshot setting, metadata fields. |
| OSD Rendering | Boxes, labels, regions, debug latency overlay, event popups. |

## Step 4: Bind a Video Source

Create or select a video source, then bind the scenario task to that channel.

Typical steps:

1. Add a local video, RTSP stream, or camera channel.
2. Assign the scenario task to the channel.
3. Draw the required region or line on the preview.
4. Configure the runtime strategy, such as target FPS and alarm interval.
5. Save and start analysis.

<!-- TODO: Add screenshots: video source creation, scenario assignment, region drawing, and runtime strategy. -->

## Step 5: Verify Live OSD and Events

Open the live analysis page and enable the relevant OSD overlay.

The expected OSD result is:

- Bounding boxes are drawn on detected targets.
- Class labels and confidence values are visible.
- Regions or lines are rendered on the video.
- Event records appear when rule conditions are met.
- Optional debug overlays show decode, inference, OSD, and encode latency.

The expected event output is:

- Scenario task ID or algorithm code is present.
- Channel ID or video source name is present.
- Detected class names match the imported labels.
- Snapshot or frame reference is available when event snapshots are enabled.
- Event timestamps line up with the visible OSD result.

<!-- TODO: Add a clean final OSD screenshot from the public demo. -->
<!-- TODO: Add a small sanitized event payload or Event Center screenshot. -->

For downstream integration, use the MQTT or HTTP webhook references:

- [MQTT Reference](../../reference/mqtt.md)
- [HTTP Webhook Reference](../../reference/webhook.md)

## Benchmark Snapshot

![ScenarioBench benchmark card](../../../assets/community/cosmoedge-scenariobench-card.png)

The ScenarioBench v1.0 public benchmark includes CV workloads using Ultralytics YOLO model sources.

Benchmark environment summary:

- Detection model: `YOLOv8n.pt`
- Classification model: `YOLOv8n-cls.pt`
- Model source: Ultralytics documentation
- Test video: `data/test-video/Safety Helmet.mp4`
- NPU hardware profile: `npu-yy-16t01-preview`
- NPU CosmoEdge software version: `V1.0.0.0`
- x86 baseline CosmoEdge software version: `V0.1.0.0`

Published results:

| Scenario | Max verified video channels | Concurrent scenario tasks | Target FPS | Result |
| --- | ---: | ---: | ---: | --- |
| No Safety Helmet | 16 | 16 | 3 | PASS |
| Pedestrian Detection | 16 | 16 | 5 | PASS |
| Pedestrian + No Safety Helmet | 16 | 32 | 3 | PASS |
| x86 CPU baseline | 7 | 7 | 3 | LIMITED |

These numbers report verified stable capacity within the published benchmark range. They are not a claimed hardware upper bound.

Evidence:

- [ScenarioBench v1.0 Summary](../../../benchmarks/scenario-bench/v1.0/README.md)
- [Benchmark Environment](../../../benchmarks/scenario-bench/v1.0/environment.md)
- [Mixed NPU Report](../../../benchmarks/scenario-bench/v1.0/pedestrian-helmet-mixed-npu/report.html)
- [Pedestrian Detection NPU Report](../../../benchmarks/scenario-bench/v1.0/pedestrian-45626-npu/report.html)
- [No Safety Helmet NPU Report](../../../benchmarks/scenario-bench/v1.0/helmet-7463-npu/report.html)
- [No Safety Helmet x86 Baseline Report](../../../benchmarks/scenario-bench/v1.0/helmet-7463-x86/report.html)

<!-- TODO: Confirm whether benchmark report links should point to repository paths, documentation-site URLs, or release assets. -->

## Common Issues

| Symptom | Likely cause | What to check |
| --- | --- | --- |
| Class names show as numbers | Label mapping is missing or in the wrong order. | Recheck `labels.txt` and Model Repository class config. |
| No boxes in live video | Model input shape or preprocessing differs from export settings. | Confirm image size, normalization, color channel, and resize mode. |
| Boxes appear in image test but not in video | Pipeline node wiring or OSD node is incomplete. | Reopen the scenario task and verify node order. |
| Detection works but events do not appear | Rule or region logic is not configured. | Check region drawing, sensitivity, alarm interval, and event reporting node. |
| Throughput is lower than expected | Running on CPU path or target FPS is too high. | Confirm hardware backend, runtime target FPS, and concurrent channel count. |
| ONNX import fails | Unsupported operator or mismatched export settings. | Re-export with the recommended Ultralytics command and validate with ONNX Runtime. |

## What to Share When Asking for Help

When asking for help in the Ultralytics or CosmoEdge community, include:

- `ultralytics`, `onnx`, and `onnxruntime` versions.
- Model family, model file name, and model source.
- Exact `yolo export` command, including `imgsz`, `opset`, `simplify`, `dynamic`, `half`, and `nms`.
- ONNX input shape and output tensor names.
- Label mapping.
- Target hardware, runtime backend, and CosmoEdge version.
- Pipeline structure or exported pipeline layout.
- Validation input, such as video file, resolution, source FPS, and target FPS.
- Validation output, such as Model Repository config, live OSD screenshot, event payload, and benchmark report.

This makes it much easier to distinguish model export issues from runtime, preprocessing, pipeline, or rule-logic issues.

## Next Steps

- Read [Volume 5: Model Porting](../05-model-porting/model-porting.md) for the full model import and conversion workflow.
- Read [Volume 4: Pipeline Orchestration](../04-pipeline-orchestration/pipeline-orchestration.md) for custom pipeline composition.
- Read [Models and Resources](../../reference/models.md) to understand available templates.
- Read [Deployment Guide](../../guide/deployment.md) before moving from a trial environment to a production device.
