---
title: Community Case Template
description: Template for contributor-submitted CosmoEdge community cases.
---

# Community Case Template

Copy this structure when proposing a new community case. Replace placeholder text and remove sections that do not apply.

## Summary

- Case name:
- Contributor:
- Target user:
- What this case demonstrates:
- Related issue or discussion:

## Status

| Field | Value |
| --- | --- |
| Review status | Proposed |
| CosmoEdge version | TODO |
| Platform | TODO, for example x86 Docker, Sophon BM1688, Windows Docker Desktop |
| Runtime backend | TODO |
| Last verified | TODO |

## Assets and Licensing

| Asset | Source | License or permission | Repository status |
| --- | --- | --- | --- |
| Model | TODO | TODO | Link only / release asset / not included |
| Video or images | TODO | TODO | Link only / release asset / not included |
| Screenshots | Contributor-provided | TODO | Included after sanitization |

Do not commit large model weights, private download links, credentials, customer data, or private datasets unless maintainers explicitly approve them.

## Environment

| Item | Value |
| --- | --- |
| OS | TODO |
| Docker version | TODO |
| Docker Compose version | TODO |
| CPU / NPU / GPU | TODO |
| Browser | TODO |

## Model Metadata

| Item | Value |
| --- | --- |
| Model file | TODO |
| Model family | TODO, for example YOLOv8 detection |
| Input shape | TODO |
| Labels / classes | TODO |
| Preprocessing | TODO, for example RGB, resize mode, normalization |
| Output / post-processing | TODO, for example boxes, scores, NMS assumptions |

## Workflow

Describe only the steps needed to reproduce this case.

1. Prepare the model and labels.
2. Import or configure the model in CosmoEdge.
3. Create or choose a scenario task.
4. Bind the task to a video source or image input.
5. Verify live results and event records.

## Validation

Record what was actually tested.

| Check | Result | Evidence |
| --- | --- | --- |
| Model imports successfully | TODO | Screenshot or log |
| Labels render correctly | TODO | Screenshot |
| Live OSD shows expected output | TODO | Screenshot or video |
| Events are recorded | TODO | Screenshot or payload |

## Known Limits

- TODO

## References

- TODO
