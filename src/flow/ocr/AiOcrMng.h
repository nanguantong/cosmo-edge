// AiOcrMng — Instance manager for landmark-driven OCR actions.

#pragma once

#include "flow/action/ActionInstMngBase.h"
#include "flow/ocr/AiOcr.h"

namespace cosmo {

class AiOcrMng : public SimpleMapActionMng<AiOcr> {
public:
    AiOcrMng() : SimpleMapActionMng("AiOcrMng") {}
};

}  // namespace cosmo
