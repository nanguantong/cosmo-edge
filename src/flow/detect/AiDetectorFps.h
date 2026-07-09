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

// Fallback fps used when a task does not specify one (initFps <= 0 means "unconfigured / full-frame").
inline constexpr float kUnknownFpsEstimate = 25.0f;

// Default per-instance fps budget when no per-algCode override is configured.
inline constexpr float kDefaultInstanceFpsBudget = 36.0f;

namespace ai_detector_fps {

    // Map an unconfigured (<=0) or non-finite (NaN/inf) fps to a conservative estimate; otherwise
    // pass through. Finite out-of-range values are intentionally NOT clamped: an absurdly large fps
    // simply fails CanAccept and forces a new instance, which is the safe (over-provisioning) direction.
    inline float NormalizeRequestedFps(float fps) {
        return (fps > 0.0f && std::isfinite(fps)) ? fps : kUnknownFpsEstimate;
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
                          float requested_fps, size_t max_reuse_count, float instance_fps_budget) {
        const float normalized = NormalizeRequestedFps(requested_fps);
        const float delta      = DeltaFpsForTask(channels, channel_id, normalized);
        const bool channel_exists =
            std::any_of(channels.begin(), channels.end(),
                        [&](const AiDetectorChannel& ch) { return ch.channel == channel_id; });
        const size_t channel_count_after_add = channel_exists ? channels.size() : channels.size() + 1;
        return channel_count_after_add <= max_reuse_count &&
               AssignedFps(channels) + delta <= instance_fps_budget;
    }

}  // namespace ai_detector_fps

}  // namespace cosmo
