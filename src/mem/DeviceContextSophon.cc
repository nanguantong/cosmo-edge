// DeviceContextSophon — Device Context Sophon implementation.

#include <stdexcept>

#include "bmlib_runtime.h"
#include "mem/DeviceContext.h"
#include "util/Log.h"

namespace cosmo::mem {

// TODO(seam-test): failure-path coverage requires injecting a seam around
// bm_dev_request/bm_dev_free (no bm_* mocks exist today); tracked as P2 tech debt.
DeviceContext::DeviceContext() {
    bm_handle_t memory_handle = {0};
    auto ret                  = bm_dev_request(&memory_handle, dev_id_);
    if (ret != BM_SUCCESS) {
        LOG_ERRO("bm_dev_request(memory) failed:{}", ret);
        throw std::runtime_error("bm_dev_request(memory) failed");
    }

    bm_handle_t media_handle = {0};
    ret                      = bm_dev_request(&media_handle, dev_id_);
    if (ret != BM_SUCCESS) {
        LOG_ERRO("bm_dev_request(media) failed:{}", ret);
        bm_dev_free(memory_handle);  // partial failure: release the acquired memory handle
        throw std::runtime_error("bm_dev_request(media) failed");
    }

    // Commit only after both requests succeed: on throw, handles_ stays empty so
    // GetMemoryHandle/GetMediaHandle return nullptr via their existing handles_.empty() guard.
    handles_.resize(2);
    handles_.at(0) = reinterpret_cast<void*>(memory_handle);
    handles_.at(1) = reinterpret_cast<void*>(media_handle);
}

DeviceContext::~DeviceContext() {
    for (auto& h : handles_) {
        bm_dev_free(reinterpret_cast<bm_handle_t>(h));
    }

    handles_.clear();
}

void* DeviceContext::GetMemoryHandle() {
    return handles_.empty() ? nullptr : handles_.at(0);
}

void* DeviceContext::GetMediaHandle() {
    return handles_.empty() ? nullptr : handles_.at(1);
}

}  // namespace cosmo::mem
