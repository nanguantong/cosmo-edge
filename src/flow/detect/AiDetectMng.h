#pragma once

#include "flow/action/ActionInstMngBase.h"
#include "flow/detect/AiDetector.h"

namespace cosmo {

// AiDetectMng enables fps-aware instance placement for the regular YOLO AiDetector by HIDING
// (not overriding) MultiChannelActionMng::GetInst, which is non-virtual. Callers MUST invoke
// GetInst through the concrete AiDetectMng type (TaskBase holds its AiDetectMng member by value).
// Calling via a MultiChannelActionMng<AiDetector>& reference would silently dispatch to the base
// template and bypass fps-aware placement. Dino/Qwen3VL/Sam2 keep using the base behavior.
class AiDetectMng : public MultiChannelActionMng<AiDetector> {
public:
    AiDetectMng() : MultiChannelActionMng("AiDetectMng") {}

    AiDetectorPtr GetInst(const std::string& algCode, const std::string& channelId, const std::string& task,
                          ActionNode& action);
};

}  // namespace cosmo
