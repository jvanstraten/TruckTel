#pragma once

#include "config.h"
#include "worker.h"

// Avoid mdns.h from needing to be included transitively by including this.
// Apparently there is some funky incompatibility between it and fkYAML.h.
class MdnsServer;

/// Class managing a worker thread to run the telemetry server in.
class MdnsServerThread {

    /// Configuration for the mDNS service.
    MdnsConfiguration config;

    /// The mDNS worker thread.
    WorkerThread<MdnsServer> mdns;

public:
    /// Constructor. Doesn't start the thread yet.
    explicit MdnsServerThread(MdnsConfiguration config);

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
