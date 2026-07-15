// Receive error policy for multicast device discovery.
#pragma once

namespace cosmo::service::detail {

enum class MulticastReceiveAction {
    kRetry,
    kRestartSocket,
};

MulticastReceiveAction ClassifyMulticastReceiveError(int error_code);

}  // namespace cosmo::service::detail
