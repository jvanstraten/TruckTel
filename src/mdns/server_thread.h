#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>

#include "config.h"

/// Avoid mdns.h from needing to be included transitively by including this.
/// Apparently there is some funky incompatibility between it and fkYAML.h.
class MdnsServer;

/// Class managing a worker thread to run the telemetry server in.
class MdnsServerThread {

    /// Configuration for the mDNS service.
    MdnsConfiguration configuration;

    /// The associated server instance.
    std::unique_ptr<MdnsServer> server;

    /// Main function for the thread.
    void main();

    /// Thread handle.
    std::thread thread;

    /// Condition variable used to notify that the server has been initialized.
    std::condition_variable state_cv;

    /// Mutex for init_cv.
    std::mutex state_mutex;

    /// Whether initialization was successful. Guarded by state_mutex and
    /// state_cv.
    std::atomic<bool> init_success = false;

    /// Whether the server thread crashed. Guarded by state_mutex and state_cv.
    std::atomic<bool> server_crashed = false;

public:
    /// Constructor. Doesn't start the thread yet.
    explicit MdnsServerThread(const MdnsConfiguration &configuration);

    /// Starts running the server.
    void start();

    /// Tells the server to shut down. Does not join yet.
    void stop();

    /// Waits for the server to stop.
    void join();

    /// Destructor. Tells the server to shut down (if not done so already) and
    /// joins with the server thread.
    ~MdnsServerThread();
};
