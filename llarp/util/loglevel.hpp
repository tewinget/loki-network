#ifndef LLARP_UTIL_LOG_LEVEL_HPP
#define LLARP_UTIL_LOG_LEVEL_HPP

namespace llarp
{
  // probably will need to move out of llarp namespace for c api
  enum LogLevel
  {
    eLogDebug,
    eLogInfo,
    eLogWarn,
    eLogError,
    eLogNone
  };

}  // namespace llarp

#endif
