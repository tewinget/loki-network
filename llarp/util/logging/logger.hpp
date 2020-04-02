#ifndef LLARP_UTIL_LOGGER_HPP
#define LLARP_UTIL_LOGGER_HPP

#include <util/time.hpp>
#include <util/logging/logstream.hpp>
#include <util/logging/logger_internal.hpp>

namespace llarp
{

  struct LogContext
  {
    LogContext();
    LogLevel curLevel = eLogInfo;
    LogLevel startupLevel = eLogInfo;
    LogLevel runtimeLevel = eLogInfo;
    ILogStream_ptr logStream;
    std::string nodeName = "lokinet";

    const llarp_time_t started;

    static LogContext&
    Instance();

    void
    DropToRuntimeLevel();

    void
    RevertRuntimeLevel();
  };

  void
  SetLogLevel(LogLevel lvl);

  /** internal */
  template <typename... TArgs>
  inline static void
#ifndef LOKINET_HIVE
  _Log(LogLevel lvl, const char* fname, int lineno, TArgs&&... args) noexcept
#else
  _Log(LogLevel, const char*, int, TArgs&&...) noexcept
#endif
  {
/* nop out logging for hive mode for now */
#ifndef LOKINET_HIVE
    auto& log = LogContext::Instance();
    if (log.curLevel > lvl)
      return;
    std::stringstream ss;
    LogAppend(ss, std::forward<TArgs>(args)...);
    log.logStream->AppendLog(lvl, fname, lineno, log.nodeName, ss.str());
#endif
  }
}  // namespace llarp

#define LogTrace(...) _Log(llarp::eLogTrace, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogDebug(...) _Log(llarp::eLogDebug, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogInfo(...) _Log(llarp::eLogInfo, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogWarn(...) _Log(llarp::eLogWarn, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogError(...) _Log(llarp::eLogError, LOG_TAG, __LINE__, __VA_ARGS__)

#define LogTraceTag(tag, ...) _Log(llarp::eLogTrace, tag, __LINE__, __VA_ARGS__)
#define LogDebugTag(tag, ...) _Log(llarp::eLogDebug, tag, __LINE__, __VA_ARGS__)
#define LogInfoTag(tag, ...) _Log(llarp::eLogInfo, tag, __LINE__, __VA_ARGS__)
#define LogWarnTag(tag, ...) _Log(llarp::eLogWarn, tag, __LINE__, __VA_ARGS__)
#define LogErrorTag(tag, ...) _Log(llarp::eLogError, tag, __LINE__, __VA_ARGS__)

#define LogTraceExplicit(tag, line, ...) _Log(llarp::eLogTrace, tag, line, __VA_ARGS__)
#define LogDebugExplicit(tag, line, ...) _Log(llarp::eLogDebug, tag, line, __VA_ARGS__)
#define LogInfoExplicit(tag, line, ...) _Log(llarp::eLogInfo, tag, line __VA_ARGS__)
#define LogWarnExplicit(tag, line, ...) _Log(llarp::eLogWarn, tag, line, __VA_ARGS__)
#define LogErrorExplicit(tag, line, ...) _Log(llarp::eLogError, tag, line, __VA_ARGS__)

#ifndef LOG_TAG
#define LOG_TAG "default"
#endif

#endif
