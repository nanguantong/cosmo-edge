// DinoDetectorParam.cc — Parameter and task management for DinoDetector.
// Split from DinoDetector.cc to reduce file size (DEBT-007).

#include <unistd.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <thread>

#include "flow/common/AlgDataRecord.h"
#include "flow/detect/DinoDetector.h"
#include "media/VideoFrame.h"
#include "service/detail/ServiceRegistry.h"
#include "service/model/IModelPathMapping.h"
#include "service/model/IModelService.h"
#include "util/GeometricPos.h"
#include "util/Log.h"
#include "util/TimeUtil.h"
#include "util/TimingConstants.h"
#include "util/UuidUtil.h"
#include "util/dto/ActionCodes.h"

static constexpr const char* kTag = "DINO-DETECTER ";
namespace cosmo {

bool DinoDetector::TaskExist(const std::string& channel_id, const std::string& task) const {
    std::lock_guard<std::shared_mutex> lock(mtx);
    return std::any_of(channel_list_.begin(), channel_list_.end(), [&channel_id, &task](const auto& ch) {
        if (ch.channel != channel_id)
            return false;
        return std::any_of(ch.tasks.begin(), ch.tasks.end(), [&task](const auto& t) { return t == task; });
    });
}

bool DinoDetector::TaskIsFull() const {
    std::lock_guard<std::shared_mutex> lock(mtx);
    return channel_list_.size() >= max_reuse_count_;
}

bool DinoDetector::TaskIsEmpty() const {
    std::lock_guard<std::shared_mutex> lock(mtx);
    return channel_list_.empty();
}

size_t DinoDetector::ChannelCount() const {
    std::lock_guard<std::shared_mutex> lock(mtx);
    return channel_list_.size();
}

size_t DinoDetector::TaskCount() const {
    std::lock_guard<std::shared_mutex> lock(mtx);
    size_t count = 0;
    for (auto& ch : channel_list_) {
        count += ch.tasks.size();
    }
    return count;
}

DinoDetectorParamEl DinoDetector::FoundLocalParamByTask(const AlgTaskUnit& task) const {
    DinoDetectorParamEl param;
    param.task_id = task.task_id;
    return param;
}

DinoDetectorParamEl DinoDetector::GetTaskParamsUnlocked(const std::string& taskId) const {
    auto it = std::find_if(params_.param.begin(), params_.param.end(),
                           [&taskId](const auto& param) { return param.task_id == taskId; });
    if (it != params_.param.end()) {
        return *it;
    }
    // If no exact taskId match and only one task param exists, use it (avoid default fallback)
    if (params_.param.size() == 1)
        return params_.param[0];
    DinoDetectorParamEl emptyParam;
    return emptyParam;
}

DinoDetectorParamEl DinoDetector::GetTaskParams(const std::string& taskId) const {
    std::lock_guard<std::shared_mutex> lock(mtx);
    return GetTaskParamsUnlocked(taskId);
}

void DinoDetector::HandFrames(std::vector<AlgDataPtr> alg_datas) {
    if (!detector_) {
        action_status = util::ErrorEnum::AI_INST_NOTCREATED;
        if (!DinoSdkInit())
            return;
    }

    // Collect valid frames and per-frame prompts (different tasks may have different prompts)
    std::vector<VideoFramePtr> images;
    std::vector<std::string> prompts;
    std::vector<AlgDataPtr> validDatas;

    for (auto& data : alg_datas) {
        if (!data || !data->chanDataDec.frame || !data->chanDataDec.frame->Active()) {
            continue;
        }
        std::string taskId = data->taskId;
        if (taskId.empty()) {
            std::shared_lock<std::shared_mutex> lockCh(mtx);
            auto it = std::find_if(channel_list_.begin(), channel_list_.end(), [&data](const auto& ch) {
                return ch.channel == data->channelId && !ch.tasks.empty();
            });
            if (it != channel_list_.end()) {
                taskId = it->tasks.front();
            }
        }

        DinoDetectorParamEl taskParam = GetTaskParams(taskId);
        std::string prompt            = taskParam.prompt.empty() ? "person" : taskParam.prompt;

        images.push_back(data->chanDataDec.frame);
        prompts.push_back(prompt);
        validDatas.push_back(data);
    }

    if (images.empty()) {
        return;
    }

    // Group by prompt so each task uses its own detection prompt
    std::map<std::string, std::vector<size_t>> promptToIndices;
    for (size_t i = 0; i < prompts.size(); i++) {
        promptToIndices[prompts[i]].push_back(i);
    }

    std::vector<std::vector<AiDetectRstEl>> resultsByIndex(validDatas.size());

    for (const auto& kv : promptToIndices) {
        const std::string& prompt          = kv.first;
        const std::vector<size_t>& indices = kv.second;
        std::vector<VideoFramePtr> groupImages;
        groupImages.reserve(indices.size());
        std::transform(indices.begin(), indices.end(), std::back_inserter(groupImages),
                       [&images](size_t idx) { return images[idx]; });
        std::vector<std::vector<AiDetectRstEl>> groupResults;
        auto ret = detector_->Detect(groupImages, prompt, groupResults);
        if (util::ErrorEnum::Success != ret) {
            LOG_WARN("{}[{} {}] Detect Failed. prompt:\", kTag{}\" Ret:{}", alg_code_, uuid, prompt, ret);
            continue;
        }
        for (size_t k = 0; k < indices.size() && k < groupResults.size(); k++) {
            resultsByIndex[indices[k]] = std::move(groupResults[k]);
        }
    }

    // Write detection results back in original order and distribute
    for (size_t i = 0; i < validDatas.size(); i++) {
        auto& data                               = validDatas[i];
        data->dataType                           = AlgDataType::ChannelDataDetect;
        data->chanDataDetect.atomicCode          = alg_code_;
        data->chanDataDetect.detRet              = std::make_shared<DataDetTrackClassify>();
        data->chanDataDetect.detRet->streamIndex = images[i]->GetStreamIndex();
        data->chanDataDetect.detRet->frameIndex  = images[i]->GetFrameIndex();
        data->chanDataDetect.detRet->timestamp   = images[i]->GetTimestamp();
        data->chanDataDetect.detRet->picWidth    = images[i]->GetWidth();
        data->chanDataDetect.detRet->picHeight   = images[i]->GetHeight();
        data->chanDataDetect.detRet->targets     = std::move(resultsByIndex[i]);

        distributor->DistributorData(
            data->channelId, data, [this](AlgDataPtr frame, const std::string& taskId) {
                auto outData = AlgDataCopy(frame);
                if (!outData) {
                    return frame;
                }
                outData->taskId = taskId;

                // Filter out low-confidence detections based on task-specific threshold
                if (outData->chanDataDetect.detRet) {
                    DinoDetectorParamEl taskParam;
                    {
                        std::shared_lock<std::shared_mutex> paramLock(mtx);
                        taskParam = GetTaskParamsUnlocked(taskId);
                    }
                    auto& targets = outData->chanDataDetect.detRet->targets;
                    targets.erase(std::remove_if(targets.begin(), targets.end(),
                                                 [&taskParam](const AiDetectRstEl& target) {
                                                     return target.confidence.confidence <
                                                            taskParam.box_confidence;
                                                 }),
                                  targets.end());
                }

                SignTargetAreas(outData, taskId);
                return outData;
            });
    }

    LOG_DEBUG("{}[{} {}] Processed {} frames ({} prompts)", kTag, alg_code_, uuid, validDatas.size(),
              promptToIndices.size());
}

void DinoDetector::TargetAddArea(AiDetectRstEl& target, TargetPosition pos, TargetAreaType type,
                                 MsgTaskArea& area, int pic_width, int pic_height, bool associated_area,
                                 const std::string& main_area_id) {
    if (BoxInArea(target.box, pos, target.point, area.points, pic_width, pic_height)) {
        TargetAreaUnit targetArea;
        targetArea.area_id            = area.areaId;
        targetArea.area_name          = area.name;
        targetArea.is_associated_area = associated_area;
        targetArea.main_area_id       = main_area_id;
        targetArea.position           = pos;
        targetArea.type               = type;
        target.targetPos              = pos;
        if (TargetAreaType::kNormal == type) {
            target.areaSign.areas.push_back(targetArea);
        } else {
            target.areaSign.shielded_areas.push_back(targetArea);
        }
    }
}

void DinoDetector::TargetAddLine(AiDetectRstEl& target, TargetPosition pos, MsgTaskArea& area, int pic_width,
                                 int pic_height, bool associated_area, const std::string& main_area_id) {
    TargetLineUnit targetOnLineUnit;
    targetOnLineUnit.area_id            = area.areaId;
    targetOnLineUnit.area_name          = area.name;
    targetOnLineUnit.is_associated_area = associated_area;
    targetOnLineUnit.main_area_id       = main_area_id;
    targetOnLineUnit.position           = pos;
    targetOnLineUnit.type =
        BoxOnLinePos(target.box, pos, target.point, area.linePoints, pic_width, pic_height);
    target.targetPos = pos;
    target.areaSign.lines.push_back(targetOnLineUnit);
}

void DinoDetector::SignTargetAreas(AlgDataPtr data_ptr, const std::string& taskId) {
    if (!data_ptr || !data_ptr->chanDataDec.frame || !data_ptr->chanDataDetect.detRet) {
        return;
    }

    auto detRet = data_ptr->chanDataDetect.detRet;
    auto width  = data_ptr->chanDataDec.frame->GetWidth();
    auto height = data_ptr->chanDataDec.frame->GetHeight();

    std::shared_lock<std::shared_mutex> lock(mtx);
    auto taskAreaIt = task_areas_.find(taskId);
    if (taskAreaIt != task_areas_.end()) {
        auto& taskArea = taskAreaIt->second;
        if (!taskArea.areas.empty()) {
            detRet->bHaveArea = true;
        }
        if (!taskArea.shieldedAreas.empty()) {
            detRet->bHaveShieldedArea = true;
        }

        for (auto& target : detRet->targets) {
            auto pos = TargetPosition::kBottom;
            for (auto area : taskArea.areas) {
                if (!area.points.empty()) {
                    TargetAddArea(target, pos, TargetAreaType::kNormal, area, width, height, false, "");
                }
                if (!area.linePoints.empty()) {
                    TargetAddLine(target, pos, area, width, height, false, "");
                }
                for (auto assArea : area.associatedAreas) {
                    if (!assArea.points.empty()) {
                        TargetAddArea(target, pos, TargetAreaType::kNormal, assArea, width, height, true,
                                      area.areaId);
                    }
                    if (!assArea.linePoints.empty()) {
                        TargetAddLine(target, pos, assArea, width, height, true, area.areaId);
                    }
                }
            }

            for (auto shieldedArea : taskArea.shieldedAreas) {
                if (!shieldedArea.points.empty()) {
                    TargetAddArea(target, pos, TargetAreaType::kShield, shieldedArea, width, height);
                }
                for (auto assArea : shieldedArea.associatedAreas) {
                    if (!assArea.points.empty()) {
                        TargetAddArea(target, pos, TargetAreaType::kShield, assArea, width, height, true,
                                      shieldedArea.areaId);
                    }
                    if (!assArea.linePoints.empty()) {
                        TargetAddLine(target, pos, assArea, width, height, true, shieldedArea.areaId);
                    }
                }
            }
        }
    }

    OverviewRecord(taskId, detRet);
}

void DinoDetector::AddOverviewTask(const std::string& taskId) {
    auto it = overview_rec_insts_.find(taskId);
    if (it == overview_rec_insts_.end()) {
        auto recordInst             = std::make_shared<OverviewRecordAiRst>(taskId, "detect_dino");
        overview_rec_insts_[taskId] = recordInst;
    }
}

void DinoDetector::OverviewRecord(const std::string& taskId, DataDetTrackClassifyPtr det_ret) {
    auto it                                 = overview_rec_insts_.find(taskId);
    static int64_t s_lastOverviewWriteLogTs = 0;
    auto now                                = util::GetMilliseconds();
    if ((now - s_lastOverviewWriteLogTs) > 1000) {
        s_lastOverviewWriteLogTs = now;
        int targetCnt            = (det_ret ? static_cast<int>(det_ret->targets.size()) : -1);
        int64_t frameIndex       = det_ret ? det_ret->frameIndex : -1;
        int64_t frame_ts         = det_ret ? det_ret->timestamp : -1;
        LOG_INFO("[DINO_OV_WRITE][{}:{}] taskId:{} frame:{} ts:{} targets:{} overviewInst:{}", alg_code_,
                 uuid, taskId, frameIndex, frame_ts, targetCnt,
                 (it != overview_rec_insts_.end() && it->second) ? 1 : 0);
    }
    if (it != overview_rec_insts_.end() && it->second) {
        it->second->OverviewRecordFrame(det_ret);
    }
}

MsgOverviewMem DinoDetector::GetOverviewInfo(const std::string& /*channel_id*/, const std::string& taskId,
                                             int64_t streamIndex, int64_t from, int64_t to) {
    MsgOverviewMem info;
    auto it = overview_rec_insts_.find(taskId);
    if (it != overview_rec_insts_.end() && it->second) {
        info = it->second->GetOverviewInfo(streamIndex, from, to);
    }
    return info;
}

void DinoDetector::run() {
    LOG_INFO("{}[{} {}] Thread Start", kTag, alg_code_, uuid);
    int index = 0;
    while (running) {
        if ((data_queue->RestSize() >= batch_count_) || ((index >= 100) && (data_queue->RestSize() > 0))) {
            size_t detnum     = data_queue->RestSize();
            auto inputFpsCalc = input_fps_calc.FpsWithFrame();
            handle_frame_cnt  = inputFpsCalc.first;
            in_fps            = inputFpsCalc.second;
            std::vector<AlgDataPtr> algDatas;
            for (size_t i = 0; i < detnum && i < batch_count_; i++) {
                auto data = data_queue->Pop();
                if (data) {
                    algDatas.push_back(data);
                }
            }
            if (!algDatas.empty()) {
                duration_stat.BeginSample();
                HandFrames(algDatas);
                duration_stat.EndSample();
            }
            index = 0;
        } else {
            std::this_thread::sleep_for(timing::kFastPollInterval);
            index += 1;
        }
    }
    // Release model on thread exit to free VRAM (HandFrames->DinoSdkInit will reload on next start)
    if (detector_) {
        LOG_INFO("{}[{} {}] Thread exiting, releasing Dino model to free VRAM", kTag, alg_code_, uuid);
        detector_.reset();
        detector_              = nullptr;
        is_detector_inst_init_ = false;
    }
    LOG_INFO("{}[{} {}] Thread Stop", kTag, alg_code_, uuid);
}

}  // namespace cosmo
