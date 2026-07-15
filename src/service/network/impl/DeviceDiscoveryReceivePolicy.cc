// Receive error policy for multicast device discovery.

#include "service/network/impl/DeviceDiscoveryReceivePolicy.h"

#include <cerrno>

namespace cosmo::service::detail {

MulticastReceiveAction ClassifyMulticastReceiveError(int error_code) {
    if (error_code == EAGAIN || error_code == EINTR) {
        return MulticastReceiveAction::kRetry;
    }
#if EWOULDBLOCK != EAGAIN
    if (error_code == EWOULDBLOCK) {
        return MulticastReceiveAction::kRetry;
    }
#endif
    return MulticastReceiveAction::kRestartSocket;
}

}  // namespace cosmo::service::detail
