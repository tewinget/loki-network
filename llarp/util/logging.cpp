#include "logging.hpp"

#include <oxen/log/catlogger.hpp>
#include <oxen/log/ring_buffer_sink.hpp>

namespace llarp
{

    log::CategoryLogger log_global = log::Cat("session-router");

    std::shared_ptr<log::RingBufferSink> logRingBuffer{};

}  // namespace llarp
