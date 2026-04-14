#pragma once

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "logger.h"

namespace wspp {

// Shorthands for commonly used namespaces in websocketpp.
using namespace websocketpp;
using namespace websocketpp::log;

/// Logger class that ties into our Logger instance. Much of this was copied
/// from the basic (ostream) logger built into WebsocketPP, just with the
/// stream removed and code style unified with the rest of TruckTel.
template <typename concurrency, typename names>
class LoggerAdapter {
private:
    /// Static loglevel, could be used to optimize out channels.
    level const m_static_channels;

    /// Dynamically-settable loglevel.
    level m_dynamic_channels;

    /// Loglevel used for the logged messages.
    scs_log_type_t verbosity = SCS_LOG_TYPE_message;

protected:
    // Mutex used for the loglevels.
    typedef typename concurrency::scoped_lock_type scoped_lock_type;
    typedef typename concurrency::mutex_type mutex_type;
    mutex_type m_lock;

public:
    /// Constructor without static channel mask.
    explicit LoggerAdapter(
        const channel_type_hint::value h = channel_type_hint::access
    )
        : m_static_channels(0xffffffff), m_dynamic_channels(0) {}

    /// Constructor with static channel mask.
    explicit LoggerAdapter(
        const level c,
        const channel_type_hint::value h = channel_type_hint::access
    )
        : m_static_channels(c), m_dynamic_channels(0) {}

    /// Copy constructor.
    LoggerAdapter(LoggerAdapter const &other)
        : m_static_channels(other.m_static_channels),
          m_dynamic_channels(other.m_dynamic_channels) {}

#ifdef _WEBSOCKETPP_DEFAULT_DELETE_FUNCTIONS_
    // no copy assignment operator because of const member variables
    LoggerAdapter &operator=(LoggerAdapter const &) = delete;
#endif // _WEBSOCKETPP_DEFAULT_DELETE_FUNCTIONS_

#ifdef _WEBSOCKETPP_MOVE_SEMANTICS_
    /// Move constructor.
    LoggerAdapter(LoggerAdapter &&other) noexcept
        : m_static_channels(other.m_static_channels),
          m_dynamic_channels(other.m_dynamic_channels) {}

#ifdef _WEBSOCKETPP_DEFAULT_DELETE_FUNCTIONS_
    // no move assignment operator because of const member variables
    LoggerAdapter &operator=(LoggerAdapter &&) = delete;
#endif // _WEBSOCKETPP_DEFAULT_DELETE_FUNCTIONS_

#endif // _WEBSOCKETPP_MOVE_SEMANTICS_

    void set_channels(level channels) {
        if (channels == names::none) {
            clear_channels(names::all);
            return;
        }

        scoped_lock_type lock(m_lock);
        m_dynamic_channels |= (channels & m_static_channels);
    }

    void clear_channels(level channels) {
        scoped_lock_type lock(m_lock);
        m_dynamic_channels &= ~channels;
    }

    /// Write a string message to the given channel.
    void write(level channel, std::string const &msg) {
        scoped_lock_type lock(m_lock);
        if (!this->dynamic_test(channel)) {
            return;
        }
        Logger::log(
            verbosity, "[%s] %s", names::channel_name(channel), msg.c_str()
        );
    }

    /// Write a cstring message to the given channel.
    void write(level channel, char const *msg) {
        scoped_lock_type lock(m_lock);
        if (!this->dynamic_test(channel)) {
            return;
        }
        Logger::log(verbosity, "[%s] %s", names::channel_name(channel), msg);
    }

    [[nodiscard]] _WEBSOCKETPP_CONSTEXPR_TOKEN_ bool static_test(
        const level channel
    ) const {
        return ((channel & m_static_channels) != 0);
    }

    bool dynamic_test(const level channel) {
        return ((channel & m_dynamic_channels) != 0);
    }

    void set_verbosity(const scs_log_type_t new_verbosity) {
        verbosity = new_verbosity;
    }
};

/// Override of the configuration class for WebsocketPP that uses loggers that
/// connect to our Logger instance.
struct Config : config::asio {
    typedef LoggerAdapter<concurrency_type, elevel> elog_type;
    typedef LoggerAdapter<concurrency_type, alevel> alog_type;

    struct transport_config : asio::transport_config {
        typedef Config::alog_type alog_type;
        typedef Config::elog_type elog_type;
    };

    typedef transport::asio::endpoint<transport_config> transport_type;
};

/// Typedef for a WebsocketPP server with our configuration.
using Server = server<Config>;

} // namespace wspp
