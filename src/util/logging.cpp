#include "logging.hpp"

#include <oxen/log/catlogger.hpp>
#include <oxen/log/ring_buffer_sink.hpp>

namespace srouter
{

    log::CategoryLogger log_global = log::Cat("session-router");

    std::shared_ptr<log::RingBufferSink> logRingBuffer{};

}  // namespace srouter
