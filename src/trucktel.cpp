#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <vector>

#include "scssdk_telemetry.h"

#include "nlohmann/json.hpp"

#include "logger.h"
#include "recorder/recorder.h"
#include "server.h"

/// SCS API initialization callback. This is the entry point that ETS2/ATS
/// calls.
SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version, const scs_telemetry_init_params_t *const params
) {
    if (version != SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
    if (!params) return SCS_RESULT_invalid_parameter;
    const auto *init_params =
        reinterpret_cast<const scs_telemetry_init_params_v101_t *>(params);
    try {
        Logger::init(init_params->common.log);
        Recorder::init(version, init_params);
        Server::init();
        Logger::info("Init complete");
        return SCS_RESULT_ok;
    } catch (std::exception &e) {
        Logger::error("Init error: %s", e.what());
        return SCS_RESULT_generic_error;
    }
}

/// SCS API cleanup handler.
SCSAPI_VOID scs_telemetry_shutdown() {
    Logger::info("Waiting for server to shut down");
    Server::shutdown();
    Logger::info("Shutting down");
    Recorder::shutdown();
    Logger::shutdown();
}
