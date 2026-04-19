#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "config.h"

// Opaque forward reference to the actual Server class. Prevents a million
// headers from being loaded transitively by using this header.
class Server;

/// Class managing a worker thread to run the telemetry server in.
class ServerThread {

    /// The configuration for this server.
    const Configuration configuration;

    /// The associated server instance.
    std::unique_ptr<Server> server;

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
    /// Constructor. Loads the configuration file for the given path, but
    /// doesn't start the thread yet.
    explicit ServerThread(const std::filesystem::path &app_path);

    /// Returns which port is configured for this server.
    [[nodiscard]] uint16_t port() const;

    /// Returns which inputs the app hosted by this server wants access to.
    [[nodiscard]] const InputChannelDescriptors &get_input_descriptors() const;

    /// Starts running the server.
    void start();

    /// Tells the server to fetch new data from the recorders.
    void update();

    /// Tells the server to shut down. Does not join yet.
    void stop();

    /// Waits for the server to stop.
    void join();

    /// Destructor. Tells the server to shut down (if not done so already) and
    /// joins with the server thread.
    ~ServerThread();
};
