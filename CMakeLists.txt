cmake_minimum_required(VERSION 3.24)
project(trucktel)

set(CMAKE_CXX_STANDARD 20)

# Download information for dependencies.
set(SCS_SDK_VERSION 1_14)
set(SCS_SDK_SHA256 c6c1f7376b7324994d9f9c567f3c4141fbbf305b6bf803bc4cfeef2437b2023a)
set(JSON_VERSION v3.12.0)
set(JSON_SHA256 42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)

# Declare external content.
include(FetchContent)
FetchContent_Declare(scs_sdk URL https://download.eurotrucksimulator2.com/scs_sdk_${SCS_SDK_VERSION}.zip URL_HASH SHA256=${SCS_SDK_SHA256})
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/${JSON_VERSION}/json.tar.xz URL_HASH SHA256=${JSON_SHA256})

# Download external content.
FetchContent_MakeAvailable(scs_sdk)
set(SCS_SDK_INCLUDE "${scs_sdk_SOURCE_DIR}/include")
FetchContent_MakeAvailable(json)

# Build the plugin library. This is what ETS2/ATS should load. Place the .so
# (or .dll, maybe? I didn't try Windows) in a directory named "plugins" created
# within the directory that contains the game binary.
add_library(trucktel SHARED trucktel.cpp)
target_include_directories(trucktel PRIVATE ${SCS_SDK_INCLUDE})
target_link_libraries(trucktel PRIVATE nlohmann_json::nlohmann_json)

# On Windows, we explicitly need to specify functions that we want to export in
# a DLL. A normal person would use __declspec(dllexport), but we can't easily
# change the function linkage within C++ because it's defined by the SCS header
# files.
if(MSVC)
target_link_options(trucktel PRIVATE /EXPORT:scs_telemetry_init /EXPORT:scs_telemetry_shutdown)
endif()
