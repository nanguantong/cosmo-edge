// AiDetectorFps.h — fps-aware instance placement helpers for AiDetector.
//
// Header-only and free of hardware / inference dependencies, so the pure placement math can be
// unit tested by constructing std::vector<AiDetectorChannel> directly, without instantiating an
// AiDetector (whose base AlgActionBase is heavy and device-bound).

#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace cosmo {

// One task bound to a channel, carrying the task's requested fps for instance placement.
struct AiDetectorTaskBinding {
    std::string task;
    float fps{0.0f};
};

struct AiDetectorChannel {
    std::string channel;
    std::vector<AiDetectorTaskBinding> tasks;
};

// Default per-instance fps budget when no per-algCode override is configured.
inline constexpr float kDefaultInstanceFpsBudget = 36.0f;

// Hard cap used by fps-aware placement. Stress tests show 3 is a better global ceiling than 6.
inline constexpr size_t kMaxReuseHardLimit = 3;

// Safe compatibility cap when the orchestration did not expose fps to placement.
inline constexpr size_t kUnknownFpsReuseCount = 3;

namespace ai_detector_fps {

    inline bool HasConfiguredFps(float fps) {
        return fps > 0.0f && std::isfinite(fps);
    }

    // Keep unconfigured fps as 0. Placement handles it with the compatibility channel cap instead
    // of pretending it is 25fps, which over-splits low-fps deployments when initFps is not populated.
    inline float NormalizeRequestedFps(float fps) {
        return HasConfiguredFps(fps) ? fps : 0.0f;
    }

    inline size_t ClampReuseCount(size_t value, size_t hard_max_reuse_count) {
        const size_t capped_hard_max = std::max<size_t>(1, hard_max_reuse_count);
        return std::clamp(value, static_cast<size_t>(1), capped_hard_max);
    }

    inline size_t EffectiveMaxReuseCount(float requested_fps, float instance_fps_budget,
                                         size_t hard_max_reuse_count) {
        if (!HasConfiguredFps(requested_fps) || instance_fps_budget <= 0.0f ||
            !std::isfinite(instance_fps_budget)) {
            return ClampReuseCount(kUnknownFpsReuseCount, hard_max_reuse_count);
        }

        const auto by_fps = static_cast<size_t>(std::floor(instance_fps_budget / requested_fps));
        return ClampReuseCount(by_fps, hard_max_reuse_count);
    }

    // Max task fps within a single channel. Multiple tasks on one channel share one inference per
    // frame, so a channel's contribution is the max (not the sum) of its tasks' fps.
    inline float ChannelAssignedFps(const AiDetectorChannel& channel) {
        float max_fps = 0.0f;
        for (const auto& binding : channel.tasks) {
            if (binding.fps > max_fps) {
                max_fps = binding.fps;
            }
        }
        return max_fps;
    }

    // Total fps load on an instance = sum of each channel's max-task-fps.
    inline float AssignedFps(const std::vector<AiDetectorChannel>& channels) {
        float total = 0.0f;
        for (const auto& channel : channels) {
            total += ChannelAssignedFps(channel);
        }
        return total;
    }

    // Incremental fps delta if a task with requested_fps is added to channel_id:
    //   existing channel: delta = max(current, requested) - current
    //   new channel:      delta = requested (caller-normalized)
    inline float DeltaFpsForTask(const std::vector<AiDetectorChannel>& channels,
                                 const std::string& channel_id, float requested_fps) {
        for (const auto& channel : channels) {
            if (channel.channel == channel_id) {
                const float current = ChannelAssignedFps(channel);
                return std::max(current, requested_fps) - current;
            }
        }
        return requested_fps;
    }

    // Placement gate: a new task fits iff the channel hard cap and the fps budget both hold.
    inline bool CanAccept(const std::vector<AiDetectorChannel>& channels, const std::string& channel_id,
                          float requested_fps, size_t hard_max_reuse_count, float instance_fps_budget) {
        const float normalized = NormalizeRequestedFps(requested_fps);
        const float delta      = DeltaFpsForTask(channels, channel_id, normalized);
        const bool channel_exists =
            std::any_of(channels.begin(), channels.end(),
                        [&](const AiDetectorChannel& ch) { return ch.channel == channel_id; });
        const size_t channel_count_after_add = channel_exists ? channels.size() : channels.size() + 1;
        const size_t effective_max_reuse =
            EffectiveMaxReuseCount(requested_fps, instance_fps_budget, hard_max_reuse_count);

        if (channel_count_after_add > effective_max_reuse) {
            return false;
        }

        if (!HasConfiguredFps(requested_fps)) {
            return true;
        }

        return AssignedFps(channels) + delta <= instance_fps_budget;
    }

}  // namespace ai_detector_fps

}  // namespace cosmo
