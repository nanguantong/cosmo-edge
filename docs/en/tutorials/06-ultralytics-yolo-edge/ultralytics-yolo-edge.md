---
title: "Deploy Ultralytics YOLO with CosmoEdge"
description: Turn an Ultralytics YOLO model export into a visual edge video analytics pipeline with model management, OSD, events, and benchmark references.
prev:
  text: "Volume 5: Model Porting"
  link: /en/tutorials/05-model-porting/model-porting
next: false
---

# Deploy Ultralytics YOLO with CosmoEdge

> **Status**: Draft for review.
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

Install Ultralytics in your model preparation environment:

```bash
pip install ultralytics
```

Export a YOLO detection model to ONNX:

```bash
yolo export model=yolov8n.pt format=onnx imgsz=640
```

For a custom trained model:

```bash
yolo export model=path/to/best.pt format=onnx imgsz=640
```

If your pipeline also uses a classifier, export that model separately:

```bash
yolo export model=yolov8n-cls.pt format=onnx imgsz=224
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

The expected result is:

- Bounding boxes are drawn on detected targets.
- Class labels and confidence values are visible.
- Regions or lines are rendered on the video.
- Event records appear when rule conditions are met.
- Optional debug overlays show decode, inference, OSD, and encode latency.

<!-- TODO: Add a clean final OSD screenshot from the public demo. -->

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
- [No Safety Helmet NPU Report](../../../benchmarks/scenario-bench/v1.0/helmet-7463-npu/report.html)

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

- Ultralytics version.
- Model family and export command.
- ONNX input shape.
- Label mapping.
- Target hardware and CosmoEdge version.
- Pipeline structure.
- Screenshot of the Model Repository config.
- Screenshot of the live OSD or failed result.

This makes it much easier to distinguish model export issues from runtime, preprocessing, pipeline, or rule-logic issues.

## Next Steps

- Read [Volume 5: Model Porting](../05-model-porting/model-porting.md) for the full model import and conversion workflow.
- Read [Volume 4: Pipeline Orchestration](../04-pipeline-orchestration/pipeline-orchestration.md) for custom pipeline composition.
- Read [Models and Resources](../../reference/models.md) to understand available templates.
- Read [Deployment Guide](../../guide/deployment.md) before moving from a trial environment to a production device.
