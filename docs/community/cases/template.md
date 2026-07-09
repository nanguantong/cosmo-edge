---
title: 社区案例模板
description: CosmoEdge 社区操作案例的提交模板。
---

# 社区案例模板

提交新的社区案例时，请复制这个结构。填写占位内容，并删除不适用的小节。

## 摘要

- 案例名称：
- 贡献者：
- 目标用户：
- 本案例演示内容：
- 关联 issue 或 discussion：

## 状态

| 字段 | 值 |
| --- | --- |
| 审核状态 | Proposed |
| CosmoEdge 版本 | TODO |
| 平台 | TODO，例如 x86 Docker、Sophon BM1688、Windows Docker Desktop |
| 运行后端 | TODO |
| 最近验证时间 | TODO |

## 素材与许可

| 素材 | 来源 | 许可或授权 | 仓库处理方式 |
| --- | --- | --- | --- |
| 模型 | TODO | TODO | 仅链接 / release asset / 不包含 |
| 视频或图片 | TODO | TODO | 仅链接 / release asset / 不包含 |
| 截图 | 贡献者提供 | TODO | 脱敏后入库 |

除非维护者明确批准，不要提交大模型权重、私有下载链接、凭据、客户数据或私有数据集。

## 环境

| 项目 | 值 |
| --- | --- |
| 操作系统 | TODO |
| Docker 版本 | TODO |
| Docker Compose 版本 | TODO |
| CPU / NPU / GPU | TODO |
| 浏览器 | TODO |

## 模型元数据

| 项目 | 值 |
| --- | --- |
| 模型文件 | TODO |
| 模型类型 | TODO，例如 YOLOv8 detection |
| 输入尺寸 | TODO |
| 标签 / 类别 | TODO |
| 预处理 | TODO，例如 RGB、缩放方式、归一化 |
| 输出 / 后处理 | TODO，例如框、分数、NMS 假设 |

## 操作流程

只描述复现本案例所需的步骤。

1. 准备模型和标签。
2. 在 CosmoEdge 中导入或配置模型。
3. 创建或选择场景任务。
4. 将任务绑定到视频源或图片输入。
5. 验证实时结果和事件记录。

## 验证

记录实际完成的验证。

| 检查项 | 结果 | 证据 |
| --- | --- | --- |
| 模型成功导入 | TODO | 截图或日志 |
| 标签显示正确 | TODO | 截图 |
| 实时 OSD 符合预期 | TODO | 截图或视频 |
| 事件正常记录 | TODO | 截图或 payload |

## 已知限制

- TODO

## 参考

- TODO
