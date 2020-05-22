#include "tracing.hpp"

#include "v1util/base/warnings.hpp"
#include "v1util/stl-plus/filesystem.hpp"

#include <stdio.h>
#include <time.h>
#include <fstream>
#include <iostream>

extern "C" {
V1_NO_WARNINGS
#include "v1util/third-party/uu.spdr/include/spdr/spdr.h"
V1_RESTORE_WARNINGS
}

namespace v1util::tracing {

SPDR_Context* gSpdrContext = nullptr;
uint8_t* gSpdrBuffer = nullptr;
bool gWasStarted = false;

void init(size_t bufferCapacityB) {
  gSpdrBuffer = new uint8_t[bufferCapacityB]();
  spdr_init(&gSpdrContext, gSpdrBuffer, bufferCapacityB);
}

void destroy() {
  if(gSpdrBuffer) {
    spdr_deinit(&gSpdrContext);
    delete gSpdrBuffer;
    gSpdrBuffer = nullptr;
    gSpdrContext = nullptr;
  }
}

void setStarted(bool started) {
  gWasStarted = started;
  spdr_enable_trace(gSpdrContext, started ? 1 : 0);
}

bool started() {
  return gWasStarted;
}

void finishAndWriteToPath(const stdfs::path& pathPrefix) {
  const auto printLine = [](const char* pString, void* context) {
    *((std::ofstream*)context) << pString;
  };

  setStarted(false);

  time_t now;
  struct tm now_tm;
  time(&now);
#ifdef V1_OS_WIN
  _localtime64_s(&now_tm, &now);  // name clash: localtime_s is different on MSVC -.-
#else
  localtime_r(&now, &now_tm);
#endif

  constexpr const size_t kStrlen = 32;
  char suffix[kStrlen];
  ::strftime(suffix, kStrlen, "-%F-%H.%M.%S.json", &now_tm);

  auto path = pathPrefix;
  path += suffix;

  std::ofstream file;
  file.open(path, std::ios_base::out | std::ios_base::trunc);
  if(file.good()) spdr_report(gSpdrContext, SPDR_CHROME_REPORT, printLine, &file);
  file.close();

  spdr_reset(gSpdrContext);
}

void finishAndWriteToPathPrefix(const char* pPathPrefix) {
  finishAndWriteToPath(stdfs::path(pPathPrefix));
}

void finishAndWriteToTempFile(const char* pFilenamePrefix) {
  finishAndWriteToPath(stdfs::temp_directory_path() / stdfs::path(pFilenamePrefix));
}

Status status() {
  Status result;
  result.initialized = !!gSpdrBuffer;
  result.started = gWasStarted;

  if(result.initialized) {
    auto sizeInfo = spdr_capacity(gSpdrContext);
    result.capacity = sizeInfo.capacity;
    result.used = sizeInfo.count;
  } else
    result.capacity = result.used = 0U;

  return result;
}

namespace detail {

SPDR_Event_Arg toSpdrArg(const TraceArg& Arg) {
  switch(Arg.Type) {
  case TraceArg::kStaticString: return uu_spdr_arg_make_str(Arg.pKey, Arg.pDynamicString);
  case TraceArg::kInt64: return uu_spdr_arg_make_int(Arg.pKey, int(Arg.intNumber)); break;
  case TraceArg::kDouble: return uu_spdr_arg_make_double(Arg.pKey, Arg.fpNumber);
  }
  return {};  // invalid
}


TracingScope::TracingScope(const char* pCategory, const char* pName)
    : mpCategory(pCategory), mpName(pName) {
  UU_SPDR_TRACE(gSpdrContext, pCategory, pName, SPDR_BEGIN);
}

TracingScope::TracingScope(const char* pCategory, const char* pName, const TraceArg& arg0)
    : mpCategory(pCategory), mpName(pName) {
  UU_SPDR_TRACE1(gSpdrContext, pCategory, pName, SPDR_BEGIN, toSpdrArg(arg0));
}

TracingScope::TracingScope(
    const char* pCategory, const char* pName, const TraceArg& arg0, const TraceArg& arg1)
    : mpCategory(pCategory), mpName(pName) {
  UU_SPDR_TRACE2(gSpdrContext, pCategory, pName, SPDR_BEGIN, toSpdrArg(arg0), toSpdrArg(arg1));
}
TracingScope::TracingScope(const char* pCategory, const char* pName, const TraceArg& arg0,
    const TraceArg& arg1, const TraceArg& arg2)
    : mpCategory(pCategory), mpName(pName) {
  UU_SPDR_TRACE3(gSpdrContext, pCategory, pName, SPDR_BEGIN, toSpdrArg(arg0), toSpdrArg(arg1),
      toSpdrArg(arg2));
}
TracingScope::~TracingScope() {
  UU_SPDR_TRACE(gSpdrContext, mpCategory, mpName, SPDR_END);
}


void track_variable(const char* pCategory, const char* pName, const TraceArg& arg0) {
  UU_SPDR_TRACE1(gSpdrContext, pCategory, pName, SPDR_COUNTER, toSpdrArg(arg0));
}

void track_variable(
    const char* pCategory, const char* pName, const TraceArg& arg0, const TraceArg& arg1) {
  UU_SPDR_TRACE2(gSpdrContext, pCategory, pName, SPDR_COUNTER, toSpdrArg(arg0), toSpdrArg(arg1));
}

void track_variable(const char* pCategory, const char* pName, const TraceArg& arg0,
    const TraceArg& arg1, const TraceArg& arg2) {
  UU_SPDR_TRACE3(gSpdrContext, pCategory, pName, SPDR_COUNTER, toSpdrArg(arg0), toSpdrArg(arg1),
      toSpdrArg(arg2));
}


void begin_async_event(const char* pCategory, const char* pName, int64_t id) {
  UU_SPDR_TRACE1(gSpdrContext, pCategory, pName, SPDR_ASYNC_EVENT_BEGIN, SPDR_INT("id", id));
}

void begin_async_event(const char* pCategory, const char* pName, int64_t id, const TraceArg& arg0) {
  UU_SPDR_TRACE2(
      gSpdrContext, pCategory, pName, SPDR_ASYNC_EVENT_BEGIN, SPDR_INT("id", id), toSpdrArg(arg0));
}

void begin_async_event(const char* pCategory, const char* pName, int64_t id, const TraceArg& arg0,
    const TraceArg& arg1) {
  UU_SPDR_TRACE3(gSpdrContext, pCategory, pName, SPDR_ASYNC_EVENT_BEGIN, SPDR_INT("id", id),
      toSpdrArg(arg0), toSpdrArg(arg1));
}

void end_async_event(const char* pCategory, const char* pName, int64_t id) {
  UU_SPDR_TRACE1(gSpdrContext, pCategory, pName, SPDR_ASYNC_EVENT_END, SPDR_INT("id", id));
}

}  // namespace detail
}  // namespace v1util::tracing
