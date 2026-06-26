#pragma once

#include <trompeloeil.hpp>

#include "service/onboarding/IOnboardingService.h"

namespace cosmo::test {

class MockOnboardingService : public service::IOnboardingService {
public:
    MAKE_MOCK0(IsOnboardingCompleted, bool(), override);
    MAKE_MOCK0(CompleteOnboarding, void(), override);
    MAKE_MOCK0(Reset, void(), override);
};

}  // namespace cosmo::test
