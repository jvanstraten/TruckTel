cmake_minimum_required(VERSION 3.24)
project(trucktel)

set(CMAKE_CXX_STANDARD 20)

# Download information for dependencies.
set(SCS_SDK_VERSION 1_14)
set(SCS_SDK_SHA256 c6c1f7376b7324994d9f9c567f3c4141fbbf305b6bf803bc4cfeef2437b2023a)
set(JSON_VERSION v3.12.0)
set(JSON_SHA256 42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)
set(HTTPLIB_VERSION v0.42.0)

# Declare external content.
include(FetchContent)
FetchContent_Declare(scs_sdk URL https://download.eurotrucksimulator2.com/scs_sdk_${SCS_SDK_VERSION}.zip URL_HASH SHA256=${SCS_SDK_SHA256})
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/${JSON_VERSION}/json.tar.xz URL_HASH SHA256=${JSON_SHA256})
FetchContent_Declare(httplib GIT_REPOSITORY https://github.com/yhirose/cpp-httplib GIT_TAG ${HTTPLIB_VERSION} GIT_SHALLOW TRUE)

# HTTPLIB tries to link against stuff by default to support things like
# compression. Supporting that would be great, but if our plugin depends
# on external shared libraries it won't load properly in the game.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "")
set(HTTPLIB_USE_WOLFSSL_IF_AVAILABLE OFF CACHE BOOL "")
set(HTTPLIB_USE_MBEDTLS_IF_AVAILABLE OFF CACHE BOOL "")
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "")
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "")
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "")

# Download external content.
FetchContent_MakeAvailable(scs_sdk)
set(SCS_SDK_INCLUDE "${scs_sdk_SOURCE_DIR}/include")
FetchContent_MakeAvailable(json)
FetchContent_MakeAvailable(httplib)

# Build the plugin library. This is what ETS2/ATS should load. Place the .so
# (or .dll, maybe? I didn't try Windows) in a directory named "plugins" created
# within the directory that contains the game binary.
add_library(trucktel SHARED
        src/trucktel.cpp
        src/logger.cpp
        src/logger.h
        src/json_utils.h
        src/json_utils.cpp
        src/recorder/event.cpp
        src/recorder/event.h
        src/recorder/versioned.h
        src/recorder/versioned.cpp
        src/recorder/channel.h
        src/recorder/channel.cpp
        src/recorder/recorder.h
        src/recorder/recorder.cpp
        src/server.h
        src/server.cpp)
target_include_directories(trucktel PUBLIC ${SCS_SDK_INCLUDE} src)
target_link_libraries(trucktel PRIVATE nlohmann_json::nlohmann_json httplib)

# On Windows, we explicitly need to specify functions that we want to export in
# a DLL. A normal person would use __declspec(dllexport), but we can't easily
# change the function linkage within C++ because it's defined by the SCS header
# files.
if(MSVC)
target_link_options(trucktel PRIVATE /EXPORT:scs_telemetry_init /EXPORT:scs_telemetry_shutdown)
endif()

# Dummy executable for (very) basic testing.
add_executable(dummy src/dummy/main.cpp)
target_link_libraries(dummy PRIVATE trucktel)
