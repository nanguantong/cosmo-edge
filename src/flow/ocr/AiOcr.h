// AiOcr — Landmark-driven license plate OCR action.

#pragma once

#include <memory>
#include <string>

#include "flow/action/AlgActionBase.h"
#include "infer/AiOcrWordClassifierUnify.h"

namespace cosmo {

class AiOcr : public AlgActionBase {
public:
    AiOcr(const std::string& a_task_id, ActionNode& action);
    ~AiOcr() override;

    AiOcr(const AiOcr&)            = delete;
    AiOcr& operator=(const AiOcr&) = delete;

    [[nodiscard]] bool AiSdkInit();

protected:
    void HandFrame(AlgDataPtr alg_data) override;

private:
    [[nodiscard]] bool HandleTarget(const VideoFramePtr& frame, AiDetectRstEl& target,
                                    DataDetTrackClassify& input, DataAlarm& alarms);

    std::string alg_code_;
    AiOcrWordClassifierUnifyPtr classifier_;
};

using AiOcrPtr = std::shared_ptr<AiOcr>;

}  // namespace cosmo
