#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <httplib.h>

/// Class managing a worker thread to run the telemetry server in.
class Server {

    /// Main function for the thread.
    void main();

    /// HTTPLIB server instance.
    httplib::Server server;

    /// Shutdown request flag, used to shut down open websockets.
    std::atomic<bool> shutdown_request = false;

    /// Thread handle.
    std::thread thread;

    /// Constructor. Starts the server thread.
    Server();

public:
    /// Destructor. Shuts down and joins with the server thread.
    ~Server();

private:
    /// Instance of the logger, used by static methods.
    static std::unique_ptr<Server> instance;

public:
    /// Call from the SCS API telemetry initialization hook to start the
    /// telemetry server.
    static void init();

    /// Call from the SCS API telemetry shutdown hook to shut down the telemetry
    /// server.
    static void shutdown();
};
