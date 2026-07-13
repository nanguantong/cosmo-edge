#include "util/RtspUrlUtil.h"

namespace cosmo::util {

namespace {
    constexpr std::string_view kRtspPrefix = "rtsp://";

    bool GetAuthorityRange(std::string_view url, size_t& begin, size_t& end) {
        if (url.rfind(kRtspPrefix, 0) != 0) {
            return false;
        }

        begin = kRtspPrefix.size();
        end   = url.find_first_of("/?#", begin);
        if (end == std::string_view::npos) {
            end = url.size();
        }
        return end > begin;
    }
}  // namespace

std::string NormalizeRtspUrl(std::string_view url) {
    size_t authority_begin = 0;
    size_t authority_end   = 0;
    if (!GetAuthorityRange(url, authority_begin, authority_end)) {
        return std::string(url);
    }

    const size_t auth_separator = url.rfind('@', authority_end - 1);
    if (auth_separator == std::string_view::npos || auth_separator < authority_begin) {
        return std::string(url);
    }

    std::string normalized;
    normalized.reserve(url.size());
    for (size_t i = 0; i < url.size(); ++i) {
        if (i >= authority_begin && i < auth_separator && url[i] == '@') {
            normalized.append("%40");
        } else {
            normalized.push_back(url[i]);
        }
    }
    return normalized;
}

std::string RedactRtspUrl(std::string_view url) {
    std::string redacted = NormalizeRtspUrl(url);

    size_t authority_begin = 0;
    size_t authority_end   = 0;
    if (!GetAuthorityRange(redacted, authority_begin, authority_end)) {
        return redacted;
    }

    const size_t auth_separator = redacted.rfind('@', authority_end - 1);
    if (auth_separator == std::string::npos || auth_separator < authority_begin) {
        return redacted;
    }

    const size_t password_separator = redacted.find(':', authority_begin);
    if (password_separator == std::string::npos || password_separator >= auth_separator) {
        return redacted;
    }

    redacted.replace(password_separator + 1, auth_separator - password_separator - 1, "***");
    return redacted;
}

}  // namespace cosmo::util
