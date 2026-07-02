// application — application implementation.

#include "app/application.h"

#include <csignal>
#include <cstdio>
#include <filesystem>
#include <memory>

#include "app/AppConstants.h"
#include "app/app_init.h"
#include "mem/DeviceContext.h"
#include "mem/IDeviceContext.h"
#include "service/detail/ServiceRegistry.h"
#include "util/Log.h"
#include "util/PathUtil.h"
#include "util/Version.h"

namespace cosmo::app {

Application::Application(std::string name) : app_name_(std::move(name)) {}

void LogInit(const std::string& name, const std::string& basedir, const std::string& version) {
    using namespace constants;

    // Log initialization
    auto log_path = std::filesystem::path(basedir) / "logs";
    cosmo::log::LogInit(name.c_str(), log_path.c_str(), version.c_str());
    cosmo::log::SetLogCleanLogMode(0);
    cosmo::log::SetMaxLogFileCount(kMaxLogFileCount);
    cosmo::log::SetMaxLogFileKeepDays(kMaxLogFileKeepDays);
#ifdef COSMO_DEV_MODE
    // Development mode: mirror all log messages to stdout for real-time debugging.
    // In production this is disabled to prevent log output from being captured by
    // systemd/journald and flooding /var/log/syslog (especially under high-concurrency
    // benchmark loads like scenario-bench). Application logs are still written to
    // /data/cwaiuserdata/log/ via glog regardless of this setting.
    cosmo::log::SetLogCallback(
        [](const char*, const char*, int, const char* data, size_t) { printf("%s\r\n", data); });
#endif
    cosmo::log::SetMaxLogFileSize(kMaxLogFileSizeMb);
    cosmo::log::SetLogLevel(LOG_LEVEL_INFO);
    cosmo::log::SetLogFileFlushInterval(kLogFlushIntervalSec);
    cosmo::log::SetPrintLevel(LOG_LEVEL_INFO);
}

void Application::run(const char* base_dir) {
    // DeviceContext must outlive all services — created before SwDeviceInit,
    // destroyed after SwDeviceDestroy. Declared here (not constructed) so the
    // teardown after the catch still sees it; construction moved inside try so a
    // constructor exception is caught and logged cleanly instead of escaping to
    // std::terminate.
    std::unique_ptr<cosmo::mem::DeviceContext> device_ctx;

    try {
        signal(SIGILL, SIG_IGN);
        signal(SIGPIPE, SIG_IGN);

        SwDevicePreInit();

        // Log initialization
        LogInit(app_name_, cosmo::path::GetLogPath(), cosmo::util::GetAbbrVersion());

        LOG_INFO("{} Version:{}", cosmo::util::GetProgramDesc(), cosmo::util::GetAbbrVersion());

        // Construct after LogInit so a bm_dev_request failure is logged to a configured glog.
        device_ctx = std::make_unique<cosmo::mem::DeviceContext>();

        // Register DeviceContext via non-owning Set (it outlives ServiceRegistry)
        cosmo::service::ServiceRegistry::Instance().Set<cosmo::mem::IDeviceContext>(device_ctx.get());
        SwDeviceInit();

        // Blocking — runs the HTTP server event loop until shutdown.
        SwDeviceRun();

    } catch (const std::exception& ex) {
        LOG_ERRO("ERROR Exit! [{}]", ex.what());
    }

    SwDeviceDestroy();

    // Unregister before destruction
    cosmo::service::ServiceRegistry::Instance().Set<cosmo::mem::IDeviceContext>(nullptr);
    device_ctx.reset();

    LOG_INFO("{} Version:{} Quit", cosmo::util::GetProgramDesc(), cosmo::util::GetAbbrVersion());

    cosmo::log::LogShutDown();
}

}  // namespace cosmo::app