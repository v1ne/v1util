#pragma once

#include "v1util/base/macromagic.hpp"
#include "v1util/base/platform.hpp"
#include "v1util/stl-plus/filesystem-fwd.hpp"

#include <stddef.h>
#include <cstdint>


namespace v1util::tracing {
V1_PUBLIC void init(size_t bufferCapacityB);
V1_PUBLIC void destroy();
V1_PUBLIC void setStarted(bool started);
V1_PUBLIC bool started();
V1_PUBLIC std::filesystem::path finishAndWriteToPathPrefix(const std::filesystem::path& pathPrefix);

struct Status {
  bool initialized : 1;
  bool started : 1;
  size_t capacity;
  size_t used;
};
V1_PUBLIC Status status();

//! Create a trace entry for when this object created and destroyed, defining a scope.
#define V1_TRACING_SCOPE(category, name)                                                 \
  const auto V1_PP_UNQIUE_NAME(v1TracingScope) = v1util::tracing::detail::TracingScope { \
    V1_PP_STR(category), V1_PP_STR(name)                                                 \
  }
#define V1_TRACING_SCOPE1(category, name, var0)                                          \
  const auto V1_PP_UNQIUE_NAME(v1TracingScope) = v1util::tracing::detail::TracingScope { \
    V1_PP_STR(category), V1_PP_STR(name),                                                \
        v1util::tracing::detail::toTraceArg(V1_PP_STR(var0), var0)                       \
  }
#define V1_TRACING_SCOPE2(category, name, var0, var1)                                    \
  const auto V1_PP_UNQIUE_NAME(v1TracingScope) = v1util::tracing::detail::TracingScope { \
    V1_PP_STR(category), V1_PP_STR(name),                                                \
        v1util::tracing::detail::toTraceArg(V1_PP_STR(var0), var0),                      \
        v1util::tracing::detail::toTraceArg(V1_PP_STR(var1), var1)                       \
  }
#define V1_TRACING_SCOPE3(category, name, var0, var1, var2)                              \
  const auto V1_PP_UNQIUE_NAME(v1TracingScope) = v1util::tracing::detail::TracingScope { \
    V1_PP_STR(category), V1_PP_STR(name),                                                \
        v1util::tracing::detail::toTraceArg(V1_PP_STR(var0), var0),                      \
        v1util::tracing::detail::toTraceArg(V1_PP_STR(var1), var1),                      \
        v1util::tracing::detail::toTraceArg(V1_PP_STR(var2), var2)                       \
  }
//! Create a tracing scope around the statement, returning the value of the statement
#define V1_TRACING_STMT(category, name, Statement)                                   \
  [&]() {                                                                            \
    const auto V1_PP_UNQIUE_NAME(v1TracingScope) =                                   \
        v1util::tracing::detail::TracingScope{V1_PP_STR(category), V1_PP_STR(name)}; \
    return Statement;                                                                \
  }()

//! Trace the value of a variable
#define V1_TRACING_VARRIABLE1(category, name, var0)                             \
  v1util::tracing::detail::track_variable(V1_PP_STR(category), V1_PP_STR(name), \
      v1util::tracing::detail::toTraceArg(V1_PP_STR(var0), var0))
#define V1_TRACING_VARRIABLE2(category, name, var0, var1)                       \
  v1util::tracing::detail::track_variable(V1_PP_STR(category), V1_PP_STR(name), \
      v1util::tracing::detail::toTraceArg(V1_PP_STR(var0), var0),               \
      v1util::tracing::detail::toTraceArg(V1_PP_STR(var1), var1))
#define V1_TRACING_VARRIABLE3(category, name, var0, var1, var2)                 \
  v1util::tracing::detail::track_variable(V1_PP_STR(category), V1_PP_STR(name), \
      v1util::tracing::detail::toTraceArg(V1_PP_STR(var0), var0),               \
      v1util::tracing::detail::toTraceArg(V1_PP_STR(var1), var1),               \
      v1util::tracing::detail::toTraceArg(V1_PP_STR(var2), var2))

//! Trace an asynchronous event
#define V1_TRACING_ASYNC_BEGIN(category, name, id) \
  v1util::tracing::detail::begin_async_event(V1_PP_STR(category), V1_PP_STR(name), id)
#define V1_TRACING_ASYNC_BEGIN1(category, name, id, var0)                              \
  v1util::tracing::detail::begin_async_event(V1_PP_STR(category), V1_PP_STR(name), id, \
      v1util::tracing::detail::toTraceArg(V1_PP_STR(var0), var0))
#define V1_TRACING_ASYNC_BEGIN2(category, name, id, var0, var1)                        \
  v1util::tracing::detail::begin_async_event(V1_PP_STR(category), V1_PP_STR(name), id, \
      v1util::tracing::detail::toTraceArg(V1_PP_STR(var0), var0),                      \
      v1util::tracing::detail::toTraceArg(V1_PP_STR(var1), var1))
#define V1_TRACING_ASYNC_END(category, name, id) \
  v1util::tracing::detail::end_async_event(V1_PP_STR(category), V1_PP_STR(name), id)


namespace detail {

struct TraceArg {
  const char* pKey;
  union {
    const char* pDynamicString;
    int64_t intNumber;
    double fpNumber;
  };
  enum { kStaticString, kInt64, kDouble } Type;
};

inline TraceArg toTraceArg(const char* pKey, const char* pDynamicString) {
  TraceArg Arg;
  Arg.pKey = pKey;
  Arg.pDynamicString = pDynamicString;
  Arg.Type = TraceArg::kStaticString;
  return Arg;
}

inline TraceArg toTraceArg(const char* pKey, int intNumber) {
  TraceArg Arg;
  Arg.pKey = pKey;
  Arg.intNumber = int64_t(intNumber);
  Arg.Type = TraceArg::kInt64;
  return Arg;
}

inline TraceArg toTraceArg(const char* pKey, unsigned int intNumber) {
  TraceArg Arg;
  Arg.pKey = pKey;
  Arg.intNumber = int64_t(intNumber);
  Arg.Type = TraceArg::kInt64;
  return Arg;
}

inline TraceArg toTraceArg(const char* pKey, int64_t intNumber) {
  TraceArg Arg;
  Arg.pKey = pKey;
  Arg.intNumber = intNumber;
  Arg.Type = TraceArg::kInt64;
  return Arg;
}

inline TraceArg toTraceArg(const char* pKey, uint64_t intNumber) {
  TraceArg Arg;
  Arg.pKey = pKey;
  Arg.intNumber = int64_t(intNumber);
  Arg.Type = TraceArg::kInt64;
  return Arg;
}

inline TraceArg toTraceArg(const char* pKey, float fpNumber) {
  TraceArg Arg;
  Arg.pKey = pKey;
  Arg.fpNumber = double(fpNumber);
  Arg.Type = TraceArg::kDouble;
  return Arg;
}

inline TraceArg toTraceArg(const char* pKey, double fpNumber) {
  TraceArg Arg;
  Arg.pKey = pKey;
  Arg.fpNumber = fpNumber;
  Arg.Type = TraceArg::kDouble;
  return Arg;
}

class V1_PUBLIC TracingScope {
 public:
  TracingScope(const char* pCategory, const char* pName);
  TracingScope(const char* pCategory, const char* pName, const TraceArg& arg0);
  TracingScope(
      const char* pCategory, const char* pName, const TraceArg& arg0, const TraceArg& arg1);
  TracingScope(const char* pCategory, const char* pName, const TraceArg& arg0, const TraceArg& arg1,
      const TraceArg& arg2);
  ~TracingScope();

 private:
  const char* mpCategory;
  const char* mpName;
};

V1_PUBLIC void track_variable(const char* pCategory, const char* pName, const TraceArg& arg0);
V1_PUBLIC void track_variable(
    const char* pCategory, const char* pName, const TraceArg& arg0, const TraceArg& arg1);
V1_PUBLIC void track_variable(const char* pCategory, const char* pName, const TraceArg& arg0,
    const TraceArg& arg1, const TraceArg& arg2);


V1_PUBLIC void begin_async_event(const char* pCategory, const char* pName, int64_t id);
V1_PUBLIC void begin_async_event(
    const char* pCategory, const char* pName, int64_t id, const TraceArg& arg0);
V1_PUBLIC void begin_async_event(const char* pCategory, const char* pName, int64_t id,
    const TraceArg& arg0, const TraceArg& arg1);
V1_PUBLIC void end_async_event(const char* pCategory, const char* pName, int64_t id);

}  // namespace detail
}  // namespace v1util::tracing
