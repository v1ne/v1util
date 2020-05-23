
#include "filesystem.hpp"

#include <string>
#include <system_error>

namespace v1util {

namespace {
constexpr const auto kThisFile = "stl-plus/filesystem.cpp";
constexpr const auto kTestFilesDir = "test-files";
}  // namespace

stdfs::path repoPath() {
  static auto p = []() {
    std::error_code ec;
    stdfs::path path = stdfs::current_path(ec);
    if(ec) return stdfs::path{};

    stdfs::path oldPath;
    while(oldPath != path && !(stdfs::exists(kThisFile, ec) && !ec)) {
      if(stdfs::exists((path / "v1util" / kThisFile), ec) && !ec) return path / "v1util";
      if(stdfs::exists((path / "third-party/v1util" / kThisFile), ec) && !ec)
        return path / "v1util";
      oldPath = path;
      path = path.parent_path();
    }

    return path == oldPath ? stdfs::path{} : path;
  }();

  return p;
}

stdfs::path testFilesPath() {
  static auto p = []() {
    auto path = repoPath();
    return path.empty() ? path : path / kTestFilesDir;
  }();

  return p;
}

stdfs::path unique_path(stdfs::path prefix, std::string_view suffix) {
  if(prefix.empty()) return {};

  for(int count = 0; count < 1'000'000; ++count) {
    auto path = prefix;
    if(count == 0)
      path.concat(suffix);
    else
      path.concat("-" + (std::to_string(count)) + (!suffix.empty() && suffix[0] != '.' ? "-" : ""))
          .concat(suffix);
    std::error_code ec;
    if(!exists(path, ec) && !ec) return path;
  }

  return {};
}

}  // namespace v1util
