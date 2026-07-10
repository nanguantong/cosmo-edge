---
title: Community Recipes and Cases
description: Real-world CosmoEdge workflows contributed by the community.
---

# Community Recipes and Cases

This area is for practical workflows from contributors, integrators, and users. Good candidates include:

- Importing a specific ONNX model on x86.
- Reproducing a model workflow with a public dataset or demo video.
- Integrating CosmoEdge with a specific downstream system.
- Field notes that help others avoid environment or pipeline pitfalls.

Community cases should be scoped and reproducible. They should not replace the official tutorials.

## Case List

- [Import and run YOLOv8 cat/dog detection ONNX model on x86](yolov8catdog-x86.md)

## Add a Case

Start from the [case template](template.md). Use one Markdown file per case:

```text
docs/en/community/cases/<case-slug>.md
docs/community/cases/<case-slug>.md
```

Put images and small supporting assets under:

```text
docs/assets/community/<case-slug>/
```

Large model weights, private links, and private datasets must stay out of the repository unless maintainers explicitly approve them.
