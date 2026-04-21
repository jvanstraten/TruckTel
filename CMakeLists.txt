cmake_minimum_required(VERSION 3.24)

# Determine our own version.
execute_process(
        COMMAND git describe --tags --long --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE TRUCKTEL_VERSION_FULL
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(TRUCKTEL_VERSION_FULL MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+).*")
    set(TRUCKTEL_VERSION ${CMAKE_MATCH_1})
else()
    message(FATAL_ERROR "Failed to get TruckTel version from git")
endif()
project(TruckTel VERSION ${TRUCKTEL_VERSION})

# Use C++17. Any higher and WebsocketPP code will give issues.
set(CMAKE_CXX_STANDARD 17)

# Download information for dependencies.
set(SCS_SDK_VERSION "1_14")
set(SCS_SDK_SHA256 c6c1f7376b7324994d9f9c567f3c4141fbbf305b6bf803bc4cfeef2437b2023a)
set(JSON_VERSION "v3.12.0")
set(JSON_SHA256 42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)
set(FKYAML_VERSION "v0.4.2")
set(FKYAML_SHA256 1ffdf64da3d560bb86a3c477c6d11718e9a724e8a8c5d5326bd09a3a7689cdcf)
set(ASIO_VERSION "asio-1-32-0")
set(WEBSOCKETPP_VERSION "0.8.2")

# Declare external content.
include(FetchContent)
FetchContent_Declare(scs_sdk URL "https://download.eurotrucksimulator2.com/scs_sdk_${SCS_SDK_VERSION}.zip" URL_HASH SHA256=${SCS_SDK_SHA256})
FetchContent_Declare(json URL "https://github.com/nlohmann/json/releases/download/${JSON_VERSION}/json.tar.xz" URL_HASH SHA256=${JSON_SHA256})
FetchContent_Declare(fkyaml URL "https://github.com/fktn-k/fkYAML/releases/download/${FKYAML_VERSION}/fkYAML_single_header.zip" URL_HASH SHA256=${FKYAML_SHA256})
FetchContent_Declare(asio GIT_REPOSITORY "https://github.com/chriskohlhoff/asio.git" GIT_TAG "${ASIO_VERSION}")

# Download external content.
FetchContent_MakeAvailable(scs_sdk)
set(SCS_SDK_INCLUDE "${scs_sdk_SOURCE_DIR}/include")
FetchContent_MakeAvailable(json)
FetchContent_MakeAvailable(fkyaml)
FetchContent_MakeAvailable(asio)

# websocketpp has a CMakeLists.txt file that's janky, outdated, and not
# something we need for anything. But FetchContent_MakeAvailable would
# add it anyway, and there doesn't seem to be a good way to override
# that. So we also need some jank...
FetchContent_GetProperties(websocketpp)
if(NOT websocketpp_POPULATED)
    FetchContent_Populate(websocketpp QUIET GIT_REPOSITORY "https://github.com/zaphoyd/websocketpp.git" GIT_TAG "${WEBSOCKETPP_VERSION}")
endif()

# Generate version header file.
configure_file(
        src/version.h.in
        version.h
        @ONLY
)

# Build the plugin library. This is what ETS2/ATS should load. Place the .so
# (or .dll, maybe? I didn't try Windows) in a directory named "plugins" created
# within the directory that contains the game binary.
add_library(trucktel SHARED
        src/trucktel.cpp
        src/license.h
        src/api.h
        src/logger.cpp
        src/logger.h
        src/json_utils.h
        src/json_utils.cpp
        src/input.h
        src/input.cpp
        src/recorder/event.cpp
        src/recorder/event.h
        src/recorder/configuration.h
        src/recorder/configuration.cpp
        src/recorder/channel.h
        src/recorder/channel.cpp
        src/recorder/recorder.h
        src/recorder/recorder.cpp
        src/server/server_thread.h
        src/server/server_thread.cpp
        src/server/config.h
        src/server/config.cpp
        src/server/wspp_config.h
        src/server/server.h
        src/server/server.cpp
        src/server/url.h
        src/server/url.cpp
        src/server/http.h
        src/server/http.cpp
        src/server/database.h
        src/server/database.cpp
        src/server/websocket.h
        src/server/websocket.cpp
        src/mdns/server_thread.h
        src/mdns/server_thread.cpp
        src/mdns/config.h
        src/mdns/config.cpp
        src/mdns/server.h
        src/mdns/server.cpp
        src/mdns/string_table.h
        src/mdns/string_table.cpp
        src/mdns/mdns.h
        ${asio_SOURCE_DIR}/asio/src/asio.cpp
)
target_link_libraries(trucktel
        PRIVATE nlohmann_json::nlohmann_json
)
target_include_directories(trucktel
        PRIVATE "src"
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
        PUBLIC "${SCS_SDK_INCLUDE}"
        PRIVATE "${fkyaml_SOURCE_DIR}/include"
        PRIVATE "${asio_SOURCE_DIR}/asio/include"
        PRIVATE "${websocketpp_SOURCE_DIR}"
)
target_compile_definitions(trucktel
        PRIVATE ASIO_SEPARATE_COMPILATION ASIO_STANDALONE
        PRIVATE _WEBSOCKETPP_CPP11_STL_
)

# On Windows, we explicitly need to specify functions that we want to export in
# a DLL. A normal person would use __declspec(dllexport), but we can't easily
# change the function linkage within C++ because it's defined by the SCS header
# files.
if(MSVC)
    target_link_options(trucktel PRIVATE
            /EXPORT:scs_telemetry_init
            /EXPORT:scs_telemetry_shutdown
            /EXPORT:scs_input_init
            /EXPORT:scs_input_shutdown
    )

    # Required by mDNS.
    target_link_libraries(trucktel PUBLIC iphlpapi ws2_32)
endif()

# Dummy executable for (very) basic testing.
add_executable(dummy src/dummy/main.cpp)
target_link_libraries(dummy PRIVATE trucktel)
