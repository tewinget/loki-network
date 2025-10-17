#pragma once

#include <future>
#include <memory>
#include <mutex>

namespace oxen::quic
{
    class Loop;
}

namespace llarp
{
    namespace vpn
    {
        class Platform;
    }

    struct Config;
    class Router;

    // Helper "context" that aids in starting up Session Router.
    //
    // Note that this class is *not* thread-safe: only one thread should attempt to hold and
    // interact with it to manage Session Router.
    //
    // TODO FIXME this class seems unnecessary, we should get rid of it.
    struct Context
    {
        std::unique_ptr<Router> router;

        explicit Context(bool embedded);
        ~Context();

        // Starts Session Router; returns as soon as Session Router is up and running (or throws if startup
        // fails).  The loop may be provided in order to use an existing loop, but otherwise a new
        // one will be started.
        void start(Config conf, std::shared_ptr<oxen::quic::Loop> loop = nullptr);

        // Waits for Session Router to finish.  Note that this does not *trigger* such a shutdown; for that
        // you would call `stop()` before this.
        void wait();

        // Call this to deliver a signal, such as SIGTERM or SIGINT to stop Session Router if currently
        // running.  (This can be called from any thread).
        void signal(int sig);

        // Initiates Session Router shutdown, and returns immediately (without waiting for shutdown).  Call
        // `wait()` after this if you also want to wait for shutdown to complete.
        void stop();

        bool is_up() const;

        bool is_stopping() const;

        // Returns true if Session Router has stopped and `wait()` needs to be called to finish
        // destruction.
        bool is_waiting() const;

        bool looks_alive() const;

        int androidFD = -1;

      private:
        bool embedded;
        std::future<void> lifetime_waiter;
    };
}  // namespace llarp
