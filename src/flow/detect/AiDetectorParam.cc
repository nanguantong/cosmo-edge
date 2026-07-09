// AiDetectorParam.cc — Parameter management and confidence filtering for AiDetector.
// Split from AiDetector.cc to reduce file size (DEBT-007).

#include <algorithm>

#include "flow/detect/AiDetector.h"
#include "util/Keys.h"
#include "util/Log.h"
#include "util/SafeParse.h"
#include "util/dto/ActionCodes.h"

static constexpr const char* kTag = "AI-DETECTER ";
namespace cosmo {

bool AiDetector::ValidKey(MsgDynamicKeyValue& param) {
    if (param.keys.empty()) {
        LOG_WARN(
            "ModifyParam "
            "[{} {}] param.keys is Empty",
            name_, uuid);
        return false;
    }
    if (param.keys.size() != 3) {
        LOG_DEBUG(
            "ModifyParam "
            "[{} {}] Set {} Failed. key size:{}",
            name_, uuid, param.key, param.keys.size());
        return false;
    }

    if (key::AI_PARAM != param.keys[0]) {
        LOG_DEBUG(
            "ModifyParam "
            "[{} {}] param.keys[0]:{} is Not {}",
            name_, uuid, param.keys[0], key::AI_PARAM);
        return false;
    }

    return true;
}

bool AiDetector::SetConfidenceToLocal(const std::string& /*channel_id*/, const std::string& taskId,
                                      AiConfidence& confidence) {
    for (auto& param : params_.param) {
        if (taskId == param.task_id) {
            auto it = std::find_if(param.label_params.begin(), param.label_params.end(),
                                   [&confidence](const auto& lp) { return confidence.label == lp.label; });
            if (it != param.label_params.end()) {
                LOG_INFO(
                    "ModifyParam "
                    " Task:{} Detect Confidence {} Modify From {} To {}",
                    taskId, confidence.label, it->confidence, confidence.confidence);
                it->confidence = confidence.confidence;
                return true;
            }
            AiLabelParam labelParam;
            labelParam.label      = confidence.label;
            labelParam.confidence = confidence.confidence;
            param.label_params.push_back(labelParam);
            LOG_INFO(
                "ModifyParam "
                " Task:{} Detect Confidence Add {} Value {}",
                taskId, confidence.label, confidence.confidence);
            return true;
        }
    }

    AiDetectorParamEl detParam;
    detParam.task_id = taskId;
    AiLabelParam labelParam;
    labelParam.label      = confidence.label;
    labelParam.confidence = confidence.confidence;
    detParam.label_params.push_back(labelParam);
    params_.param.push_back(detParam);
    LOG_INFO(
        "ModifyParam "
        " Add Task:{} Detect Confidence {} Value {}",
        taskId, confidence.label, confidence.confidence);

    return true;
}

bool AiDetector::SetTargetPosToLocal(const std::string& /*channel_id*/, const std::string& taskId,
                                     const std::string& in_label, TargetPosition pos) {
    for (auto& param : params_.param) {
        if (taskId == param.task_id) {
            auto it = std::find_if(param.label_params.begin(), param.label_params.end(),
                                   [&in_label](const auto& lp) { return in_label == lp.label; });
            if (it != param.label_params.end()) {
                LOG_INFO(
                    "ModifyParam "
                    " Task:{} Detect detPos {} Modify From {} To {}",
                    taskId, it->label, it->pos, pos);
                it->pos = pos;
                return true;
            }
            AiLabelParam labelParam;
            labelParam.label = in_label;
            labelParam.pos   = pos;
            param.label_params.push_back(labelParam);
            LOG_INFO(
                "ModifyParam "
                " Task:{} Detect detPos Add {} Value {} WARNING Need Confidence",
                taskId, labelParam.label, labelParam.pos);
            return true;
        }
    }

    AiDetectorParamEl detParam;
    detParam.task_id = taskId;
    AiLabelParam labelParam;
    labelParam.label = in_label;
    labelParam.pos   = pos;
    detParam.label_params.push_back(labelParam);
    params_.param.push_back(detParam);
    LOG_INFO(
        "ModifyParam "
        " Add Task:{} Detect detPos {} Value {} WARNING Need Confidence",
        taskId, labelParam.label, labelParam.pos);

    return true;
}

bool AiDetector::AnalysisKey(const std::string& channel_id, const std::string& taskId,
                             MsgDynamicKeyValue& param) {
    if (!ValidKey(param)) {
        return false;
    }

    if (param.keys[2] == key::CONFIDENCE) {
        AiConfidence confidence;
        confidence.label      = param.keys[1];
        confidence.confidence = util::ParseFloat(param.value);
        return SetConfidenceToLocal(channel_id, taskId, confidence);
    }
    if (param.keys[2] == key::DET_POSITION) {
        TargetPosition value = static_cast<TargetPosition>(util::ParseInt(param.value));
        return SetTargetPosToLocal(channel_id, taskId, param.keys[1], value);
    } else {
        LOG_DEBUG(
            "ModifyParam "
            "[{} {}] param.key {} value {} is Unknown",
            name_, uuid, param.key, param.value);
        return false;
    }
}

// Modify parameters — update on top of existing parameters
bool AiDetector::ModifyParam(const std::string& channel_id, const std::string& taskId,
                             std::vector<MsgDynamicKeyValue>& params) {
    int modifyCount = 0;
    std::lock_guard<std::shared_mutex> lock(mtx);
    for (auto& param : params) {
        auto ret = AnalysisKey(channel_id, taskId, param);
        modifyCount += ret ? 1 : 0;
    }
    if (modifyCount) {
        params_.param_modify_sign += 1;
    }
    return false;
}

// Set parameters — clear previous parameters, set new ones
bool AiDetector::SetParam(const std::string& channel_id, const std::string& taskId,
                          std::vector<MsgDynamicKeyValue>& params) {
    std::lock_guard<std::shared_mutex> lock(mtx);
    params_ = {};
    for (auto& param : params) {
        AnalysisKey(channel_id, taskId, param);
    }
    params_.param_modify_sign += 1;
    return true;
}

// Set areas — clear previous areas, set new ones
bool AiDetector::SetArea(const std::string& /*channel_id*/, const std::string& taskId,
                         std::vector<MsgTaskArea>& areas, std::vector<MsgTaskArea>& shieldedAreas) {
    std::lock_guard<std::shared_mutex> lock(mtx);
    auto& taskArea         = task_areas_[taskId];
    taskArea.taskId        = taskId;
    taskArea.areas         = areas;
    taskArea.shieldedAreas = shieldedAreas;
    return true;
}

AiDetectorParamEl AiDetector::FoundLocalParamByTask(const AlgTaskUnit& task) {
    AiDetectorParamEl param;
    param.task_id     = task.task_id;
    float coefficient = 1.0f;
    if (AATrack_Code == task.actionId) {
        coefficient = 0.7f;
    }
    auto it = std::find_if(params_.param.begin(), params_.param.end(),
                           [&task](const auto& lp) { return task.task_id == lp.task_id; });
    if (it != params_.param.end()) {
        for (auto& localParamConf : it->label_params) {
            AiLabelParam labelParam;
            labelParam.label      = localParamConf.label;
            labelParam.confidence = localParamConf.confidence * coefficient;
            labelParam.pos        = localParamConf.pos;
            LOG_INFO("{}[{} {}] BindTask {} action:{} confidence:{}/{}. coefficient:{} detPostion:{}", kTag,
                     name_, uuid, task.task_id, task.actionId, labelParam.label, labelParam.confidence,
                     coefficient, labelParam.pos);
            param.label_params.push_back(labelParam);
        }
        return param;
    }

    return param;
}

struct AiTaskConfidenceEl {
    std::string task_id;
    AiConfidence confidence;
};

void AiDetector::FindActiveConfidence(const std::vector<AiDetectorParamEl>& shouldActiveParams) {
    std::vector<AiTaskConfidenceEl> taskConfidences;
    active_confidence_.clear();

    for (auto& label : labels_) {
        AiTaskConfidenceEl labelMinConfidence;
        labelMinConfidence.confidence.confidence = 100.0f;
        labelMinConfidence.confidence.label      = label;

        for (auto& taskParam : shouldActiveParams) {
            for (auto& taskConfidence : taskParam.label_params) {
                if (taskConfidence.label == label) {
                    if (labelMinConfidence.confidence.confidence > taskConfidence.confidence) {
                        labelMinConfidence.confidence.confidence = taskConfidence.confidence;
                        labelMinConfidence.task_id               = taskParam.task_id;
                    }
                }
            }
        }

        if ((labelMinConfidence.confidence.confidence >= 0.0f) &&
            (labelMinConfidence.confidence.confidence <= 1.0f)) {
            AiConfidence confidence;
            confidence.confidence = labelMinConfidence.confidence.confidence;
            confidence.label      = labelMinConfidence.confidence.label;
            LOG_INFO("{}[{} {}] {} Confidence From Task {} Value Should Be {}.", kTag, name_, uuid,
                     labelMinConfidence.confidence.label, labelMinConfidence.task_id,
                     labelMinConfidence.confidence.confidence);
            taskConfidences.push_back(labelMinConfidence);
            active_confidence_.push_back(confidence);
            active_confidence_modify_sign_ += 1;
        }
    }
}

void AiDetector::CheckAndActivateConfidence() {
    std::vector<AiDetectorParamEl> shouldActiveParams;
    if ((distributor->GetSign() != sign_register_) ||
        (params_.param_active_sign != params_.param_modify_sign)) {
        active_task_confidence_.clear();
        sign_register_            = distributor->GetSign();
        params_.param_modify_sign = params_.param_active_sign;
        LOG_INFO("{}[{} {}] Confidence Should Be Active.", kTag, name_, uuid);
        auto bindTasks = distributor->GetBindTasks();
        {
            std::shared_lock<std::shared_mutex> lock(mtx);
            for (auto& channelTask : bindTasks) {
                for (auto& task : channelTask.tasks) {
                    // Per-task confidence with tracking coefficient (0.7) for ConfidenceFilter.
                    // Tracking needs a lower threshold to keep candidates that might re-associate;
                    // this lowered value is correct for per-task filtering but must NOT leak to
                    // the detector's global SetThreshold.
                    auto taskConfidence = FoundLocalParamByTask(task);
                    LOG_INFO("{}[{} {}] BindTask {}/{}.", kTag, name_, uuid, task.channel_id, task.task_id);
                    active_task_confidence_.push_back(taskConfidence);

                    // Global detector threshold: use raw user-configured values without the
                    // tracking coefficient.  The 0.7× discount is a per-task tracking concern;
                    // applying it to SetThreshold would silently lower the model's effective
                    // detection threshold below what the user configured.
                    auto it = std::find_if(params_.param.begin(), params_.param.end(),
                                           [&task](const auto& lp) { return task.task_id == lp.task_id; });
                    if (it != params_.param.end()) {
                        shouldActiveParams.push_back(*it);
                    }
                }
            }
        }
        FindActiveConfidence(shouldActiveParams);
    }
}

AiDetectorParamEl AiDetector::GetTaskLabelParams(const std::string& taskId) {
    auto it = std::find_if(active_task_confidence_.begin(), active_task_confidence_.end(),
                           [&taskId](const auto& el) { return el.task_id == taskId; });
    if (it != active_task_confidence_.end()) {
        return *it;
    }

    return {};
}

TargetPosition AiDetector::GetTaskLabelPos(const std::string& label, const AiDetectorParamEl& taskLabels) {
    auto it = std::find_if(taskLabels.label_params.begin(), taskLabels.label_params.end(),
                           [&label](const auto& lp) { return lp.label == label; });
    if (it != taskLabels.label_params.end()) {
        return it->pos;
    }

    return TargetPosition::kBottom;  // default
}

}  // namespace cosmo
