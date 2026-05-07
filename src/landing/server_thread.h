#pragma once

#include "info.h"
#include "worker.h"

// Opaque forward reference to the actual Server class. Prevents a million
// headers from being loaded transitively by using this header.
class LandingServer;

/// Class managing a worker thread to run the landing page server in.
class LandingServerThread {

    /// Managed thread that the server runs in.
    WorkerThread<LandingServer> thread;

    /// Port on which the landing server listens.
    const uint16_t port;

    /// Information structure served to the landing page.
    const LandingInfo &info;

public:
    /// Constructor.
    explicit LandingServerThread(uint16_t port, const LandingInfo &info);

    /// Starts running the server.
    void start();

    /// Tells the server to shut down. Does not join yet.
    void stop();

    /// Waits for the server to stop.
    void join();

    /// Destructor. Tells the server to shut down (if not done so already) and
    /// joins with the server thread.
    ~LandingServerThread();
};
