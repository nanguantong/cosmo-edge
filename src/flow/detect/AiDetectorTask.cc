// AiDetectorTask.cc — Channel/Task lifecycle management for AiDetector.
// Split from AiDetector.cc to reduce file size (DEBT-007).

#include "flow/detect/AiDetector.h"
#include "util/Log.h"

static constexpr const char* kTag = "AI-DETECTER ";
namespace cosmo {

bool AiDetector::ChannelExist(const std::string& channel_id) const {
    auto iter_find = std::find_if(
        channel_list_.begin(), channel_list_.end(),
        [&](const AiDetectorChannel& channel_node) { return channel_node.channel == channel_id; });

    if (iter_find != channel_list_.end()) {
        return true;
    }

    return false;
}

bool AiDetector::TaskExist(const std::string& channel_id, const std::string& task) const {
    for (const auto& channel_node : channel_list_) {
        if (channel_id == channel_node.channel) {
            return std::any_of(channel_node.tasks.begin(), channel_node.tasks.end(),
                               [&task](const auto& t) { return task == t.task; });
        }
    }
    return false;
}

bool AiDetector::TaskIsFull() const {
    if (channel_list_.size() >= max_reuse_count_) {
        return true;
    }

    return false;
}

bool AiDetector::TaskIsEmpty() const {
    return channel_list_.empty();
}

size_t AiDetector::ChannelCount() const {
    return channel_list_.size();
}

size_t AiDetector::TaskCount() const {
    size_t task_count = 0;
    for (const auto& channel_node : channel_list_) {
        task_count += channel_node.tasks.size();
    }
    return task_count;
}

float AiDetector::NormalizeRequestedFps(float fps) const {
    return ai_detector_fps::NormalizeRequestedFps(fps);
}

float AiDetector::AssignedFps() const {
    return ai_detector_fps::AssignedFps(channel_list_);
}

float AiDetector::DeltaFpsForTask(const std::string& channel_id, float requested_fps) const {
    return ai_detector_fps::DeltaFpsForTask(channel_list_, channel_id, requested_fps);
}

bool AiDetector::CanAccept(const std::string& channel_id, float requested_fps) const {
    return ai_detector_fps::CanAccept(channel_list_, channel_id, requested_fps, max_reuse_count_,
                                      instance_fps_budget_, reuse_profile_);
}

bool AiDetector::AddTask(const std::string& channel_id, const std::string& task) {
    // Legacy entry point: fps unconfigured. NormalizeRequestedFps maps <=0 to the conservative estimate.
    return AddTask(channel_id, task, -1.0f);
}

bool AiDetector::AddTask(const std::string& channel_id, const std::string& task, float requested_fps) {
    const float binding_fps = NormalizeRequestedFps(requested_fps);
    for (auto& channel_node : channel_list_) {
        if (channel_id == channel_node.channel) {
            if (std::any_of(channel_node.tasks.begin(), channel_node.tasks.end(),
                            [&task](const auto& t) { return task == t.task; })) {
                LOG_WARN("{}[{} {}] [{} {}] Add Task. But Task Is Exist", kTag, name_, uuid, channel_id,
                         task);
                return true;
            }
            channel_node.tasks.push_back({task, binding_fps});
            AddOverviewTask(task);
            {
                std::lock_guard<std::shared_mutex> lock(mtx);
                task_histories_[task];
            }
            LOG_INFO("{}[{} {}] Add Task[{}] To Channel:{} Fps:{}.", kTag, name_, uuid, task, channel_id,
                     binding_fps);
            return true;
        }
    }

    AiDetectorChannel newChannel;
    newChannel.channel = channel_id;
    newChannel.tasks.push_back({task, binding_fps});
    channel_list_.push_back(newChannel);

    AddOverviewTask(task);
    {
        std::lock_guard<std::shared_mutex> lock(mtx);
        task_histories_[task];
    }

    LOG_INFO("{}[{} {}] Add New Task[{}] To Channel:{} Fps:{}.", kTag, name_, uuid, task, channel_id,
             binding_fps);
    return true;
}

bool AiDetector::RemoveTask(const std::string& channel_id, const std::string& task) {
    for (auto chIt = channel_list_.begin(); chIt != channel_list_.end(); ++chIt) {
        if (chIt->channel == channel_id) {
            for (auto taskIt = chIt->tasks.begin(); taskIt != chIt->tasks.end(); ++taskIt) {
                if (taskIt->task == task) {
                    chIt->tasks.erase(taskIt);
                    LOG_INFO("{}[{} {}] Remove Task Channel:{} Task:{} left:{}", kTag, name_, uuid,
                             channel_id, task, chIt->tasks.size());

                    if (chIt->tasks.empty()) {
                        channel_list_.erase(chIt);
                        LOG_INFO("{}[{} {}] Remove Channel:{}", kTag, name_, uuid, channel_id);
                    }
                    {
                        std::lock_guard<std::shared_mutex> lock(mtx);
                        task_histories_.erase(task);
                        task_areas_.erase(task);
                        overview_rec_insts_.erase(task);
                    }
                    return true;
                }
            }
            LOG_WARN("{}[{} {}] [{} {}] Remove Task. But Task Is Not Exist", kTag, name_, uuid, channel_id,
                     task);
            return false;
        }
    }

    LOG_WARN("{}[{} {}] [{} {}] Remove Task. But Channel Is Not Exist", kTag, name_, uuid, channel_id, task);
    return false;
}

void AiDetector::SetProcQueSize() {
    size_t que_max_size  = 64;
    size_t que_size_base = 10;
    auto que_in_fps      = data_queue->GetInFps();
    if (que_in_fps <= 5.5f) {
        que_max_size = que_size_base * 2 * media::kQueueSizeCoefficient;
    } else if (que_in_fps <= 10.5f) {
        que_max_size = que_size_base * 5 * media::kQueueSizeCoefficient;
    } else {
        que_max_size = que_size_base * 10 * media::kQueueSizeCoefficient;
    }

    data_queue->SetMaxSize(que_max_size);
}

}  // namespace cosmo
