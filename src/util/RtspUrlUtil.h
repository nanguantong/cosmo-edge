#pragma once

#include <string>
#include <string_view>

namespace cosmo::util {

// Convert unescaped '@' characters inside RTSP userinfo to "%40" while
// preserving the final '@' separator between credentials and the host.
std::string NormalizeRtspUrl(std::string_view url);

// Return an RTSP URL safe for logs by replacing the password with "***".
std::string RedactRtspUrl(std::string_view url);

}  // namespace cosmo::util
