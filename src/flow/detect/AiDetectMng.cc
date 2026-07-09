// AiDetectMng.cc — fps-aware instance placement for AiDetector.
//
// Hides (not overrides) MultiChannelActionMng::GetInst. Placement rules under m_mtx:
//   1) an existing channel always reuses its instance (single inference per channel);
//   2) unconfigured fps (<=0, full-frame) → exclusive new instance;
//   3) otherwise the first existing instance that fits under the channel cap and fps budget;
//   4) else create a new instance.

#include "flow/detect/AiDetectMng.h"

#include "util/Log.h"

namespace cosmo {

namespace {
    constexpr const char* kTag = "AiDetectMng";
}

AiDetectorPtr AiDetectMng::GetInst(const std::string& algCode, const std::string& channelId,
                                   const std::string& task, ActionNode& action) {
    const float requested_fps  = action.initFps;
    const float normalized_fps = ai_detector_fps::NormalizeRequestedFps(requested_fps);
    const bool exclusive       = (requested_fps <= 0.0f);

    std::lock_guard<std::shared_mutex> lock(m_mtx);
    auto& algInsts = m_insts[algCode];

    auto applyParams = [&action](const AiDetectorPtr& inst, const std::string& ch, const std::string& t) {
        if (!action.configObject.params.empty()) {
            inst->ModifyParam(ch, t, action.configObject.params);
        }
    };

    // Rule 1: existing channel → reuse. Splitting one channel across instances would duplicate
    // inference and break history/overview/confidence activation, so fps budget never rejects it.
    for (size_t i = 0; i < algInsts.size(); ++i) {
        const auto& inst = algInsts[i];
        if (inst->ChannelExist(channelId)) {
            if (!inst->AddTask(channelId, task, requested_fps)) {
                LOG_WARN("[{}] Channel:{} Task:{} Get {} Inst Failed", kTag, channelId, task, algCode);
                return nullptr;
            }
            applyParams(inst, channelId, task);
            LOG_INFO(
                "[{}] Place {} ch:{} task:{} reqFps:{} normFps:{} -> reuse-existing idx:{} chs:{} "
                "assignedFps:{} budget:{} runtimeInFps:{}",
                kTag, algCode, channelId, task, requested_fps, normalized_fps, i, inst->ChannelCount(),
                inst->AssignedFps(), inst->GetInstanceFpsBudget(), inst->GetInFps());
            return inst;
        }
    }

    // Rule 2: unconfigured fps → exclusive new instance (load unknown, stay conservative).
    if (exclusive) {
        auto inst = std::make_shared<AiDetector>(action);
        if (!inst->AddTask(channelId, task, requested_fps)) {
            LOG_WARN("[{}] Channel:{} Task:{} Get {} Inst Failed", kTag, channelId, task, algCode);
            return nullptr;
        }
        applyParams(inst, channelId, task);
        algInsts.push_back(inst);
        LOG_INFO(
            "[{}] Place {} ch:{} task:{} reqFps:{} normFps:{} -> create-exclusive idx:{} chs:{} "
            "assignedFps:{} budget:{} runtimeInFps:{}",
            kTag, algCode, channelId, task, requested_fps, normalized_fps, algInsts.size() - 1,
            inst->ChannelCount(), inst->AssignedFps(), inst->GetInstanceFpsBudget(), inst->GetInFps());
        return inst;
    }

    // Rule 3: first existing instance that fits under both the channel cap and the fps budget.
    for (size_t i = 0; i < algInsts.size(); ++i) {
        const auto& inst            = algInsts[i];
        const float assigned_before = inst->AssignedFps();
        const float delta           = inst->DeltaFpsForTask(channelId, requested_fps);
        if (inst->CanAccept(channelId, requested_fps)) {
            if (!inst->AddTask(channelId, task, requested_fps)) {
                LOG_WARN("[{}] Channel:{} Task:{} Get {} Inst Failed", kTag, channelId, task, algCode);
                return nullptr;
            }
            applyParams(inst, channelId, task);
            LOG_INFO(
                "[{}] Place {} ch:{} task:{} reqFps:{} normFps:{} -> accept-existing idx:{} chs:{} "
                "assignedFpsBefore:{} delta:{} budget:{} runtimeInFps:{}",
                kTag, algCode, channelId, task, requested_fps, normalized_fps, i, inst->ChannelCount(),
                assigned_before, delta, inst->GetInstanceFpsBudget(), inst->GetInFps());
            return inst;
        }
    }

    // Rule 4: no existing instance fits → create a new one.
    auto inst = std::make_shared<AiDetector>(action);
    if (!inst->AddTask(channelId, task, requested_fps)) {
        LOG_WARN("[{}] Channel:{} Task:{} Get {} Inst Failed", kTag, channelId, task, algCode);
        return nullptr;
    }
    applyParams(inst, channelId, task);
    algInsts.push_back(inst);
    LOG_INFO(
        "[{}] Place {} ch:{} task:{} reqFps:{} normFps:{} -> create-new idx:{} chs:{} assignedFps:{} "
        "budget:{} runtimeInFps:{}",
        kTag, algCode, channelId, task, requested_fps, normalized_fps, algInsts.size() - 1,
        inst->ChannelCount(), inst->AssignedFps(), inst->GetInstanceFpsBudget(), inst->GetInFps());
    return inst;
}

}  // namespace cosmo
