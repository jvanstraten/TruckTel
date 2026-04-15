#pragma once

#include <memory>
#include <thread>

#include "config.h"

// Opaque forward reference to the actual Server class. Prevents a million
// headers from being loaded transitively by using this header.
class Server;

/// Class managing a worker thread to run the telemetry server in.
class ServerThread {

    /// The associated server instance.
    std::unique_ptr<Server> server;

    /// Main function for the thread.
    void main(ServerConfig config) const;

    /// Thread handle.
    std::thread thread;

    /// Constructor. Starts the server thread.
    explicit ServerThread(const ServerConfig &config);

public:
    /// Destructor. Shuts down and joins with the server thread.
    ~ServerThread();

private:
    /// Instance of the logger, used by static methods.
    static std::unique_ptr<ServerThread> instance;

public:
    /// Call from the SCS API telemetry initialization hook to start the
    /// telemetry server.
    static void init(const ServerConfig &config);

    /// Call from the SCS API telemetry shutdown hook to shut down the telemetry
    /// server.
    static void shutdown();
};
