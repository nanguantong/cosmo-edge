#include "nn/core/blob_impl.h"

#include "nn/core/abstract_device.h"
#include "nn/utils/blob_memory_size_info.h"

namespace cosmo::nn {

BlobImpl::BlobImpl(BlobDesc desc_) {
    desc         = desc_;
    alloc_memory = false;
}

BlobImpl::BlobImpl(BlobDesc desc_, bool alloc_memory_) {
    desc         = desc_;
    alloc_memory = alloc_memory_;

    if (alloc_memory) {
        auto device = GetDevice(desc.device_type);
        if (device != NULL) {
            BlobMemorySizeInfo size_info = device->Calculate(desc);
            device->Allocate(&handle.base, &handle.phy, size_info);
        }
    }
}

BlobImpl::BlobImpl(BlobDesc desc_, BlobHandle handle_) {
    desc         = desc_;
    handle       = handle_;
    alloc_memory = false;
}

BlobImpl::~BlobImpl() {
    if (alloc_memory && handle.base != nullptr) {
        auto device = GetDevice(desc.device_type);
        if (device) {
            device->Free(handle.base, handle.phy);
        }
    }
}

void BlobImpl::SetBlobDesc(BlobDesc desc_) {
    desc = desc_;
}

BlobDesc& BlobImpl::GetBlobDesc() {
    return desc;
}

BlobHandle BlobImpl::GetHandle() {
    return handle;
}

void BlobImpl::SetHandle(BlobHandle handle_) {
    if (alloc_memory) {
        auto device = GetDevice(desc.device_type);
        if (device) {
            device->Free(handle.base, handle.phy);
        }
    }
    handle       = handle_;
    alloc_memory = false;
}

void BlobImpl::ClearHandle() {
    // Forget the handle without freeing it. Used after BlobStore has already
    // freed the device memory, so a repeat FreeBlob is a no-op and ~BlobImpl
    // does not double-free. Unlike SetHandle, this never calls device->Free.
    handle       = BlobHandle{};
    alloc_memory = false;
}

}  // namespace cosmo::nn