// FaceLogicMng — face logic management.
#pragma once

#include <map>
#include <memory>
#include <string>

#include "flow/action/ActionInstMngBase.h"
#include "flow/logical/FaceLogic.h"

namespace cosmo {
class FaceLogicMng : public IMngStatusProvider {
public:
    FaceLogicMng();
    ~FaceLogicMng();

    FaceLogicPtr GetInst(const std::string &taskId, ActionNode &actionFaceLogic);
    bool DeleteInst(FaceLogicPtr inst);

    void QueueStatus(std::vector<AlgActionDataQueueStatus> &queStatus,
                     unsigned int durationSec = 30) override;
    void ActionInfo(std::vector<ActionRuntimeInfo> &actionInfo) override;

private:
    FaceLogicPtr GetInst(const std::string &taskId);

    std::shared_mutex mtx_;
    std::vector<FaceLogicPtr> insts_;
};
}  // namespace cosmo
